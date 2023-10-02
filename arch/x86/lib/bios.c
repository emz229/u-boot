// SPDX-License-Identifier: GPL-2.0
/*
 * From Coreboot file device/oprom/realmode/x86.c
 *
 * Copyright (C) 2007 Advanced Micro Devices, Inc.
 * Copyright (C) 2009-2010 coresystems GmbH
 */
#include <common.h>
#include <bios_emul.h>
#include <vbe.h>
#include <linux/linkage.h>
#include <asm/cache.h>
#include <asm/processor.h>
#include <asm/i8259.h>
#include <asm/io.h>
#include <asm/post.h>
#include "bios.h"
#include <cpu.h>

/* Interrupt handlers for each interrupt the ROM can call */
static int (*int_handler[256])(void);

/* to have a common register file for interrupt handlers */
X86EMU_sysEnv _X86EMU_env;

asmlinkage void (*realmode_call)(u32 addr, u32 eax, u32 ebx, u32 ecx, u32 edx,
				 u32 esi, u32 edi);

asmlinkage void (*realmode_interrupt)(u32 intno, u32 eax, u32 ebx, u32 ecx,
				      u32 edx, u32 esi, u32 edi);

static void setup_realmode_code(void)
{
	memcpy((void *)REALMODE_BASE, &asm_realmode_code,
	       asm_realmode_code_size);

	/* Ensure the global pointers are relocated properly. */
	realmode_call = PTR_TO_REAL_MODE(asm_realmode_call);
	realmode_interrupt = PTR_TO_REAL_MODE(__realmode_interrupt);

	printf("Real mode stub @%x: %d bytes\n", REALMODE_BASE,
	      asm_realmode_code_size);
}

static void setup_rombios(void)
{
	const char date[] = "06/11/99";
	memcpy((void *)0xffff5, &date, 8);

	const char ident[] = "PCI_ISA";
	memcpy((void *)0xfffd9, &ident, 7);

	/* system model: IBM-AT */
	writeb(0xfc, 0xffffe);
}

static int int_exception_handler(void)
{
	/* compatibility shim */
	struct eregs reg_info = {
		.eax = M.x86.R_EAX,
		.ecx = M.x86.R_ECX,
		.edx = M.x86.R_EDX,
		.ebx = M.x86.R_EBX,
		.esp = M.x86.R_ESP,
		.ebp = M.x86.R_EBP,
		.esi = M.x86.R_ESI,
		.edi = M.x86.R_EDI,
		.vector = M.x86.intno,
		.error_code = 0,
		.eip = M.x86.R_EIP,
		.cs = M.x86.R_CS,
		.eflags = M.x86.R_EFLG
	};
	struct eregs *regs = &reg_info;

	printf("Oops, exception %d while executing option rom\n", regs->vector);
	cpu_hlt();

	return 0;
}

static int int_unknown_handler(void)
{
	printf("Unsupported software interrupt #0x%x eax 0x%x\n",
	      M.x86.intno, M.x86.R_EAX);

	return -1;
}

/* setup interrupt handlers for mainboard */
void bios_set_interrupt_handler(int intnum, int (*int_func)(void))
{
	int_handler[intnum] = int_func;
}

static void setup_interrupt_handlers(void)
{
	int i;

	/*
	 * The first 16 int_handler functions are not BIOS services,
	 * but the CPU-generated exceptions ("hardware interrupts")
	 */
	for (i = 0; i < 0x10; i++)
		int_handler[i] = &int_exception_handler;

	/* Mark all other int_handler calls as unknown first */
	for (i = 0x10; i < 0x100; i++) {
		/* Skip if bios_set_interrupt_handler() isn't called first */
		if (int_handler[i])
			continue;

		 /*
		  * Now set the default functions that are actually needed
		  * to initialize the option roms. The board may override
		  * these with bios_set_interrupt_handler()
		 */
		switch (i) {
		case 0x10:
			int_handler[0x10] = &int10_handler;
			break;
		case 0x12:
			int_handler[0x12] = &int12_handler;
			break;
		case 0x16:
			int_handler[0x16] = &int16_handler;
			break;
		case 0x1a:
			int_handler[0x1a] = &int1a_handler;
			break;
		default:
			int_handler[i] = &int_unknown_handler;
			break;
		}
	}
}

static void write_idt_stub(void *target, u8 intnum)
{
	unsigned char *codeptr;

	codeptr = (unsigned char *)target;
	memcpy(codeptr, &__idt_handler, __idt_handler_size);
	codeptr[3] = intnum; /* modify int# in the code stub. */
}

