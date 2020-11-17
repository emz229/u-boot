// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016 Stefan Roese <sr@denx.de>
 * Copyright (C) 2016 George McCollister <george.mccollister@gmail.com>
 */

#include <common.h>
#include <asm/fsp/fsp_support.h>
#include <altera.h>
#include <dm.h>
#include <pci.h>

/* ALC262 Verb Table - 10EC0262 */
static const u32 verb_table_data13[] = {
	/* Pin Complex (NID 0x11) */
	0x01171cf0,
	0x01171d11,
	0x01171e11,
	0x01171f41,
	/* Pin Complex (NID 0x12) */
	0x01271cf0,
	0x01271d11,
	0x01271e11,
	0x01271f41,
	/* Pin Complex (NID 0x14) */
	0x01471c10,
	0x01471d40,
	0x01471e01,
	0x01471f01,
	/* Pin Complex (NID 0x15) */
	0x01571cf0,
	0x01571d11,
	0x01571e11,
	0x01571f41,
	/* Pin Complex (NID 0x16) */
	0x01671cf0,
	0x01671d11,
	0x01671e11,
	0x01671f41,
	/* Pin Complex (NID 0x18) */
	0x01871c20,
	0x01871d98,
	0x01871ea1,
	0x01871f01,
	/* Pin Complex (NID 0x19) */
	0x01971c21,
	0x01971d98,
	0x01971ea1,
	0x01971f02,
	/* Pin Complex (NID 0x1A) */
	0x01a71c2f,
	0x01a71d30,
	0x01a71e81,
	0x01a71f01,
	/* Pin Complex */
	0x01b71c1f,
	0x01b71d40,
	0x01b71e21,
	0x01b71f02,
	/* Pin Complex */
	0x01c71cf0,
	0x01c71d11,
	0x01c71e11,
	0x01c71f41,
	/* Pin Complex */
	0x01d71c01,
	0x01d71dc6,
	0x01d71e14,
	0x01d71f40,
	/* Pin Complex */
	0x01e71cf0,
	0x01e71d11,
	0x01e71e11,
	0x01e71f41,
	/* Pin Complex */
	0x01f71cf0,
	0x01f71d11,
	0x01f71e11,
	0x01f71f41,
};

/*
 * This needs to be in ROM since if we put it in CAR, FSP init loses it when
 * it drops CAR.
 *
 * VerbTable: (RealTek ALC262)
 * Revision ID = 0xFF, support all steps
 * Codec Verb Table For AZALIA
 * Codec Address: CAd value (0/1/2)
 * Codec Vendor: 0x10EC0262
 */
static const struct azalia_verb_table azalia_verb_table[] = {
	{
		{
			0x10ec0262,
			0x0000,
			0xff,
			0x01,
			0x000b,
			0x0002,
		},
		verb_table_data13
	}
};

static const struct azalia_config azalia_config = {
	.pme_enable = 1,
	.docking_supported = 1,
	.docking_attached = 0,
	.hdmi_codec_enable = 1,
	.azalia_v_ci_enable = 1,
	.rsvdbits = 0,
	.verb_table_num = 1,
	.verb_table = azalia_verb_table,
	.reset_wait_timer_ms = 300
};

void update_fsp_azalia_configs(const struct azalia_config **azalia)
{
	*azalia = &azalia_config;
}

int board_early_init_f(void)
{
	/*
	 * The FSP enables the BayTrail internal legacy UART (again).
	 * Disable it again, so that the one on the EC can be used.
	 */
	setup_internal_uart(0);

	return 0;
}

#ifdef CONFIG_FPGA
static Altera_desc altera_fpga[] = {
	{
		/* Family */
		Altera_CVP,
		/* Interface type */
		fast_passive_parallel,
		/* No limitation as additional data will be ignored */
		-1,
		/* No device function table */
		NULL,
		/* Base interface address specified in driver */
		NULL,
		/* No cookie implementation */
		0
	},
};

int board_fpga_init(void)
{
	int i;
	fpga_init();
	for (i = 0; i < ARRAY_SIZE(altera_fpga); i++)
		fpga_add(fpga_altera, &altera_fpga[i]);

	return 0;
}
#endif

/* Hijack this vendor ID for identifying cards that require an overlay */
#define PCI_VENDOR_ID_NOVAWEB 0x14ac
#define PCI_CLASS(c) (((c) >> 8) & 0xffff)
int board_walk_pci(int *busnum, int depth, int root_devfn)
{
	int ret;
	struct udevice *dev, *bus;
	int i;

	ret = uclass_get_device_by_seq(UCLASS_PCI, *busnum, &bus);
	if (ret) {
		printf("No such bus\n");
		return -1;
	}

	for (device_find_first_child(bus, &dev);
	     dev;
	     device_find_next_child(&dev)) {
		struct pci_child_platdata *pplat;
		u16 subsystem_vid;
		u16 subsystem_id;
		u8 header_type;

		pplat = dev_get_parent_platdata(dev);

		for (i = 0; i < depth; i++) {
			debug("\t");
		}

		debug("%02x.%02x.%02x - ", bus->seq,
		      PCI_DEV(pplat->devfn), PCI_FUNC(pplat->devfn));
		debug("%04x:%04x %04x",
		      pplat->vendor, pplat->device,
		      PCI_CLASS(pplat->class));

		dm_pci_read_config8(dev, PCI_HEADER_TYPE, &header_type);
		if ((header_type & 0x03) == PCI_HEADER_TYPE_NORMAL) {
			dm_pci_read_config16(dev, PCI_SUBSYSTEM_VENDOR_ID,
					     &subsystem_vid);

			dm_pci_read_config16(dev, PCI_SUBSYSTEM_ID,
					     &subsystem_id);
			debug(", %04x:%04x",
			      subsystem_vid,
			      subsystem_id);

			/*
			 * We will set the subsystem vid on some PCIe device
			 * on every card which requires an overlay to
			 * PCI_VENDOR_ID_NOVAWEB.
			 * We will use the subsystem id to identify the
			 * overlay.
			 */
			if (subsystem_vid == PCI_VENDOR_ID_NOVAWEB &&
			    root_devfn) {
				char name[16];
				char value[48];

				snprintf(name, sizeof(name),
					"dtbo_%02x_%02x",
					PCI_DEV(root_devfn),
					PCI_FUNC(root_devfn));
				snprintf(value, sizeof(value),
					"#conf@orionlx-plus-slot-"
					"%02x-%02x-card-%04x.dtbo",
					PCI_DEV(root_devfn),
					PCI_FUNC(root_devfn),
					subsystem_id);
				env_set(name, value);
			}
		}
		debug("\n");

		if (PCI_CLASS(pplat->class) == PCI_CLASS_BRIDGE_PCI) {
			(*busnum)++;
			if (!depth)
				root_devfn = pplat->devfn;
			ret = board_walk_pci(busnum, depth + 1, root_devfn);
			if (ret)
				return ret;
		}
	}

	return 0;
}

int board_late_init(void)
{
	int ret = 0;
	int busnum = 0;

	board_walk_pci(&busnum, 0, 0);

#ifdef CONFIG_FPGA
	ret = board_fpga_init();
#endif
	return ret;
}
