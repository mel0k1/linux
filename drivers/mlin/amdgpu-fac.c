// SPDX-License-Identifier: GPL-2.0
/*
 * MiLin (M11): amdgpu-fac — фасад amdgpu в kdriver-домене.
 *
 * Запросный цикл GPU живёт в kthread домена (M9): /dev/amdgpu —
 * write() кладёт запрос [op][payload], read() забирает ответ. Клиенты
 * микроядра говорят на mlgpu: INFO/ALLOC/FREE/SUBMIT/WAIT.
 *
 * Размещение — политика (M9): при placement=kernel цикл здесь; при
 * 'user' домен отклоняет операции (-ENODEV), протокол обслуживает
 * userspace-демон gpud через lane шины "gpu.amdgpu" — тот же
 * wire-формат, отдельный state.
 *
 * Связь с amdgpu (M10): режим определяется лениво на первом INFO —
 * драйвер amdgpu регистрируется устройственно позже device_initcall.
 *   facade — amdgpu нет (diet-сборка): эмуляция пула VRAM и заборов;
 *   amdgpu — драйвер builtin (kdrv), устройства нет: запросная
 *           плоскость перед DRM, очереди/буферы фасада;
 *   hw     — найдено AMD-устройство: точка форвардинга в DRM/TTM
 *           (в QEMU AMD-железа нет, поэтому форвардинг не активен).
 */

#include <linux/kernel.h>
#include <linux/minmax.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/unaligned.h>

#include "mlgpu.h"
#include "mlkd.h"

#define FAC_SLOTS       8
#define FAC_SLOT_SIZE   (1u << 20)      /* 1 MiB на слот эмулируемой VRAM */
#define FAC_VRAM        ((u64)FAC_SLOTS * FAC_SLOT_SIZE)
#define FAC_HANDLE_MAGIC 0xfac00000ull
#define FAC_MAX_DW      64

struct fac_state {
        u32     used[FAC_SLOTS];
        u32     size[FAC_SLOTS];
        u64     fences;
        u64     allocs;
        int     mode;
        int     mode_done;
};

/* режим лениво: amdgpu регистрирует pci-драйвер позже нашего initcall */
static void fac_detect(struct fac_state *st)
{
        st->mode = MLGPU_MODE_FACADE;
#ifdef CONFIG_PCI
        if (driver_find("amdgpu", &pci_bus_type)) {
                st->mode = MLGPU_MODE_AMDGPU;
                if (pci_get_device(PCI_VENDOR_ID_ATI, PCI_ANY_ID, NULL))
                        st->mode = MLGPU_MODE_HW;
        }
#endif
        st->mode_done = 1;
}

/* хэндл: магия + номер слота, чтобы чужие u64 отсекались по значению */
static int fac_slot_of(u64 handle)
{
        u32 h = (u32)handle;

        if (handle >> 32 || (h & 0xffff0000) != FAC_HANDLE_MAGIC)
                return -1;
        h &= 0xffff;
        return h < FAC_SLOTS ? (int)h : -1;
}

static long fac_alloc(struct fac_state *st, const u8 *in, u32 inlen,
                      u8 *out, u32 outmax)
{
        struct mlgpu_alloc_in a;
        struct mlgpu_alloc_out o;

        if (inlen != sizeof(a) || outmax < sizeof(o))
                return -EMSGSIZE;
        memcpy(&a, in, sizeof(a));
        if (!a.size || a.size > FAC_SLOT_SIZE)
                return -EINVAL;

        for (int i = 0; i < FAC_SLOTS; i++) {
                if (st->used[i])
                        continue;
                st->used[i] = 1;
                st->size[i] = a.size;
                st->allocs++;
                o.handle = FAC_HANDLE_MAGIC | i;
                o.offset = (u64)i * FAC_SLOT_SIZE;
                memcpy(out, &o, sizeof(o));
                return sizeof(o);
        }
        return -ENOMEM;
}

