/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2015 Google, Inc
 */

/*
 * board/config.h - configuration options, board specific
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <configs/x86-common.h>

#define CONFIG_BOARD_LATE_INIT

#define CONFIG_SYS_MONITOR_LEN		(1 << 20)

#define CONFIG_STD_DEVICES_SETTINGS	"stdin=serial,usbkbd\0" \
					"stdout=serial,vidconsole\0" \
					"stderr=serial,vidconsole\0"

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
	CONFIG_STD_DEVICES_SETTINGS \
	"optargs=\0" \
	"loadaddr=0x32000000\0" \
	"initrd_high=0xffffffff\0" \
	"bootfile=fitImage-overlay-ima-initramfs-image-orionlx-plus.bin\0" \
	"fsfile=orion-graphical-image-orionlx-plus.fsfit\0" \
	"altbootfile=alt-fitImage-overlay-ima-initramfs-image-orionlx-plus.bin\0" \
	"altfsfile=alt-orion-graphical-image-orionlx-plus.fsfit\0" \
	"usemain=setenv bootmediatype scsi;" \
		"setenv bootpart part2;" \
		"setenv usebootfile ${bootfile};" \
		"setenv usefsfile ${fsfile};\0" \
	"usealt=setenv bootmediatype scsi;" \
		"setenv bootpart part3;" \
		"setenv usebootfile ${altbootfile};" \
		"setenv usefsfile ${altfsfile};\0" \
	"loadimage=" \
		"if ext4load ${bootmediatype} 0:1 ${loadaddr} ${usefsfile}; then " \
			"echo loaded ${usefsfile};" \
			"if source ${loadaddr}:script; then " \
				"echo sourced ${usefsfile};" \
				"if ext4load ${bootmediatype} 0:1 ${loadaddr} ${usebootfile}; then " \
					"echo loaded ${usebootfile};" \
					"if source ${loadaddr}:script; then " \
						"echo sourced ${usebootfile};" \
						"if test \"${fs_version}\" = \"${kernel_version}\"; then " \
							"echo filesystem version matches kernel version.;" \
							"true;" \
						"else;" \
							"echo filesystem version doesnt match kernel version!;" \
							"false;" \
						"fi;" \
					"else;" \
						"echo sourcing ${usebootfile} failed;" \
						"false;" \
					"fi;" \
				"else;" \
					"echo loading ${usebootfile} failed;" \
					"false;" \
				"fi;" \
			"else;" \
				"echo sourcing ${usefsfile} failed;" \
				"false;" \
			"fi;" \
		"else;" \
			"echo loading ${usefsfile} failed;" \
			"false;" \
		"fi;\0" \
	"scsiboot=echo Booting from scsi ...; " \
		"setenv bootargs lowerdev=${scsibootdisk}-${bootpart} " \
		"upperdev=${scsibootdisk}-part4 " \
		"u-boot-version=\\\\\"\"${ver}\"\\\\\" " \
		"${scsiconfigargs} " \
		"fs_sha1sum=${fs_sha1sum} fs_sha256sum=${fs_sha256sum} fs_len=${fs_len} ${optargs};" \
		"bootm ${loadaddr}#conf@orionlx-plus.dtb" \
		"${dtbo_1c_01}${dtbo_1c_02}${dtbo_1c_03};" \
		"bootm ${loadaddr}\0" \
	"extendfspcr=" \
		"if test -n ${fs_sha1sum}; then " \
			"if tpm extend 2 ${fs_sha1sum}; then " \
				"echo tpm extend 2 passed;" \
			"else;" \
				"echo tpm extend 2 failed;" \
				"reset;" \
			"fi;" \
		"else;" \
			"echo no fs_sha1sum found;" \
			"reset;" \
		"fi\0" \
	"extendfitpcr=hash sha1 ${fileaddr} ${filesize} fitsum; " \
		"if test -n ${fitsum}; then " \
			"if tpm extend 1 ${fitsum}; then " \
				"echo tpm extend 1 passed;" \
			"else;" \
				"echo tpm extend 1 failed;" \
				"reset;" \
			"fi;" \
		"else;" \
			"echo no fitsum found;" \
			"reset;" \
		"fi\0" \
	"checkminversion=" \
		"if tpm nv_read d 0x1008 tpm_min_version; then " \
			"if test \"${tpm_min_version}\" -le \"${kernel_version}\"; then " \
				"if test -n ${pplocked}; then " \
					"echo physical presence already locked;" \
				"elif tpm tsc_physical_presence 0x10; then " \
					"if tpm tsc_physical_presence 0x4; then " \
						"echo locked physical presence;" \
						"setenv pplocked true;" \
					"else;" \
						"echo failed to lock physical presence;" \
						"false;" \
					"fi;" \
				"else;" \
					"echo failed to clear physical presence;" \
					"false;" \
				"fi;" \
			"else;" \
				"echo kernel version of ${kernel_version} is lower than requirement of ${tpm_min_version};" \
				"false;" \
			"fi;" \
		"else;" \
			"echo error reading tpm_min_version;" \
			"false;" \
		"fi;\0" \
	"updateminversion=" \
		"if tpm nv_read d 0x1008 tpm_min_version && tpm nv_read d 0x100c booted_min_version; then " \
			"if test \"${tpm_min_version}\" -lt \"${booted_min_version}\" -a \"${booted_min_version}\" != \"4294967295\"; then " \
				"if tpm tsc_physical_presence 0x8; then " \
					"if tpm nv_write d 0x1008 ${booted_min_version}; then " \
						"echo set tpm_min_version to ${booted_min_version};" \
					"else;" \
						"echo failed to set tpm_min_version to ${booted_min_version};" \
						"false;" \
					"fi;" \
				"else;" \
					"echo failed to set physical presence;" \
					"false;" \
				"fi;" \
			"else;" \
				"echo tpm_min_version okay;" \
			"fi;" \
		"else;" \
			"echo error reading versions from tpm;" \
			"false;" \
		"fi;\0" \
	"whichbootslot=" /* return false for main (A), true for alt (B)*/ \
		"scsi read ${loadaddr} 0x800 1;" \
		"if itest.b *${loadaddr} > 0 && itest.b *${loadaddr} < 0xff; then " \
			"if itest.b *${loadaddr} == 1; then " \
				"setenv revert_boot true;" \
			"else;" \
				"setexpr.b dec_bootctr *${loadaddr} - 1;" \
				"mw.b ${loadaddr} ${dec_bootctr} 1;" \
				"scsi write ${loadaddr} 0x800 1;" \
			"fi;" \
		"fi;" \
		"if test -e scsi 0:1 bootalt.txt; then " \
			"if test -n ${revert_boot}; then " \
				"echo prefer main boot slot;" \
				"false;" \
			"else;" \
				"echo prefer alt boot slot;" \
			"fi;" \
		"else;" \
			"if test -n ${revert_boot}; then " \
				"echo prefer alt boot slot;" \
			"else;" \
				"echo prefer main boot slot;" \
				"false;" \
			"fi;" \
		"fi;\0" \
	"extendrompcr=" \
		"if sf read $loadaddr 0 800000; then " \
			"echo sf read passed;" \
		"else;" \
			"echo sf read failed;" \
			"reset;" \
		"fi;" \
		"if hash sha1 $loadaddr 800000 spisum; then " \
			"echo spisum passed;" \
		"else;" \
			"echo spisum failed;" \
			"reset;" \
		"fi;" \
		"if tpm extend 0 $spisum; then " \
			"echo tpm extend 0 passed;" \
		"else;" \
			"echo tpm extend 0 failed;" \
			"reset;" \
		"fi\0" \
	"tpm_init=" \
		"if tpm init; then " \
			"echo tpm init passed;" \
		"else;" \
			"echo tpm init failed;" \
			"reset;" \
		"fi;" \
		"if tpm startup TPM_ST_CLEAR; then " \
			"echo tpm startup passed;" \
		"else;" \
			"echo tpm startup failed;" \
			"reset;" \
		"fi;" \
		"if tpm continue_self_test; then " \
			"echo continue_self_test passed;" \
		"else;" \
			"echo continue_self_test failed;" \
			"reset;" \
		"fi\0"