static void setup_realmode_idt(void)
{
	struct realmode_idt *idts = NULL;
	int i;

	/*
	 * Copy IDT stub code for each interrupt. This might seem wasteful
	 * but it is really simple
	 */
	 for (i = 0; i < 256; i++) {
		idts[i].cs = 0;
		idts[i].offset = 0x1000 + (i * __idt_handler_size);
		write_idt_stub((void *)((ulong)idts[i].offset), i);
	}

	/*
	 * Many option ROMs use the hard coded interrupt entry points in the
	 * system bios. So install them at the known locations.
	 */

	/* int42 is the relocated int10 */
	write_idt_stub((void *)0xff065, 0x42);
	/* BIOS Int 11 Handler F000:F84D */
	write_idt_stub((void *)0xff84d, 0x11);
	/* BIOS Int 12 Handler F000:F841 */
	write_idt_stub((void *)0xff841, 0x12);
	/* BIOS Int 13 Handler F000:EC59 */
	write_idt_stub((void *)0xfec59, 0x13);
	/* BIOS Int 14 Handler F000:E739 */
	write_idt_stub((void *)0xfe739, 0x14);
	/* BIOS Int 15 Handler F000:F859 */
	write_idt_stub((void *)0xff859, 0x15);
	/* BIOS Int 16 Handler F000:E82E */
	write_idt_stub((void *)0xfe82e, 0x16);
	/* BIOS Int 17 Handler F000:EFD2 */
	write_idt_stub((void *)0xfefd2, 0x17);
	/* ROM BIOS Int 1A Handler F000:FE6E */
	write_idt_stub((void *)0xffe6e, 0x1a);
}

#ifdef CONFIG_FRAMEBUFFER_SET_VESA_MODE
static u8 vbe_get_mode_info(struct vbe_mode_info *mi)
{
	u16 buffer_seg;
	u16 buffer_adr;
	char *buffer;

	printf("VBE: Getting information about VESA mode %04x\n",
	      mi->video_mode);
	buffer = PTR_TO_REAL_MODE(asm_realmode_buffer);
	buffer_seg = (((unsigned long)buffer) >> 4) & 0xff00;
	buffer_adr = ((unsigned long)buffer) & 0xffff;

	realmode_interrupt(0x10, VESA_GET_MODE_INFO, 0x0000, mi->video_mode,
			   0x0000, buffer_seg, buffer_adr);
	memcpy(mi->mode_info_block, buffer, sizeof(struct vbe_mode_info));
	mi->valid = true;

	return 0;
}

static u8 vbe_set_mode(struct vbe_mode_info *mi)
{
	int video_mode = mi->video_mode;

	printf("VBE: Setting VESA mode %#04x\n", video_mode);
	/* request linear framebuffer mode */
	video_mode |= (1 << 14);
	/* don't clear the framebuffer, we do that later */
	video_mode |= (1 << 15);
	realmode_interrupt(0x10, VESA_SET_MODE, video_mode,
			   0x0000, 0x0000, 0x0000, 0x0000);

	return 0;
}

static void vbe_set_graphics(int vesa_mode, struct vbe_mode_info *mode_info)
{
	unsigned char *framebuffer;

	mode_info->video_mode = (1 << 14) | vesa_mode;
	vbe_get_mode_info(mode_info);

	framebuffer = (unsigned char *)(ulong)mode_info->vesa.phys_base_ptr;
	printf("VBE: resolution:  %dx%d@%d\n",
	      le16_to_cpu(mode_info->vesa.x_resolution),
	      le16_to_cpu(mode_info->vesa.y_resolution),
	      mode_info->vesa.bits_per_pixel);
	printf("VBE: framebuffer: %p\n", framebuffer);
	if (!framebuffer) {
		printf("VBE: Mode does not support linear framebuffer\n");
		return;
	}

	mode_info->video_mode &= 0x3ff;
	vbe_set_mode(mode_info);
}
#endif /* CONFIG_FRAMEBUFFER_SET_VESA_MODE */

void bios_run_on_x86(struct udevice *dev, unsigned long addr, int vesa_mode,
		     struct vbe_mode_info *mode_info)
{
	pci_dev_t pcidev;
	u32 num_dev;
	u32 pci_data;
	u32 mmio_bar;
	u32 offset_word;
	u8 data_byte;
	u32 offset_end;

	pcidev = dm_pci_get_bdf(dev);
	num_dev = PCI_BUS(pcidev) << 8 | PCI_DEV(pcidev) << 3 |
			PCI_FUNC(pcidev);

	/* Needed to avoid exceptions in some ROMs */
	interrupt_init();

	/* Set up some legacy information in the F segment */
	setup_rombios();

	/* Set up C interrupt handlers */
	setup_interrupt_handlers();

	/* Set up real-mode IDT */
	setup_realmode_idt();

	/* Make sure the code is placed. */
	setup_realmode_code();

