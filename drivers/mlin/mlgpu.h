/* SPDX-License-Identifier: GPL-2.0 */
/* MiLin mlgpu: wire-протокол фасада amdgpu (M11).
 * Зеркало в MiLin/userspace/mlgpu.h — менять синхронно.
 * Формат запроса: [ u8 op ][ payload ], ответ — чистый payload.
 */
#ifndef _MLGPU_H
#define _MLGPU_H

#define MLGPU_API		1

/* op-коды */
enum {
	MLGPU_INFO	= 'I',
	MLGPU_ALLOC	= 'A',
	MLGPU_FREE	= 'F',
	MLGPU_SUBMIT	= 'S',
	MLGPU_WAIT	= 'W',
};

/* режим фасада: связь с amdgpu (M10) */
enum {
	MLGPU_MODE_FACADE	= 0,	/* amdgpu нет: эмуляция */
	MLGPU_MODE_AMDGPU	= 1,	/* драйвер amdgpu в ядре, устройства нет */
	MLGPU_MODE_HW		= 2,	/* AMD-устройство найдено (форвардинг в DRM) */
};

struct mlgpu_info_out {
	char	name[16];
	u32	api;
	u32	mode;
	u32	nslots;
	u32	pad;
	u64	vram_bytes;
	u64	fences;
	u64	allocs;
};

struct mlgpu_alloc_in {
	u64	size;
};

struct mlgpu_alloc_out {
	u64	handle;
	u64	offset;
};

struct mlgpu_free_in {
	u64	handle;
};

struct mlgpu_submit_in {
	u64	handle;
	u32	ndw;
	u32	pad;
	/* u32 cmd[ndw] — сразу за структурой */
};

struct mlgpu_submit_out {
	u64	fence;
};

struct mlgpu_wait_in {
	u64	fence;
};

struct mlgpu_wait_out {
	u32	signaled;
	u32	pad;
};

#endif /* _MLGPU_H */
