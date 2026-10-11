// SPDX-License-Identifier: GPL-2.0
/*
 * MiLin (M9): kdriver-домен — драйверы «между ядром и userspace».
 *
 * Каждый драйвер домена — отдельный kthread: свой pid, свой стек,
 * свой цикл запросов; при пробое домен перезапускает его (счётчик
 * restarts). Userspace общается с драйвером через /dev/<name>:
 * write() кладёт запрос, домен обрабатывает его в своём контексте,
 * read() забирает ответ. Размещение — политика: AUTO ставит драйвер
 * в домен ядра, 'user' через sysfs отдаёт его userspace-серверу
 * (UIO-путь), 'kernel' возвращает обратно; операции при 'user'
 * отклоняются с -ENODEV.
 */

#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "mlkd.h"

static dev_t mlkd_devt_base;
static struct class *mlkd_class;
static LIST_HEAD(mlkd_list);
static DEFINE_MUTEX(mlkd_reg_lock);
static int mlkd_count;

struct mlkd_job {
        struct list_head        list;
        struct completion       done;
        u32                     inlen;
        u32                     outlen;
        long                    rc;
        u8                      buf[];  /* [inlen][MLKD_RESP_MAX] */
};

struct mlkd_client {
        struct mlkd             *kd;
        struct mutex            m;
        u8                      resp[MLKD_RESP_MAX];
        u32                     resplen;
};

/* разобрать очередь: все необработанные запросы получают err */
static void mlkd_fail_pend(struct mlkd *kd, long err)
{
        struct mlkd_job *job;

        for (;;) {
                spin_lock_irq(&kd->lock);
                job = list_first_entry_or_null(&kd->pend, struct mlkd_job, list);
                if (job)
                        list_del(&job->list);
                spin_unlock_irq(&kd->lock);
                if (!job)
                        break;
                job->rc = err;
                job->outlen = 0;
                complete(&job->done);
        }
}

static int mlkd_thread(void *data)
{
        struct mlkd *kd = data;
        struct mlkd_job *job;
        long rc;

        rc = kd->ops.probe ? kd->ops.probe(kd) : 0;
        if (rc) {
                /* драйвер не завёлся: домен отпускает тред и держит backoff */
                mutex_lock(&kd->tm);
                kd->task = NULL;
                kd->probe_fail = rc;
                kd->fail_until = jiffies + 2 * HZ;
                kd->restarts++;
                mutex_unlock(&kd->tm);
                mlkd_fail_pend(kd, -EIO);
                return 0;
        }

        while (!kthread_should_stop()) {
                wait_event_interruptible(kd->rq, kthread_should_stop() ||
                                         !list_empty(&kd->pend));
                if (kthread_should_stop())
                        break;

                spin_lock_irq(&kd->lock);
                job = list_first_entry_or_null(&kd->pend, struct mlkd_job, list);
                if (job)
                        list_del(&job->list);
                spin_unlock_irq(&kd->lock);
                if (!job)
                        continue;

                rc = kd->ops.request(kd, job->buf, job->inlen,
                                     job->buf + job->inlen, MLKD_RESP_MAX);
                job->rc = rc;
                job->outlen = rc >= 0 ? (u32)min_t(long, rc, MLKD_RESP_MAX) : 0;
                kd->requests++;
                complete(&job->done);
        }

        mlkd_fail_pend(kd, -EIO);
        return 0;
}

/* tm должен удерживаться */
static long __mlkd_thread_start(struct mlkd *kd)
{
        struct task_struct *t;

        if (kd->task)
                return 0;
        /* драйвер недавно падал: не молотим по кругу */
        if (kd->probe_fail && time_is_before_jiffies(kd->fail_until))
                return -EAGAIN;

        t = kthread_create(mlkd_thread, kd, "mlkdd/%s", kd->name);
        if (IS_ERR(t))
                return PTR_ERR(t);

        kd->task = t;
        kd->probe_fail = 0;
        wake_up_process(t);
        return 0;
}

static long mlkd_thread_start(struct mlkd *kd)
{
        long rc;

        mutex_lock(&kd->tm);
        rc = __mlkd_thread_start(kd);
        mutex_unlock(&kd->tm);
        return rc;
}

static void mlkd_thread_stop(struct mlkd *kd)
{
        struct task_struct *t;

        mutex_lock(&kd->tm);
        t = kd->task;
        kd->task = NULL;
        mutex_unlock(&kd->tm);
        if (t)
                kthread_stop(t);
        mlkd_fail_pend(kd, -ENODEV);
}