	printf("Running on x86 CPU ID: %08x", cpu_get_family_model());

	printf("Calling Option ROM at %lx, pci device %#x...", addr, num_dev);

	/* Option ROM entry point is at OPROM start + 3 */
	realmode_call(addr + 0x0003, num_dev, 0xffff, 0x0000, 0xffff, 0x0,
		      0x0);
	printf("done\n");

	printf("BDF Notation: %x:%x.%x\n", PCI_BUS(pcidev), PCI_DEV(pcidev), PCI_FUNC(pcidev));

	mmio_bar = 0;

	for (int offset = 0; offset < 0xFF; offset=offset+4) {
		dm_pci_read_config32(dev, offset, &pci_data);
		printf("Offset: %X, Data: %08X\n", offset, pci_data);
		if (offset == 0x10) { //GTTMMADR address to be use as BAR for MMIO
			mmio_bar = pci_data;
		}
	}

	printf("Memory Mapped Registers (8Bit):\n");

	// GTTMMADR_LSB is offset 10h
	// CRX (CRX_MDA) is offset GTTMMADR_LSB + 180000h + 3B4h
	// Therefore init register is 1803C4h

	offset_word = 0x1803B4;
	data_byte;

	for (int i = 0; i < 15; i++, offset_word++) {
		// dm_pci_read_config8(dev, offset_word, &data_byte);
		data_byte = readb(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %02X\n", (mmio_bar+offset_word), data_byte);
	}

	printf("Memory Mapped Registers (32Bit):\n");

	// GTTMMADR_LSB is offset 10h
	// GPIOCTL_0 is offset GTTMMADR_LSB + 180000h + 5010h
	// Therefore init register is 185010h

	offset_word = 0x185010;
	pci_data = 0;
	offset_end = 0x185137;

	// Range 1
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
		if ((offset_word == 0x185014) && (pci_data != 0x00000808)){
			printf("Detected invalid config.. setting\n");
			writel(0x00000808, mmio_bar + offset_word);
		}
	}
	//Range 2
	offset_word = 0x186014;
	offset_end = 0x186107;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 3
	offset_word = 0x186200;
	offset_end = 0x186213;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 4
	offset_word = 0x186500;
	offset_end = 0x186513;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 5
	offset_word = 0x18A000;
	offset_end = 0x18A803;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 6
	offset_word = 0x18B000;
	offset_end = 0x18B01B;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 7
	offset_word = 0x18B01C;
	offset_end = 0x18B13B;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 8
	offset_word = 0x18B800;
	offset_end = 0x18B897;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 9
	offset_word = 0x18B904;
	offset_end = 0x18B93B;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 10
	offset_word = 0x1E0000;
	offset_end = 0x1E00C7;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 11
	offset_word = 0x1E0200;
	offset_end = 0x1E0213;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 12
	offset_word = 0x1E1000;
	offset_end = 0x1E104F;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	printf("Memory Map Table 2:\n");
	//Range 1
	offset_word = 0x1E1050;
	offset_end = 0x1E1083;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 2
	offset_word = 0x1E1090;
	offset_end = 0x1E10A3;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 3
	offset_word = 0x1E10B0;
	offset_end = 0x1E10C7;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 4
	offset_word = 0x1E1100;
	offset_end = 0x1E1117;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 5
	offset_word = 0x1E1140;
	offset_end = 0x1E1143;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 6
	offset_word = 0x1E1154;
	offset_end = 0x1E1157;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 7
	offset_word = 0x1E1160;
	offset_end = 0x1E117B;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 8
	offset_word = 0x1E1190;
	offset_end = 0x1E11B3;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 9
	offset_word = 0x1E1200;
	offset_end = 0x1E1213;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 10
	offset_word = 0x1E1230;
	offset_end = 0x1E123F;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 11
	offset_word = 0x1E1250;
	offset_end = 0x1E126B;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 12
	offset_word = 0x1E1300;
	offset_end = 0x1E1313;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 13
	offset_word = 0x1E1350;
	offset_end = 0x1E136B;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 14
	offset_word = 0x1E1700;
	offset_end = 0x1E1707;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 15
	offset_word = 0x1E2000;
	offset_end = 0x1E20D7;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 16
	offset_word = 0x1E2100;
	offset_end = 0x1E21B7;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 17
	offset_word = 0x1E2F00;
	offset_end = 0x1E2F9B;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 18
	offset_word = 0x1E4100;
	offset_end = 0x1E4153;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 19
	offset_word = 0x1E4200;
	offset_end = 0x1E422B;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 20
	offset_word = 0x1E5000;
	offset_end = 0x1E506B;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 21
	offset_word = 0x1E5800;
	offset_end = 0x1E586B;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 22
	offset_word = 0x1F0000;
	offset_end = 0x1F00EF;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 23
	offset_word = 0x1F017C;
	offset_end = 0x1F01AF;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 24
	offset_word = 0x1F0400;
	offset_end = 0x1F0453;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 25
	offset_word = 0x1F1000;
	offset_end = 0x1F104B;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 26
	offset_word = 0x1F117C;
	offset_end = 0x1F11AF;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 27
	offset_word = 0x1F1200;
	offset_end = 0x1F1203;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 28
	offset_word = 0x1F1400;
	offset_end = 0x1F144F;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 29
	offset_word = 0x1F2180;
	offset_end = 0x1F21F7;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 30
	offset_word = 0x1F2280;
	offset_end = 0x1F22F7;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 31
	offset_word = 0x1F2380;
	offset_end = 0x1F23F7;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 32
	offset_word = 0x1F2414;
	offset_end = 0x1F24F7;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}
	//Range 33
	offset_word = 0x1F3000;
	offset_end = 0x1F300F;
	for ( ; offset_word < offset_end; offset_word = offset_word+4) {
		// pci_readl(offset_word, &pci_data);
		pci_data = readl(mmio_bar + offset_word);
		printf("Offset: %08X, Data: %08X\n", (mmio_bar+offset_word), pci_data);
	}