#ifdef CONFIG_ORIONLX_PLUS_USB_BOOT
#define ORIONLX_PLUS_USB_BOOT \
	"usb start;" \
	"setenv bootmediatype usb;" \
	"setenv usebootfile ${bootfile};" \
	"setenv usefsfile ${fsfile};" \
	"if run loadimage; then " \
		"setenv bootargs lowerdev=/dev/disk/by-label/usb.rootfs.ro " \
		"upperdev=/dev/disk/by-label/usb.rootfs.rw " \
		"u-boot-version=\\\\\"\"${ver}\"\\\\\" " \
		"${scsiconfigargs} " \
		"${optargs};" \
		"if tpm nv_read d 0x1008 tpm_min_version; then " \
			"if run checkminversion; then " \
				"run extendfitpcr;" \
				"run extendfspcr;" \
				"bootm ${loadaddr}#conf@orionlx-plus.dtb" \
				"${dtbo_1c_01}${dtbo_1c_02}${dtbo_1c_03};" \
				"bootm $loadaddr;" \
			"fi;" \
		"else;" \
			"echo failed to read tpm nv index 0x1008, doing nv setup;" \
			"if tpm tsc_physical_presence 0x8; then " \
				"echo set physical presence;" \
				"if tpm nv_define_space 0xffffffff 0 0; then " \
					"echo defined nv index 0xffffffff;" \
				"else;" \
					"echo failed to define nv index 0xffffffff;" \
				"fi;" \
				"if tpm nv_define d 0x1008 1; then " \
					"echo defined nv index 0x1008;" \
					"if tpm nv_write d 0x1008 0; then " \
						"echo set nv index 0x1008 to 0;" \
					"else;" \
						"echo failed to set nv index 0x1008 to 0;" \
					"fi;" \
				"else;" \
					"echo failed to define nv index 0x1008;" \
				"fi;" \
			"else;" \
				"echo failed to set physical presence;" \
			"fi;" \
			"run extendfitpcr;" \
			"run extendfspcr;" \
			"bootm ${loadaddr}#conf@orionlx-plus.dtb" \
			"${dtbo_1c_01}${dtbo_1c_02}${dtbo_1c_03};" \
			"bootm $loadaddr;" \
		"fi;" \
	"fi;"