static int mlkd_open(struct inode *ino, struct file *f)
{
        struct mlkd *kd = container_of(ino->i_cdev, struct mlkd, cdev);
        struct mlkd_client *cl;
        int rc;

        cl = kzalloc(sizeof(*cl), GFP_KERNEL);
        if (!cl)
                return -ENOMEM;
        cl->kd = kd;
        mutex_init(&cl->m);
        f->private_data = cl;
        rc = nonseekable_open(ino, f);
        if (rc) {
                kfree(cl);
                f->private_data = NULL;
        }
        return rc;
}

static int mlkd_release(struct inode *ino, struct file *f)
{
        kfree(f->private_data);
        return 0;
}

static ssize_t mlkd_write(struct file *f, const char __user *ub,
                          size_t len, loff_t *pp)
{
        struct mlkd_client *cl = f->private_data;
        struct mlkd *kd = cl->kd;
        struct mlkd_job *job;
        ssize_t rc;

        if (!len || len > MLKD_REQ_MAX)
                return len > MLKD_REQ_MAX ? -EMSGSIZE : -EINVAL;
        if (mutex_lock_interruptible(&cl->m))
                return -ERESTARTSYS;

        job = kzalloc(sizeof(*job) + len + MLKD_RESP_MAX, GFP_KERNEL);
        if (!job) {
                rc = -ENOMEM;
                goto out;
        }
        job->inlen = len;
        if (copy_from_user(job->buf, ub, len)) {
                rc = -EFAULT;
                goto out_free;
        }
        init_completion(&job->done);

        mutex_lock(&kd->tm);
        if (kd->resolved != MLKD_RS_KERNEL) {
                /* драйвер в userspace: домен его не обслуживает */
                mutex_unlock(&kd->tm);
                rc = -ENODEV;
                goto out_free;
        }
        if (!kd->task) {
                rc = __mlkd_thread_start(kd);
                if (rc) {
                        mutex_unlock(&kd->tm);
                        goto out_free;
                }
        }
        spin_lock_irq(&kd->lock);
        list_add_tail(&job->list, &kd->pend);
        spin_unlock_irq(&kd->lock);
        wake_up(&kd->rq);
        mutex_unlock(&kd->tm);

        wait_for_completion(&job->done);
        if (job->rc >= 0) {
                u32 n = min(job->outlen, (u32)MLKD_RESP_MAX);

                memcpy(cl->resp, job->buf + job->inlen, n);
                cl->resplen = n;
                rc = len;       /* запрос принят и обработан */
        } else {
                rc = job->rc;
        }
out_free:
        kfree(job);
out:
        mutex_unlock(&cl->m);
        return rc;
}

static ssize_t mlkd_read(struct file *f, char __user *ub,
                         size_t len, loff_t *pp)
{
        struct mlkd_client *cl = f->private_data;
        ssize_t rc;

        if (mutex_lock_interruptible(&cl->m))
                return -ERESTARTSYS;
        if (cl->resplen > len) {
                rc = -ETOOSMALL;
                goto out;
        }
        if (copy_to_user(ub, cl->resp, cl->resplen)) {
                rc = -EFAULT;
                goto out;
        }
        rc = cl->resplen;
        cl->resplen = 0;
out:
        mutex_unlock(&cl->m);
        return rc;
}

static const struct file_operations mlkd_fops = {
        .owner          = THIS_MODULE,
        .open           = mlkd_open,
        .release        = mlkd_release,
        .read           = mlkd_read,
        .write          = mlkd_write,
        .llseek         = noop_llseek,
};

/* --- sysfs: /sys/class/mlkd/<name>/{placement,state,requests,restarts} --- */

static ssize_t placement_show(struct device *dev, struct device_attribute *a,
                              char *buf)
{
        struct mlkd *kd = dev_get_drvdata(dev);

        return sysfs_emit(buf, "%s\n",
                          kd->placement == MLKD_PL_USER ? "user" :
                          kd->placement == MLKD_PL_KERNEL ? "kernel" : "auto");
}