static long fac_free(struct fac_state *st, const u8 *in, u32 inlen)
{
        struct mlgpu_free_in f;
        int slot;

        if (inlen != sizeof(f))
                return -EMSGSIZE;
        memcpy(&f, in, sizeof(f));
        slot = fac_slot_of(f.handle);
        if (slot < 0 || !st->used[slot])
                return -EINVAL;
        st->used[slot] = 0;
        return 0;
}

static long fac_submit(struct fac_state *st, const u8 *in, u32 inlen,
                       u8 *out, u32 outmax)
{
        struct mlgpu_submit_in s;
        struct mlgpu_submit_out o;
        u32 ndw, sum = 0;
        int slot;

        if (inlen < sizeof(s) || outmax < sizeof(o))
                return -EMSGSIZE;
        memcpy(&s, in, sizeof(s));
        ndw = min(s.ndw, FAC_MAX_DW);
        if (s.ndw > FAC_MAX_DW || inlen != sizeof(s) + ndw * 4)
                return -EMSGSIZE;
        slot = fac_slot_of(s.handle);
        if (slot < 0 || !st->used[slot])
                return -EINVAL;

        /* эмуляция: проход по командному буферу (в hw — foreach_ib в DRM) */
        for (u32 i = 0; i < ndw; i++)
                sum += get_unaligned_le32(in + sizeof(s) + i * 4);
        st->fences++;
        o.fence = st->fences;
        memcpy(out, &o, sizeof(o));
        return sizeof(o);
}

static long fac_wait(struct fac_state *st, const u8 *in, u32 inlen,
                     u8 *out, u32 outmax)
{
        struct mlgpu_wait_in w;
        struct mlgpu_wait_out o;

        if (inlen != sizeof(w) || outmax < sizeof(o))
                return -EMSGSIZE;
        memcpy(&w, in, sizeof(w));
        /* заборы завершаются синхронно с submit — эмуляция конвейера */
        o.signaled = w.fence && w.fence <= st->fences;
        o.pad = 0;
        memcpy(out, &o, sizeof(o));
        return sizeof(o);
}

static long fac_request(struct mlkd *kd, const u8 *in, u32 inlen,
                        u8 *out, u32 outmax)
{
        struct fac_state *st = kd->priv;

        if (inlen < 1)
                return -EINVAL;

        switch (in[0]) {
        case MLGPU_INFO: {
                struct mlgpu_info_out o;

                if (outmax < sizeof(o))
                        return -EMSGSIZE;
                if (!st->mode_done)
                        fac_detect(st);
                memset(&o, 0, sizeof(o));
                strscpy(o.name, "amdgpu", sizeof(o.name));
                o.api = MLGPU_API;
                o.mode = st->mode;
                o.nslots = FAC_SLOTS;
                o.vram_bytes = FAC_VRAM;
                o.fences = st->fences;
                o.allocs = st->allocs;
                memcpy(out, &o, sizeof(o));
                return sizeof(o);
        }
        case MLGPU_ALLOC:
                return fac_alloc(st, in + 1, inlen - 1, out, outmax);
        case MLGPU_FREE:
                return fac_free(st, in + 1, inlen - 1);
        case MLGPU_SUBMIT:
                return fac_submit(st, in + 1, inlen - 1, out, outmax);
        case MLGPU_WAIT:
                return fac_wait(st, in + 1, inlen - 1, out, outmax);
        default:
                return -EOPNOTSUPP;
        }
}

static int fac_probe(struct mlkd *kd)
{
        pr_info("mlkd: amdgpu-fac: запросная плоскость GPU в домене (pid %d), "
                "режим уточняется на первом INFO\n", current->pid);
        return 0;
}

static struct fac_state fac_state;
static struct mlkd amdgpu_fac = {
        .name           = "amdgpu",
        .placement      = MLKD_PL_AUTO,
        .ops            = {
                .probe          = fac_probe,
                .request        = fac_request,
        },
        .priv           = &fac_state,
};

static int __init amdgpu_fac_init(void)
{
        return mlkd_register(&amdgpu_fac);
}
device_initcall(amdgpu_fac_init);
