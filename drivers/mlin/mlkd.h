#ifndef _MLKD_H
#define _MLKD_H

#include <linux/completion.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/wait.h>

#define MLKD_REQ_MAX            4096
#define MLKD_RESP_MAX           4096
#define MLKD_MAX_DRIVERS        16

enum {
        MLKD_PL_AUTO = 0,
        MLKD_PL_KERNEL,
        MLKD_PL_USER,
};

enum {
        MLKD_RS_KERNEL = 0,
        MLKD_RS_USER,
};

struct mlkd;

struct mlkd_ops {
        int (*probe)(struct mlkd *kd);
        long (*request)(struct mlkd *kd, const u8 *in, u32 inlen,
                        u8 *out, u32 outmax);
};

struct mlkd {
        const char              *name;
        int                     placement;      /* MLKD_PL_* */
        struct mlkd_ops         ops;
        void                    *priv;

        int                     resolved;       /* MLKD_RS_* */
        unsigned long           requests;
        unsigned long           restarts;
        unsigned long           fail_until;
        long                    probe_fail;
        struct task_struct      *task;
        struct mutex            tm;
        spinlock_t              lock;
        wait_queue_head_t       rq;
        struct list_head        pend;
        struct list_head        list;
        struct cdev             cdev;
        dev_t                   devt;
        struct device           *sysdev;
};

int mlkd_register(struct mlkd *kd);

#endif /* _MLKD_H */
