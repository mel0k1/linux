// SPDX-License-Identifier: GPL-2.0
/*
 * MiLin (M12): /dev/mlpinned — pinned-DMA регион для userspace-драйверов.
 *
 * Драйвер в userspace (netd поверх UIO) обязан дать NIC физические адреса
 * колец и буферов. Регион резервируется из e820 параметром memmap=
 * (memmap=4M$0x8000000): его физика — факт машины, а не арифметика
 * page_to_phys, которая в SPARSEMEM_VMEMMAP-сборке врёт. Ядро регион
 * не трогает: mmap отдаёт его процессу, ioctl возвращает физический
 * адрес базы. Закрытие fd — регион освобождён. Один регион на fd.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define PINDMA_PHYS     0x08000000ULL   /* memmap=4M$0x8000000 */
#define PINDMA_SIZE     (4u << 20)      /* 4 МиБ непрерывно */
#define PINDMA_DEV      "mlpinned"

#define MLPIO_GETPHYS   _IOR('m', 1, u64)
#define MLPIO_GETSIZE   _IOR('m', 2, u64)

static dev_t pindma_devt;
static struct class *pindma_class;
static struct cdev pindma_cdev;
static atomic_t pindma_users = ATOMIC_INIT(0);

static int pindma_open(struct inode *ino, struct file *filp)
{
        if (atomic_inc_return(&pindma_users) > 1) {
                atomic_dec(&pindma_users);
                return -EBUSY;          /* регион один — на одного клиента */
        }
        pr_info("mlpinned: регион %u KiB @ phys %llx\n",
                PINDMA_SIZE >> 10, PINDMA_PHYS);
        return 0;
}

static int pindma_release(struct inode *ino, struct file *filp)
{
        atomic_dec(&pindma_users);
        return 0;
}

static int pindma_mmap(struct file *filp, struct vm_area_struct *vma)
{
        unsigned long size = vma->vm_end - vma->vm_start;
        int rc;

        if (vma->vm_pgoff != 0)
                return -EINVAL;
        if (size > PINDMA_SIZE)
                return -EINVAL;
        vm_flags_set(vma, VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP);
        rc = remap_pfn_range(vma, vma->vm_start,
                             PINDMA_PHYS >> PAGE_SHIFT, size,
                             vma->vm_page_prot);
        if (rc)
                return rc;
        return 0;
}

static long pindma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        u64 val;

        switch (cmd) {
        case MLPIO_GETPHYS:
                val = PINDMA_PHYS;
                break;
        case MLPIO_GETSIZE:
                val = PINDMA_SIZE;
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
        pr_info("mlpinned: pinned-DMA регион готов (major %d, phys %llx)\n",
                MAJOR(pindma_devt), PINDMA_PHYS);
        return 0;

err_cdev:
        cdev_del(&pindma_cdev);
err_region:
        unregister_chrdev_region(pindma_devt, 1);
        return rc;
}
subsys_initcall(pindma_init);
