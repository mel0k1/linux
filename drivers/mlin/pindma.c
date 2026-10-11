// SPDX-License-Identifier: GPL-2.0
/*
 * MiLin (M12): /dev/mlpinned — pinned-DMA регион для userspace-драйверов.
 *
 * Драйвер в userspace (netd поверх UIO) обязан дать NIC физические адреса
 * колец и буферов, а обычная память процесса физику не раскрывает. Открытие
 * устройства выделяет непрерывный регион (alloc_pages, порядок PINDMA_ORDER),
 * mmap() отдаёт его процессу, ioctl(MLPIO_GETPHYS) возвращает физический
 * адрес базы — кольца/буферы живут внутри региона, NIC читает их DMA'ом.
 * Закрытие fd — регион освобождён. Один регион на открытый fd.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define PINDMA_ORDER    10      /* 2^10 страниц = 4 МиБ непрерывно */
#define PINDMA_DEV      "mlpinned"

struct mlpin {
        struct page             *pages;
        u64                     phys;
        size_t                  size;
};

#define MLPIO_GETPHYS   _IOR('m', 1, u64)
#define MLPIO_GETSIZE   _IOR('m', 2, u64)

static dev_t pindma_devt;
static struct class *pindma_class;
static struct cdev pindma_cdev;

static int pindma_open(struct inode *ino, struct file *filp)
{
        struct mlpin *mp;

        mp = kzalloc(sizeof(*mp), GFP_KERNEL);
        if (!mp)
                return -ENOMEM;
        mp->size = (size_t)PAGE_SIZE << PINDMA_ORDER;
        mp->pages = alloc_pages(GFP_KERNEL | __GFP_ZERO | __GFP_COMP,
                                PINDMA_ORDER);
        if (!mp->pages) {
                kfree(mp);
                return -ENOMEM;
        }
        mp->phys = page_to_phys(mp->pages);
        filp->private_data = mp;
        pr_info("mlpinned: регион %zu KiB @ phys %llx\n",
                mp->size >> 10, mp->phys);
        return 0;
}

static int pindma_release(struct inode *ino, struct file *filp)
{
        struct mlpin *mp = filp->private_data;

        if (mp) {
                if (mp->pages)
                        __free_pages(mp->pages, PINDMA_ORDER);
                kfree(mp);
        }
        return 0;
}

static int pindma_mmap(struct file *filp, struct vm_area_struct *vma)
{
        struct mlpin *mp = filp->private_data;

        if (vma->vm_pgoff != 0)
                return -EINVAL;
        if (vma->vm_end - vma->vm_start > mp->size)
                return -EINVAL;
        return remap_pfn_range(vma, vma->vm_start,
                               page_to_pfn(mp->pages),
                               vma->vm_end - vma->vm_start,
                               vma->vm_page_prot);
}

static long pindma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        struct mlpin *mp = filp->private_data;
        u64 val;

        switch (cmd) {
        case MLPIO_GETPHYS:
                val = mp->phys;
                break;
        case MLPIO_GETSIZE:
                val = mp->size;
                break;
        default:
                return -ENOTTY;
        }
        return put_user(val, (u64 __user *)arg) ? -EFAULT : 0;
}

static const struct file_operations pindma_fops = {
        .owner          = THIS_MODULE,
        .open           = pindma_open,
        .release        = pindma_release,
        .mmap           = pindma_mmap,
        .unlocked_ioctl = pindma_ioctl,
};

static int __init pindma_init(void)
{
        int rc;

        rc = alloc_chrdev_region(&pindma_devt, 0, 1, PINDMA_DEV);
        if (rc)
                return rc;
        cdev_init(&pindma_cdev, &pindma_fops);
        rc = cdev_add(&pindma_cdev, pindma_devt, 1);
        if (rc)
                goto err_region;
        pindma_class = class_create(PINDMA_DEV);
        if (IS_ERR(pindma_class)) {
                rc = PTR_ERR(pindma_class);
                goto err_cdev;
        }
        device_create(pindma_class, NULL, pindma_devt, NULL, PINDMA_DEV);
        pr_info("mlpinned: pinned-DMA регион готов (major %d)\n",
                MAJOR(pindma_devt));
        return 0;

err_cdev:
        cdev_del(&pindma_cdev);
err_region:
        unregister_chrdev_region(pindma_devt, 1);
        return rc;
}
subsys_initcall(pindma_init);