#else
#define ORIONLX_PLUS_USB_BOOT ""
#endif

#define ORIONLX_PROTECT_FLASH \
	"if sf protect lock 0 800000; then " \
		"echo sf protect lock passed;" \
	"else;" \
		"echo sf protect lock failed;" \
	"fi;" \
	"if sf sr-protect hardware; then " \
		"echo sf sr-protect passed;" \
	"else;" \
		"echo sf sr-protect failed;" \
	"fi;"

#undef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND \
	"scsi scan;" \
	"if date; then " \
		"echo valid date/time read from RTC;" \
	"else;" \
		"echo invalid date/time, resetting RTC;" \
		"date reset;" \
	"fi;" \
	"run tpm_init;" \
	"if sf probe; then " \
		"echo sf probe passed;" \
	"else;" \
		"echo sf probe failed;" \
		"reset;" \
	"fi;" \
	ORIONLX_PROTECT_FLASH \
	"run extendrompcr;" \
	ORIONLX_PLUS_USB_BOOT \
	"if run updateminversion; then " \
		"echo updateminversion passed;" \
	"else;" \
		"echo updateminversion failed;" \
		"reset;" \
	"fi;" \
	"if run whichbootslot; then " \
		"run usealt;" \
		"if run loadimage; then " \
			"if run checkminversion; then " \
				"run extendfitpcr;" \
				"run extendfspcr;" \
				"run scsiboot;" \
			"fi;" \
		"fi;" \
	"fi;" \
	"run usemain;" \
	"if run loadimage; then " \
		"if run checkminversion; then " \
			"run extendfitpcr;" \
			"run extendfspcr;" \
			"run scsiboot;" \
		"fi;" \
	"fi;" \
	"run usealt;" \
	"if run loadimage; then " \
		"if run checkminversion; then " \
			"run extendfitpcr;" \
			"run extendfspcr;" \
			"run scsiboot;" \
		"fi;" \
	"fi;" \
	"reset;"

#endif	/* __CONFIG_H */
