# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel device drivers.
#
# 15 Sep 2000, Christoph Hellwig <hch@infradead.org>
# Rewritten to use lists instead of if-statements.
#

obj-y                   += net/
obj-$(CONFIG_BLOCK)     += block/
obj-$(CONFIG_NVME_CORE) += nvme/
obj-$(CONFIG_SCSI)      += scsi/
obj-$(CONFIG_ATA)       += ata/
obj-$(CONFIG_INPUT)     += input/
obj-$(CONFIG_SERIO)     += input/serio/
obj-$(CONFIG_INPUT) +=
obj-y                   += clocksource/
obj-$(CONFIG_RTC_LIB)           += rtc/


# GPIO must come after pinctrl as gpios may need to mux pins etc
obj-$(CONFIG_PINCTRL) +=
obj-$(CONFIG_GPIOLIB) +=

# LEDs must come before PCI, it is needed by NPEM driver
obj-y                   += pci/

obj-$(CONFIG_PARISC) +=
obj-$(CONFIG_RAPIDIO) +=
obj-y                   += video/

# IPMI must come before ACPI in order to provide IPMI opregion support
obj-y                           += char/ipmi/

obj-$(CONFIG_ACPI) +=

# PnP must come after ACPI since it will eventually need to check if acpi
# was used and do nothing if so
obj-$(CONFIG_PNP) +=

# Many drivers will want to use DMA so this has to be made available
# really early.
obj-$(CONFIG_DMADEVICES) +=

# SOC specific infrastructure drivers.
obj-$(CONFIG_PM_GENERIC_DOMAINS) +=

obj-y                           += virtio/
obj-$(CONFIG_VDPA) +=
obj-$(CONFIG_XEN) +=

# regulators early, since some subsystems rely on them to initialize
obj-$(CONFIG_REGULATOR) +=

# reset controllers early, since gpu drivers might rely on them to initialize
obj-$(CONFIG_RESET_CONTROLLER) +=

# tty/ comes before char/ so that the VT console is the boot-time
# default.
obj-y                           += tty/
obj-y                           += char/

# MiLin (M10): gpu/i2c/media/power/hwmon восстановлены из v7.2 для kdrv-сборки
obj-y                           += i2c/ media/
obj-y                           += power/
obj-$(CONFIG_HWMON)             += hwmon/

# iommu/ comes before gpu as gpu are using iommu controllers

# gpu/ comes after char for AGP vs DRM startup and after iommu
obj-y                           += gpu/

obj-$(CONFIG_CONNECTOR) +=

# i810fb depends on char/agp/
obj-$(CONFIG_FB_I810) +=

