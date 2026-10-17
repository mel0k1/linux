// SPDX-License-Identifier: GPL-2.0
/* MiLin: /dev/mltap — фреймовый мост netdev<->userspace (netd поверх kernel-драйверов: wlan/eth). */

#include <linux/cdev.h>
#include <linux/etherdevice.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/poll.h>
#include <linux/rtnetlink.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#define MLTAP_DEV       "mltap"
#define MLTAP_RING      256
#define MLTAP_FRAME     2048

#define MLTPIO_BIND     _IOW('m', 0x10, struct mltap_bind)
#define MLTPIO_UP       _IO('m', 0x11)
#define MLTPIO_MAC      _IOR('m', 0x12, struct mltap_mac)

struct mltap_bind {
        char ifname[16];
};

struct mltap_mac {
        u8 mac[6];
};

struct mltap_slot {
        u32 len;
        u8 data[MLTAP_FRAME];
};

struct mltap_bound {
        struct net_device *dev;
        struct packet_type pt;
        struct mltap_slot *ring;
        u32 head, tail;
        u32 dropped;
        spinlock_t lock;
        wait_queue_head_t wq;
        bool up;
};

static dev_t mltap_devt;
static struct class *mltap_class;
static struct cdev mltap_cdev;

static int mltap_rx(struct sk_buff *skb, struct net_device *dev,
                    struct packet_type *pt, struct net_device *orig_dev)
{
        struct mltap_bound *b = container_of(pt, struct mltap_bound, pt);
        struct mltap_slot *s;
        unsigned long flags;
        u32 flen;
        int mo = 0;

        if (skb->pkt_type == PACKET_OUTGOING)
                return NET_RX_SUCCESS;          // свои TX-клоны обратно не отдаём
        if (skb_mac_header_was_set(skb)) {
                mo = (int)(skb_mac_header(skb) - skb->data);
                flen = skb->len - mo;
        } else {
                flen = skb->len;
        }
        if (flen == 0 || flen > MLTAP_FRAME) {
                b->dropped++;
                return NET_RX_SUCCESS;
        }
        spin_lock_irqsave(&b->lock, flags);
        if ((b->head + 1) % MLTAP_RING == b->tail) {
                b->dropped++;
                spin_unlock_irqrestore(&b->lock, flags);
                return NET_RX_SUCCESS;
        }
        s = &b->ring[b->head];
        skb_copy_bits(skb, mo, s->data, flen);
        s->len = flen;
        b->head = (b->head + 1) % MLTAP_RING;
        spin_unlock_irqrestore(&b->lock, flags);
        wake_up_interruptible(&b->wq);
        return NET_RX_SUCCESS;
}

static int mltap_open(struct inode *ino, struct file *filp)
{
        filp->private_data = NULL;
        return 0;
}

static int mltap_release(struct inode *ino, struct file *filp)
{
        struct mltap_bound *b = filp->private_data;

        if (!b)
                return 0;
        dev_remove_pack(&b->pt);
        if (b->up)
                dev_change_flags(b->dev, b->dev->flags & ~IFF_UP, NULL);
        dev_put(b->dev);
        kvfree(b->ring);
        kfree(b);
        return 0;
}

static long mltap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        struct mltap_bind ib;
        struct mltap_bound *b;
        int rc;

        switch (cmd) {
        case MLTPIO_BIND:
                if (filp->private_data)
                        return -EBUSY;
                if (copy_from_user(&ib, (void __user *)arg, sizeof(ib)))
                        return -EFAULT;
                ib.ifname[sizeof(ib.ifname) - 1] = 0;
                b = kzalloc(sizeof(*b), GFP_KERNEL);
                if (!b)
                        return -ENOMEM;
                b->dev = dev_get_by_name(&init_net, ib.ifname);
                if (!b->dev) {
                        kfree(b);
                        return -ENODEV;
                }
                b->ring = kvmalloc(sizeof(*b->ring) * MLTAP_RING, GFP_KERNEL);
                if (!b->ring) {
                        dev_put(b->dev);
                        kfree(b);
                        return -ENOMEM;
                }
                spin_lock_init(&b->lock);
                init_waitqueue_head(&b->wq);
                b->pt.type = htons(ETH_P_ALL);
                b->pt.dev = b->dev;
                b->pt.func = mltap_rx;
                dev_add_pack(&b->pt);
                filp->private_data = b;
                pr_info("mltap: bound %s\n", ib.ifname);
                return 0;
        case MLTPIO_UP:
                b = filp->private_data;
                if (!b)
                        return -EINVAL;
                rtnl_lock();
                rc = dev_change_flags(b->dev, b->dev->flags | IFF_UP, NULL);
                rtnl_unlock();
                if (!rc)
                        b->up = true;
                return rc;
        case MLTPIO_MAC:
                b = filp->private_data;
                if (!b)
                        return -EINVAL;
                return copy_to_user((void __user *)arg, b->dev->dev_addr,
                                    sizeof(struct mltap_mac)) ? -EFAULT : 0;
        default:
                return -ENOTTY;
        }
}