static ssize_t placement_store(struct device *dev, struct device_attribute *a,
                               const char *buf, size_t count)
{
        struct mlkd *kd = dev_get_drvdata(dev);
        int pl;

        if (sysfs_streq(buf, "kernel"))
                pl = MLKD_PL_KERNEL;
        else if (sysfs_streq(buf, "user"))
                pl = MLKD_PL_USER;
        else if (sysfs_streq(buf, "auto"))
                pl = MLKD_PL_AUTO;
        else
                return -EINVAL;

        kd->placement = pl;
        mutex_lock(&kd->tm);
        kd->resolved = pl == MLKD_PL_USER ? MLKD_RS_USER : MLKD_RS_KERNEL;
        if (kd->resolved == MLKD_RS_KERNEL)
                __mlkd_thread_start(kd);
        mutex_unlock(&kd->tm);
        if (kd->resolved == MLKD_RS_USER)
                mlkd_thread_stop(kd);

        dev_info(dev, "mlkd: %s: размещение -> %s\n", kd->name,
                 kd->resolved == MLKD_RS_USER ? "user" : "kernel");
        return count;
}
static DEVICE_ATTR_RW(placement);

static ssize_t state_show(struct device *dev, struct device_attribute *a,
                          char *buf)
{
        struct mlkd *kd = dev_get_drvdata(dev);

        if (kd->resolved == MLKD_RS_USER)
                return sysfs_emit(buf, "user\n");
        return sysfs_emit(buf, kd->task ? "kernel\n" : "kernel-stopped\n");
}
static DEVICE_ATTR_RO(state);

static ssize_t requests_show(struct device *dev, struct device_attribute *a,
                             char *buf)
{
        struct mlkd *kd = dev_get_drvdata(dev);

        return sysfs_emit(buf, "%lu\n", kd->requests);
}
static DEVICE_ATTR_RO(requests);

static ssize_t restarts_show(struct device *dev, struct device_attribute *a,
                             char *buf)
{
        struct mlkd *kd = dev_get_drvdata(dev);

        return sysfs_emit(buf, "%lu\n", kd->restarts);
}
static DEVICE_ATTR_RO(restarts);

int mlkd_register(struct mlkd *kd)
{
        int rc;

        mutex_lock(&mlkd_reg_lock);
        if (mlkd_count >= MLKD_MAX_DRIVERS) {
                rc = -ENOSPC;
                goto out;
        }
        kd->resolved = kd->placement == MLKD_PL_USER ?
                       MLKD_RS_USER : MLKD_RS_KERNEL;
        mutex_init(&kd->tm);
        spin_lock_init(&kd->lock);
        init_waitqueue_head(&kd->rq);
        INIT_LIST_HEAD(&kd->pend);
        kd->devt = MKDEV(MAJOR(mlkd_devt_base), mlkd_count);

        cdev_init(&kd->cdev, &mlkd_fops);
        rc = cdev_add(&kd->cdev, kd->devt, 1);
        if (rc)
                goto out;
        kd->sysdev = device_create(mlkd_class, NULL, kd->devt, kd, "%s",
                                   kd->name);
        if (IS_ERR(kd->sysdev)) {
                rc = PTR_ERR(kd->sysdev);
                goto err_cdev;
        }
        rc = device_create_file(kd->sysdev, &dev_attr_placement) ||
             device_create_file(kd->sysdev, &dev_attr_state) ||
             device_create_file(kd->sysdev, &dev_attr_requests) ||
             device_create_file(kd->sysdev, &dev_attr_restarts);
        if (rc)
                goto err_dev;

        list_add_tail(&kd->list, &mlkd_list);
        mlkd_count++;
        mutex_unlock(&mlkd_reg_lock);

        if (kd->resolved == MLKD_RS_KERNEL)
                mlkd_thread_start(kd);
        pr_info("mlkd: %s зарегистрирован (%d:%d, placement=%s)\n",
                kd->name, MAJOR(kd->devt), MINOR(kd->devt),
                kd->resolved == MLKD_RS_KERNEL ? "kernel" : "user");
        return 0;

err_dev:
        device_destroy(mlkd_class, kd->devt);
err_cdev:
        cdev_del(&kd->cdev);
out:
        mutex_unlock(&mlkd_reg_lock);
        return rc;
}
EXPORT_SYMBOL_GPL(mlkd_register);

static int __init mlkd_init(void)
{
        int rc = alloc_chrdev_region(&mlkd_devt_base, 0, MLKD_MAX_DRIVERS,
                                     "mlkd");

        if (rc)
                return rc;
        mlkd_class = class_create("mlkd");
        if (IS_ERR(mlkd_class)) {
                rc = PTR_ERR(mlkd_class);
                unregister_chrdev_region(mlkd_devt_base, MLKD_MAX_DRIVERS);
                return rc;
        }
        pr_info("mlkd: домен драйверов готов (major %d)\n",
                MAJOR(mlkd_devt_base));
        return 0;
}
subsys_initcall(mlkd_init);