#if 0
	// Show VSYNC count
	pci_data = readl(mmio_bar + 0x1F0040);
	printf("VSYNC Count: %x\n", pci_data);
	printf("VBIOS complete! Enter infinite loop...\n");
	do {
		//Infinite loop to prevent VBIOS load 2nd time
		//Could not find simple halt() function..
	} while (true);
#endif

#ifdef CONFIG_FRAMEBUFFER_SET_VESA_MODE
	if (vesa_mode != -1) {
		printf("Calling vbe_set_graphics...\n");
		vbe_set_graphics(vesa_mode, mode_info);
	}
#endif
}

asmlinkage int interrupt_handler(u32 intnumber, u32 gsfs, u32 dses,
				 u32 edi, u32 esi, u32 ebp, u32 esp,
				 u32 ebx, u32 edx, u32 ecx, u32 eax,
				 u32 cs_ip, u16 stackflags)
{
	u32 ip;
	u32 cs;
	u32 flags;
	int ret = 0;

	ip = cs_ip & 0xffff;
	cs = cs_ip >> 16;
	flags = stackflags;

#ifdef CONFIG_REALMODE_DEBUG
	printf("oprom: INT# 0x%x\n", intnumber);
	printf("oprom: eax: %08x ebx: %08x ecx: %08x edx: %08x\n",
	      eax, ebx, ecx, edx);
	printf("oprom: ebp: %08x esp: %08x edi: %08x esi: %08x\n",
	      ebp, esp, edi, esi);
	printf("oprom:  ip: %04x      cs: %04x   flags: %08x\n",
	      ip, cs, flags);
	printf("oprom: stackflags = %04x\n", stackflags);
#endif

	/*
	 * Fetch arguments from the stack and put them to a place
	 * suitable for the interrupt handlers
	 */
	M.x86.R_EAX = eax;
	M.x86.R_ECX = ecx;
	M.x86.R_EDX = edx;
	M.x86.R_EBX = ebx;
	M.x86.R_ESP = esp;
	M.x86.R_EBP = ebp;
	M.x86.R_ESI = esi;
	M.x86.R_EDI = edi;
	M.x86.intno = intnumber;
	M.x86.R_EIP = ip;
	M.x86.R_CS = cs;
	M.x86.R_EFLG = flags;

	/* Call the interrupt handler for this interrupt number */
	ret = int_handler[intnumber]();

	/*
	 * This code is quite strange...
	 *
	 * Put registers back on the stack. The assembler code will pop them
	 * later. We force (volatile!) changing the values of the parameters
	 * of this function. We know that they stay alive on the stack after
	 * we leave this function.
	 */
	*(volatile u32 *)&eax = M.x86.R_EAX;
	*(volatile u32 *)&ecx = M.x86.R_ECX;
	*(volatile u32 *)&edx = M.x86.R_EDX;
	*(volatile u32 *)&ebx = M.x86.R_EBX;
	*(volatile u32 *)&esi = M.x86.R_ESI;
	*(volatile u32 *)&edi = M.x86.R_EDI;
	flags = M.x86.R_EFLG;

	/* Pass success or error back to our caller via the CARRY flag */
	if (ret) {
		flags &= ~1; /* no error: clear carry */
	} else {
		printf("int%02x call returned error\n", intnumber);
		flags |= 1;  /* error: set carry */
	}
	*(volatile u16 *)&stackflags = flags;

	return ret;
}
