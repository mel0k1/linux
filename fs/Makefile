# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux filesystems.
#
# 14 Sep 2000, Christoph Hellwig <hch@infradead.org>
# Rewritten to use lists instead of if-statements.
# 


obj-y :=	open.o read_write.o file_table.o super.o \
		char_dev.o stat.o exec.o pipe.o namei.o fcntl.o \
		ioctl.o readdir.o select.o dcache.o inode.o \
		attr.o bad_inode.o file.o filesystems.o namespace.o \
		seq_file.o xattr.o libfs.o fs-writeback.o \
		pnode.o splice.o sync.o utimes.o d_path.o \
		stack.o fs_struct.o statfs.o fs_pin.o nsfs.o \
		fs_dirent.o fs_context.o fs_parser.o fsopen.o init.o \
		kernel_read_file.o mnt_idmapping.o remap_range.o pidfs.o \
		file_attr.o fserror.o nullfs.o

obj-$(CONFIG_BUFFER_HEAD)	+= buffer.o mpage.o
obj-$(CONFIG_PROC_FS)		+= proc_namespace.o
obj-$(CONFIG_LEGACY_DIRECT_IO)	+= direct-io.o
obj-y +=
obj-$(CONFIG_EPOLL)		+= eventpoll.o
obj-y				+= anon_inodes.o
obj-$(CONFIG_SIGNALFD)		+= signalfd.o
obj-$(CONFIG_TIMERFD)		+= timerfd.o
obj-$(CONFIG_EVENTFD)		+= eventfd.o
obj-$(CONFIG_AIO)               += aio.o
obj-$(CONFIG_FS_DAX)		+= dax.o
obj-$(CONFIG_FS_ENCRYPTION) +=
obj-$(CONFIG_FS_VERITY) +=
obj-$(CONFIG_FILE_LOCKING)      += locks.o
obj-$(CONFIG_BINFMT_MISC)	+= binfmt_misc.o
obj-$(CONFIG_BINFMT_SCRIPT)	+= binfmt_script.o
obj-$(CONFIG_BINFMT_ELF)	+= binfmt_elf.o
obj-$(CONFIG_COMPAT_BINFMT_ELF)	+= compat_binfmt_elf.o
obj-$(CONFIG_BINFMT_ELF_FDPIC)	+= binfmt_elf_fdpic.o
obj-$(CONFIG_BINFMT_FLAT)	+= binfmt_flat.o

obj-$(CONFIG_FS_STACK)		+= backing-file.o
obj-$(CONFIG_FS_MBCACHE)	+= mbcache.o
obj-$(CONFIG_FS_POSIX_ACL)	+= posix_acl.o
obj-$(CONFIG_NFS_COMMON) +=
obj-$(CONFIG_COREDUMP)		+= coredump.o
obj-$(CONFIG_SYSCTL)		+= drop_caches.o sysctls.o

obj-$(CONFIG_FHANDLE)		+= fhandle.o
obj-y +=

obj-y +=

obj-$(CONFIG_PROC_FS) +=
obj-$(CONFIG_KERNFS) +=
obj-$(CONFIG_SYSFS) +=
obj-$(CONFIG_CONFIGFS_FS) +=
obj-y +=

obj-$(CONFIG_DLM) +=
 
# Do not add any filesystems before this line
obj-$(CONFIG_NETFS_SUPPORT) +=
obj-$(CONFIG_EXT4_FS) +=
# We place ext4 before ext2 so that clean ext3 root fs's do NOT mount using the
# ext2 driver, which doesn't know about journalling!  Explicitly request ext2
# by giving the rootfstype= parameter.
obj-$(CONFIG_EXT2_FS) +=
obj-$(CONFIG_JBD2) +=
obj-$(CONFIG_CRAMFS) +=
obj-$(CONFIG_SQUASHFS) +=
obj-y				+= ramfs/
obj-$(CONFIG_HUGETLBFS) +=
obj-$(CONFIG_CODA_FS) +=
obj-$(CONFIG_MINIX_FS) +=
obj-$(CONFIG_FAT_FS) +=
obj-$(CONFIG_EXFAT_FS) +=
obj-$(CONFIG_BFS_FS) +=
obj-$(CONFIG_ISO9660_FS) +=
obj-$(CONFIG_HFSPLUS_FS) += # Before hfs to find wrapped HFS+
obj-$(CONFIG_HFS_FS) +=
obj-$(CONFIG_ECRYPT_FS) +=
obj-$(CONFIG_VXFS_FS) +=
obj-$(CONFIG_NFS_FS) +=
obj-$(CONFIG_EXPORTFS) +=
obj-$(CONFIG_NFSD) +=
obj-$(CONFIG_LOCKD) +=
obj-$(CONFIG_NLS) +=
obj-y +=
obj-$(CONFIG_SMBFS) +=
obj-$(CONFIG_HPFS_FS) +=
obj-$(CONFIG_NTFS_FS) +=
obj-$(CONFIG_NTFS3_FS) +=
obj-$(CONFIG_UFS_FS) +=
obj-$(CONFIG_EFS_FS) +=
obj-$(CONFIG_JFFS2_FS) +=
obj-$(CONFIG_UBIFS_FS) +=
obj-$(CONFIG_AFFS_FS) +=
obj-$(CONFIG_ROMFS_FS) +=
obj-$(CONFIG_QNX4FS_FS) +=
obj-$(CONFIG_QNX6FS_FS) +=
obj-$(CONFIG_AUTOFS_FS) +=
obj-$(CONFIG_ADFS_FS) +=
obj-$(CONFIG_FUSE_FS)		+= fuse/
obj-$(CONFIG_OVERLAY_FS) +=
obj-$(CONFIG_ORANGEFS_FS) +=
obj-$(CONFIG_UDF_FS) +=
obj-$(CONFIG_SUN_OPENPROMFS) +=
obj-$(CONFIG_OMFS_FS) +=
obj-$(CONFIG_JFS_FS) +=
obj-$(CONFIG_XFS_FS) +=
obj-$(CONFIG_9P_FS) +=
obj-$(CONFIG_AFS_FS) +=
obj-$(CONFIG_NILFS2_FS) +=
obj-$(CONFIG_BEFS_FS) +=
obj-y +=
obj-$(CONFIG_CACHEFILES) +=
obj-$(CONFIG_DEBUG_FS) +=
obj-$(CONFIG_TRACING) +=
obj-$(CONFIG_OCFS2_FS) +=
obj-$(CONFIG_BTRFS_FS) +=
obj-$(CONFIG_GFS2_FS) +=
obj-$(CONFIG_F2FS_FS) +=
obj-$(CONFIG_CEPH_FS) +=
obj-$(CONFIG_PSTORE) +=
obj-$(CONFIG_EFIVAR_FS) +=
obj-$(CONFIG_EROFS_FS) +=
obj-$(CONFIG_VBOXSF_FS) +=
obj-$(CONFIG_ZONEFS_FS) +=
obj-$(CONFIG_BPF_LSM)		+= bpf_fs_kfuncs.o
obj-$(CONFIG_RESCTRL_FS) +=
