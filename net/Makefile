# SPDX-License-Identifier: GPL-2.0
#
# MiLin: минимальный net (M5) — только транспорт AF_UNIX.
#
# socket.o   — syscall-слой сокетов (sockfs)
# core/      — ядро сети: skbuff, scm (SCM_RIGHTS), sock, dev-каркас
# ethernet/  — eth_type_trans/alloc_etherdev_mqs (нужны dev.o и loopback)
# ethtool/   — ioctl.o+common.o (obj-y у v7.2, ссылки из dev.o/sock.o)
# ipv6/      — только obj-y ядро (__ipv6_addr_type для core/utils.o);
#              сам IPv6 (CONFIG_IPV6) остаётся выключенным
# netlink/   — rtnetlink без него не линкуется (af_netlink + genetlink)
# sched/     — sch_generic/sch_mq: noqueue_qdisc_ops нужен register_netdevice
# unix/      — сам AF_UNIX

obj-y                           := devres.o socket.o core/

obj-$(CONFIG_SYSCTL)            += sysctl_net.o

obj-y                           += ethernet/ ethtool/ ipv6/
obj-$(CONFIG_BPF_SYSCALL)       += bpf/
obj-y                           += netlink/
obj-y                           += sched/

obj-$(CONFIG_UNIX)              += unix/
