// SPDX-License-Identifier: GPL-2.0
/* MiLin: kdemo — образец драйвера домена; 'E' эхо, 'C' счётчик, 'I' инфо. */

#include <linux/kernel.h>
#include <linux/minmax.h>
#include <linux/sched.h>
#include <linux/unaligned.h>

#include "mlkd.h"

struct kdemo_state {
	u32 counter;
};

static long kdemo_request(struct mlkd *kd, const u8 *in, u32 inlen,
			  u8 *out, u32 outmax)
{
	struct kdemo_state *st = kd->priv;

	if (!inlen)
		return -EINVAL;

	switch (in[0]) {
	case 'E':
		if (inlen - 1 > outmax)
			return -EMSGSIZE;
		memcpy(out, in + 1, inlen - 1);
		return inlen - 1;
	case 'C':
		if (outmax < 4)
			return -EMSGSIZE;
		put_unaligned_le32(++st->counter, out);
		return 4;
	case 'I':
		return snprintf(out, outmax,
				"kdemo: домен ядра, kthread pid=%d, requests=%lu",
				current->pid, kd->requests);
	default:
		return -EOPNOTSUPP;
	}
}

static int kdemo_probe(struct mlkd *kd)
{
	pr_info("mlkd: kdemo стартовал в домене ядра (kthread pid %d)\n",
		current->pid);
	return 0;
}

static struct kdemo_state kdemo_state;
static struct mlkd kdemo = {
	.name		= "kdemo",
	.placement	= MLKD_PL_AUTO,
	.ops		= {
		.probe		= kdemo_probe,
		.request	= kdemo_request,
	},
	.priv		= &kdemo_state,
};

static int __init kdemo_init(void)
{
	return mlkd_register(&kdemo);
}
device_initcall(kdemo_init);