obj-$(CONFIG_PARPORT) +=
obj-y += base/
obj-$(CONFIG_MILIN_KDRIVER) += mlin/
obj-$(CONFIG_LIBNVDIMM) +=
obj-$(CONFIG_DMA_SHARED_BUFFER) += dma-buf/
obj-$(CONFIG_NUBUS) +=
obj-$(CONFIG_ATA) +=
obj-$(CONFIG_TARGET_CORE) +=
obj-$(CONFIG_MTD) +=
obj-$(CONFIG_SPI) +=
obj-$(CONFIG_SPMI) +=
obj-$(CONFIG_HSI) +=
obj-$(CONFIG_SLIMBUS) +=
obj-$(CONFIG_ATM) +=
obj-$(CONFIG_FUSION) +=
obj-$(CONFIG_UIO)               += uio/
obj-$(CONFIG_VFIO) +=
obj-$(CONFIG_PCCARD) +=
obj-$(CONFIG_DIO) +=
obj-$(CONFIG_SBUS) +=
obj-$(CONFIG_ZORRO) +=
obj-$(CONFIG_ATA_OVER_ETH) +=
obj-$(CONFIG_TC) +=
obj-$(CONFIG_USB_PHY) +=
obj-$(CONFIG_USB) +=
obj-$(CONFIG_USB_SUPPORT) +=
obj-$(CONFIG_PCI) +=
obj-$(CONFIG_USB_GADGET) +=
obj-$(CONFIG_OF) +=
obj-$(CONFIG_SERIO) +=
obj-$(CONFIG_GAMEPORT) +=
obj-$(CONFIG_INPUT) +=
obj-$(CONFIG_RTC_LIB) +=
obj-$(CONFIG_PPS) +=
obj-$(CONFIG_W1) +=
obj-$(CONFIG_HWMON) +=
obj-$(CONFIG_THERMAL) +=
obj-$(CONFIG_WATCHDOG) +=
obj-$(CONFIG_MD) +=
obj-$(CONFIG_BT) +=
obj-$(CONFIG_ACCESSIBILITY) +=
obj-$(CONFIG_EDAC) +=
obj-$(CONFIG_EISA) +=
obj-$(CONFIG_PM_OPP) +=
obj-$(CONFIG_CPU_FREQ) +=
obj-$(CONFIG_CPU_IDLE) +=
obj-$(CONFIG_MEMSTICK) +=
obj-$(CONFIG_INFINIBAND) +=
obj-y                           += firmware/
obj-$(CONFIG_FWCTL) +=
obj-$(CONFIG_CRYPTO) +=
obj-$(CONFIG_SUPERH) +=
obj-$(CONFIG_DCA) +=
obj-$(CONFIG_HID_SUPPORT) +=
obj-$(CONFIG_PPC_PS3) +=
obj-$(CONFIG_OF) +=
obj-$(CONFIG_SSB) +=
obj-$(CONFIG_BCMA) +=
obj-$(CONFIG_VHOST_RING) +=
obj-$(CONFIG_VHOST_IOTLB) +=
obj-$(CONFIG_VHOST) +=
obj-$(CONFIG_GREYBUS) +=
obj-$(CONFIG_COMEDI) +=
obj-$(CONFIG_GPIB) +=
obj-$(CONFIG_STAGING) +=

obj-$(CONFIG_MAILBOX) +=
obj-$(CONFIG_HWSPINLOCK) +=
obj-$(CONFIG_REMOTEPROC) +=
obj-$(CONFIG_RPMSG) +=
obj-$(CONFIG_SOUNDWIRE) +=

# Virtualization drivers
obj-$(CONFIG_HYPERV) +=

obj-$(CONFIG_PM_DEVFREQ) +=
obj-$(CONFIG_EXTCON) +=
obj-$(CONFIG_MEMORY) +=
obj-$(CONFIG_IIO) +=
obj-$(CONFIG_IPACK_BUS) +=
obj-$(CONFIG_NTB) +=
obj-$(CONFIG_POWERCAP) +=
obj-$(CONFIG_MCB) +=
obj-$(CONFIG_PERF_EVENTS) +=
obj-$(CONFIG_RAS) +=
obj-$(CONFIG_USB4) +=
obj-$(CONFIG_CORESIGHT) +=
obj-$(CONFIG_STM) +=
obj-$(CONFIG_HISI_PTT) +=
obj-$(CONFIG_NVMEM) +=
obj-$(CONFIG_FPGA) +=
obj-$(CONFIG_FSI) +=
obj-$(CONFIG_TEE) +=
obj-$(CONFIG_MULTIPLEXER) +=
obj-$(CONFIG_SIOX) +=
obj-$(CONFIG_GNSS) +=
obj-$(CONFIG_INTERCONNECT) +=
obj-$(CONFIG_COUNTER) +=
obj-$(CONFIG_MOST) +=
obj-$(CONFIG_PECI) +=
obj-$(CONFIG_HTE) +=
obj-$(CONFIG_DRM_ACCEL) +=
obj-$(CONFIG_CDX_BUS) +=
obj-$(CONFIG_DPLL) +=

obj-$(CONFIG_DIBS) +=
obj-$(CONFIG_S390) +=
