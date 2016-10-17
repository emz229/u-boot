/*
 * Copyright (C) 2015 Google, Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * board/config.h - configuration options, board specific
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <configs/x86-common.h>

#define CONFIG_SYS_MONITOR_LEN		(1 << 20)
#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_ARCH_EARLY_INIT_R
#define CONFIG_ARCH_MISC_INIT

#define CONFIG_PCI_PNP
#define CONFIG_STD_DEVICES_SETTINGS	"stdin=serial,usbkbd\0" \
					"stdout=serial,vidconsole\0" \
					"stderr=serial,vidconsole\0"

#define CONFIG_SCSI_DEV_LIST		\
	{PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_VALLEYVIEW_SATA}, \
	{PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_VALLEYVIEW_SATA_ALT}

#define VIDEO_IO_OFFSET				0
#define CONFIG_X86EMU_RAW_IO

#define CONFIG_CMD_HASH

#undef CONFIG_ENV_SIZE
#define CONFIG_ENV_SIZE			0x8000

#undef CONFIG_ENV_IS_IN_SPI_FLASH
#define CONFIG_ENV_IS_NOWHERE
#define CONFIG_ENV_SECT_SIZE		0x1000
#define CONFIG_ENV_OFFSET		0x006ef000

#undef CONFIG_SYS_BOOTM_LEN
#define CONFIG_SYS_BOOTM_LEN		(64 << 20)

#undef CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS \
	"console=ttyS0,115200n8\0" \
	"optargs=\0" \
	"loadaddr=0x32000000\0" \
	"bootfile=fitImage-overlay-ima-initramfs-image-orionlx-plus.bin\0" \
	"altbootfile=alt-fitImage-overlay-ima-initramfs-image-orionlx-plus.bin\0" \
	"loadimage=setenv lowerdev /dev/disk/by-path/pci-0000:00:13.0-ata-1-part2; " \
		"ext4load scsi 0:1 ${loadaddr} ${bootfile}\0" \
	"altloadimage=setenv lowerdev /dev/disk/by-path/pci-0000:00:13.0-ata-1-part3; " \
		"ext4load scsi 0:1 ${loadaddr} ${altbootfile}\0" \
	"scsiargs=setenv bootargs console=${console} " \
		"lowerdev=${lowerdev} upperdev=/dev/disk/by-path/pci-0000:00:13.0-ata-1-part5 " \
		"video=vesafb vga=0x318 ima_tcb ima_appraise=enforce " \
		"${optargs}\0" \
	"scsiboot=echo Booting from scsi ...; " \
		"run scsiargs; " \
		"bootm ${loadaddr}\0"

#undef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND \
	"usb start;" \
	"if ext4load usb 0:1 $loadaddr fitImage-overlay-ima-initramfs-image-orionlx-plus.bin; then " \
		"setenv bootargs lowerdev=/dev/disk/by-label/usb.rootfs.ro " \
		"upperdev=/dev/disk/by-label/usb.rootfs.rw " \
		"video=vesafb vga=0x318 ima_tcb ima_appraise=enforce " \
		"console=ttyS0,115200n8;" \
		"bootm $loadaddr;" \
	"else;" \
		"if test -e scsi 0:1 bootalt.txt; then " \
			"if run altloadimage; then " \
				"run scsiboot;" \
			"else;" \
				"if run loadimage; then " \
					"run scsiboot;" \
				"fi;" \
			"fi;" \
		"else;" \
			"if run loadimage; then " \
				"run scsiboot;" \
			"else;" \
				"if run altloadimage; then " \
					"run scsiboot;" \
				"fi;" \
			"fi;" \
		"fi;" \
	"fi;"

#endif	/* __CONFIG_H */
