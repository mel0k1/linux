# SPDX-License-Identifier: GPL-2.0
# Makefile for the Linux sound card driver
#
# MiLin (M27): из upstream остались только core/, hda/ (common+controllers+
# codecs+core) и x86/ (SND_INTEL_DSP_CONFIG).

obj-$(CONFIG_SOUND) += soundcore.o
obj-$(CONFIG_SND) += core/ hda/ x86/

ifeq ($(CONFIG_SND),y)
  obj-y += last.o
endif

soundcore-y  := sound_core.o