static ssize_t mltap_write(struct file *filp, const char __user *ubuf,
                           size_t len, loff_t *ppos)
{
        struct mltap_bound *b = filp->private_data;
        struct sk_buff *skb;
        int rc;

        if (!b || len == 0 || len > MLTAP_FRAME)
                return -EINVAL;
        if (!netif_running(b->dev))
                return -ENETDOWN;
        skb = alloc_skb(len + NET_IP_ALIGN, GFP_KERNEL);
        if (!skb)
                return -ENOMEM;
        skb_reserve(skb, NET_IP_ALIGN);
        if (copy_from_user(skb_put(skb, len), ubuf, len)) {
                kfree_skb(skb);
                return -EFAULT;
        }
        skb->dev = b->dev;
        rc = dev_queue_xmit(skb);
        return rc >= 0 ? len : -EIO;
}

static ssize_t mltap_read(struct file *filp, char __user *ubuf,
                          size_t len, loff_t *ppos)
{
        struct mltap_bound *b = filp->private_data;
        struct mltap_slot *s;
        unsigned long flags;
        u32 t;
        int rc;

        if (!b)
                return -EINVAL;
        if (len < MLTAP_FRAME)
                return -EINVAL;
        if (filp->f_flags & O_NONBLOCK) {
                spin_lock_irqsave(&b->lock, flags);
                if (b->tail == b->head) {
                        spin_unlock_irqrestore(&b->lock, flags);
                        return -EAGAIN;
                }
                t = b->tail;
                spin_unlock_irqrestore(&b->lock, flags);
        } else {
                rc = wait_event_interruptible(b->wq, b->tail != b->head);
                if (rc)
                        return rc;
                spin_lock_irqsave(&b->lock, flags);
                t = b->tail;
                spin_unlock_irqrestore(&b->lock, flags);
        }
        s = &b->ring[t];
        if (copy_to_user(ubuf, s->data, s->len))
                return -EFAULT;
        spin_lock_irqsave(&b->lock, flags);
        b->tail = (t + 1) % MLTAP_RING;
        spin_unlock_irqrestore(&b->lock, flags);
        return s->len;
}

static __poll_t mltap_poll(struct file *filp, poll_table *wait)
{
        struct mltap_bound *b = filp->private_data;
        __poll_t mask = POLLOUT | POLLWRNORM;

        if (!b)
                return POLLERR;
        poll_wait(filp, &b->wq, wait);
        if (b->tail != b->head)
                mask |= POLLIN | POLLRDNORM;
        return mask;
}

static const struct file_operations mltap_fops = {
        .owner          = THIS_MODULE,
        .open           = mltap_open,
        .release        = mltap_release,
        .unlocked_ioctl = mltap_ioctl,
        .write          = mltap_write,
        .read           = mltap_read,
        .poll           = mltap_poll,
        .llseek         = noop_llseek,
};

static int __init mltap_init(void)
{
        int rc;

        rc = alloc_chrdev_region(&mltap_devt, 0, 1, MLTAP_DEV);
        if (rc)
                return rc;
        cdev_init(&mltap_cdev, &mltap_fops);
        rc = cdev_add(&mltap_cdev, mltap_devt, 1);
        if (rc)
                goto err;
        mltap_class = class_create(MLTAP_DEV);
        if (IS_ERR(mltap_class)) {
                rc = PTR_ERR(mltap_class);
                goto err_cdev;
        }
        device_create(mltap_class, NULL, mltap_devt, NULL, MLTAP_DEV);
        pr_info("mltap: готов\n");
        return 0;

err_cdev:
        cdev_del(&mltap_cdev);
err:
        unregister_chrdev_region(mltap_devt, 1);
        return rc;
}

static void __exit mltap_exit(void)
{
        device_destroy(mltap_class, mltap_devt);
        class_destroy(mltap_class);
        cdev_del(&mltap_cdev);
        unregister_chrdev_region(mltap_devt, 1);
}

module_init(mltap_init);
module_exit(mltap_exit);
MODULE_LICENSE("GPL");
