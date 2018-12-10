// SPDX-License-Identifier: GPL-2.0+
/*
 * FPGA Driver for Altera Arria/Cyclone/Stratix CvP
 *
 * Based on the Linux kernel driver:
 * FPGA Manager Driver for Altera Arria/Cyclone/Stratix CvP
 *
 * Copyright (C) 2017 DENX Software Engineering
 *
 * Anatolij Gustschin <agust@denx.de>
 */
#include <common.h>
#include <altera.h>
#include <asm/io.h>
#include <linux/errno.h>
#include <pci.h>

#define SZ_4K 0x1000

#define CVP_DUMMY_WR	244	/* dummy writes to clear CvP state machine */
#define TIMEOUT_US	2000	/* CVP STATUS timeout for USERMODE polling */

/* Vendor Specific Extended Capability Registers */
#define VSE_PCIE_EXT_CAP_ID		0x200
#define VSE_PCIE_EXT_CAP_ID_VAL		0x000b	/* 16bit */

#define VSE_CVP_STATUS			0x21c	/* 32bit */
#define VSE_CVP_STATUS_CFG_RDY		BIT(18)	/* CVP_CONFIG_READY */
#define VSE_CVP_STATUS_CFG_ERR		BIT(19)	/* CVP_CONFIG_ERROR */
#define VSE_CVP_STATUS_CVP_EN		BIT(20)	/* ctrl block is enabling CVP */
#define VSE_CVP_STATUS_USERMODE		BIT(21)	/* USERMODE */
#define VSE_CVP_STATUS_CFG_DONE		BIT(23)	/* CVP_CONFIG_DONE */
#define VSE_CVP_STATUS_PLD_CLK_IN_USE	BIT(24)	/* PLD_CLK_IN_USE */

#define VSE_CVP_MODE_CTRL		0x220	/* 32bit */
#define VSE_CVP_MODE_CTRL_CVP_MODE	BIT(0)	/* CVP (1) or normal mode (0) */
#define VSE_CVP_MODE_CTRL_HIP_CLK_SEL	BIT(1) /* PMA (1) or fabric clock (0) */
#define VSE_CVP_MODE_CTRL_NUMCLKS_OFF	8	/* NUMCLKS bits offset */
#define VSE_CVP_MODE_CTRL_NUMCLKS_MASK	GENMASK(15, 8)

#define VSE_CVP_DATA			0x228	/* 32bit */
#define VSE_CVP_PROG_CTRL		0x22c	/* 32bit */
#define VSE_CVP_PROG_CTRL_CONFIG	BIT(0)
#define VSE_CVP_PROG_CTRL_START_XFER	BIT(1)

#define VSE_UNCOR_ERR_STATUS		0x234	/* 32bit */
#define VSE_UNCOR_ERR_CVP_CFG_ERR	BIT(5)	/* CVP_CONFIG_ERROR_LATCHED */

struct altera_cvp_conf {
	pci_dev_t		pci_dev;
	void __iomem		*map;
	void			(*write_data)(struct altera_cvp_conf *, u32);
	u8			numclks;
};

static bool altera_cvp_chkcfg = false;

static struct pci_device_id supported[] = {
	{ PCI_DEVICE(0x1172, 0xe001) },
	{}
};

static void altera_cvp_write_data_iomem(struct altera_cvp_conf *conf, u32 val)
{
	writel(val, conf->map);
}

static void altera_cvp_write_data_config(struct altera_cvp_conf *conf, u32 val)
{
	pci_write_config32(conf->pci_dev, VSE_CVP_DATA, val);
}

/* switches between CvP clock and internal clock */
static void altera_cvp_dummy_write(struct altera_cvp_conf *conf)
{
	unsigned int i;
	u32 val;

	/* set 1 CVP clock cycle for every CVP Data Register Write */
	pci_read_config32(conf->pci_dev, VSE_CVP_MODE_CTRL, &val);
	val &= ~VSE_CVP_MODE_CTRL_NUMCLKS_MASK;
	val |= 1 << VSE_CVP_MODE_CTRL_NUMCLKS_OFF;
	pci_write_config32(conf->pci_dev, VSE_CVP_MODE_CTRL, val);

	for (i = 0; i < CVP_DUMMY_WR; i++)
		conf->write_data(conf, 0); /* dummy data, could be any value */
}

static int altera_cvp_wait_status(struct altera_cvp_conf *conf, u32 status_mask,
				  u32 status_val, int timeout_us)
{
	unsigned int retries;
	u32 val;

	retries = timeout_us / 10;
	if (timeout_us % 10)
		retries++;

	do {
		pci_read_config32(conf->pci_dev, VSE_CVP_STATUS, &val);
		if ((val & status_mask) == status_val)
			return 0;

		/* use small usleep value to re-check and break early */
		udelay(10);
	} while (--retries);

	return -ETIMEDOUT;
}

static int altera_cvp_teardown(struct altera_cvp_conf *conf)
{
	int ret;
	u32 val;

	/* STEP 12 - reset START_XFER bit */
	pci_read_config32(conf->pci_dev, VSE_CVP_PROG_CTRL, &val);
	val &= ~VSE_CVP_PROG_CTRL_START_XFER;
	pci_write_config32(conf->pci_dev, VSE_CVP_PROG_CTRL, val);

	/* STEP 13 - reset CVP_CONFIG bit */
	val &= ~VSE_CVP_PROG_CTRL_CONFIG;
	pci_write_config32(conf->pci_dev, VSE_CVP_PROG_CTRL, val);

	/*
	 * STEP 14
	 * - set CVP_NUMCLKS to 1 and then issue CVP_DUMMY_WR dummy
	 *   writes to the HIP
	 */
	altera_cvp_dummy_write(conf); /* from CVP clock to internal clock */

	/* STEP 15 - poll CVP_CONFIG_READY bit for 0 with 10us timeout */
	ret = altera_cvp_wait_status(conf, VSE_CVP_STATUS_CFG_RDY, 0, 10);
	if (ret)
		printf("CFG_RDY == 0 timeout\n");

	return ret;
}

static int altera_cvp_probe(pci_dev_t devno, struct altera_cvp_conf *conf)
{
	u16 cmd, val;

	/*
	 * First check if this is the expected FPGA device. PCI config
	 * space access works without enabling the PCI device, memory
	 * space access is enabled further down.
	 */
	pci_read_config16(devno, VSE_PCIE_EXT_CAP_ID, &val);
	if (val != VSE_PCIE_EXT_CAP_ID_VAL) {
		printf("Wrong EXT_CAP_ID value 0x%x\n", val);
		return -ENODEV;
	}

	/*
	 * Enable memory BAR access. We cannot use pci_enable_device() here
	 * because it will make the driver unusable with FPGA devices that
	 * have additional big IOMEM resources (e.g. 4GiB BARs) on 32-bit
	 * platform. Such BARs will not have an assigned address range and
	 * pci_enable_device() will fail, complaining about not claimed BAR,
	 * even if the concerned BAR is not needed for FPGA configuration
	 * at all. Thus, enable the device via PCI config space command.
	 */
	pci_read_config16(devno, PCI_COMMAND, &cmd);
	if (!(cmd & PCI_COMMAND_MEMORY)) {
		cmd |= PCI_COMMAND_MEMORY;
		pci_write_config16(devno, PCI_COMMAND, cmd);
	}

	conf->pci_dev = devno;
	conf->write_data = altera_cvp_write_data_iomem;

	conf->map = pci_map_bar(devno, PCI_BASE_ADDRESS_0, PCI_REGION_MEM);
	if (!conf->map) {
		printf("Mapping CVP BAR failed\n");
		conf->write_data = altera_cvp_write_data_config;
	}

	return 0;
}

int altera_cvp_write_init(struct altera_cvp_conf *conf)
{
	u32 val;
	int ret;

	/* TODO - add support for compressed and encrypted bitstreams */
	conf->numclks = 1; /* for uncompressed and unencrypted images */

	/* STEP 1 - read CVP status and check CVP_EN flag */
	pci_read_config32(conf->pci_dev, VSE_CVP_STATUS, &val);
	if (!(val & VSE_CVP_STATUS_CVP_EN)) {
		printf("CVP mode off: 0x%04x\n", val);
		return -ENODEV;
	}

	if (val & VSE_CVP_STATUS_CFG_RDY) {
		printf("CvP already started, teardown first\n");
		ret = altera_cvp_teardown(conf);
		if (ret)
			return ret;
	}

	/*
	 * STEP 2
	 * - set HIP_CLK_SEL and CVP_MODE (must be set in the order mentioned)
	 */
	/* switch from fabric to PMA clock */
	pci_read_config32(conf->pci_dev, VSE_CVP_MODE_CTRL, &val);
	val |= VSE_CVP_MODE_CTRL_HIP_CLK_SEL;
	pci_write_config32(conf->pci_dev, VSE_CVP_MODE_CTRL, val);

	/* set CVP mode */
	pci_read_config32(conf->pci_dev, VSE_CVP_MODE_CTRL, &val);
	val |= VSE_CVP_MODE_CTRL_CVP_MODE;
	pci_write_config32(conf->pci_dev, VSE_CVP_MODE_CTRL, val);

	/*
	 * STEP 3
	 * - set CVP_NUMCLKS to 1 and issue CVP_DUMMY_WR dummy writes to the HIP
	 */
	altera_cvp_dummy_write(conf);

	/* STEP 4 - set CVP_CONFIG bit */
	pci_read_config32(conf->pci_dev, VSE_CVP_PROG_CTRL, &val);
	/* request control block to begin transfer using CVP */
	val |= VSE_CVP_PROG_CTRL_CONFIG;
	pci_write_config32(conf->pci_dev, VSE_CVP_PROG_CTRL, val);

	/*
	 * STEP 5 - poll CVP_CONFIG READY for 1 with 1000us timeout.
	 *          10us isn't long enough when FPGA is already in user mode.
	 */
	ret = altera_cvp_wait_status(conf, VSE_CVP_STATUS_CFG_RDY,
				     VSE_CVP_STATUS_CFG_RDY, 1000);
	if (ret) {
		printf("CFG_RDY == 1 timeout\n");
		return ret;
	}

	/*
	 * STEP 6
	 * - set CVP_NUMCLKS to 1 and issue CVP_DUMMY_WR dummy writes to the HIP
	 */
	altera_cvp_dummy_write(conf);

	/* STEP 7 - set START_XFER */
	pci_read_config32(conf->pci_dev, VSE_CVP_PROG_CTRL, &val);
	val |= VSE_CVP_PROG_CTRL_START_XFER;
	pci_write_config32(conf->pci_dev, VSE_CVP_PROG_CTRL, val);

	/* STEP 8 - start transfer (set CVP_NUMCLKS for bitstream) */
	pci_read_config32(conf->pci_dev, VSE_CVP_MODE_CTRL, &val);
	val &= ~VSE_CVP_MODE_CTRL_NUMCLKS_MASK;
	val |= conf->numclks << VSE_CVP_MODE_CTRL_NUMCLKS_OFF;
	pci_write_config32(conf->pci_dev, VSE_CVP_MODE_CTRL, val);

	return 0;
}

static inline int altera_cvp_chk_error(struct altera_cvp_conf *conf, size_t bytes)
{
	u32 val;

	/* STEP 10 (optional) - check CVP_CONFIG_ERROR flag */
	pci_read_config32(conf->pci_dev, VSE_CVP_STATUS, &val);
	if (val & VSE_CVP_STATUS_CFG_ERR) {
		printf("CVP_CONFIG_ERROR after %zu bytes!\n",
			bytes);
		return -EPROTO;
	}
	return 0;
}

static int altera_cvp_write(struct altera_cvp_conf *conf, const void *buf,
			    size_t count)
{
	const u32 *data;
	size_t done, remaining;
	int status = 0;
	u32 mask;

	/* STEP 9 - write 32-bit data from RBF file to CVP data register */
	data = (u32 *)buf;
	remaining = count;
	done = 0;

	while (remaining >= 4) {
		conf->write_data(conf, *data++);
		done += 4;
		remaining -= 4;

		/*
		 * STEP 10 (optional) and STEP 11
		 * - check error flag
		 * - loop until data transfer completed
		 * Config images can be huge (more than 40 MiB), so
		 * only check after a new 4k data block has been written.
		 * This reduces the number of checks and speeds up the
		 * configuration process.
		 */
		if (altera_cvp_chkcfg && !(done % SZ_4K)) {
			status = altera_cvp_chk_error(conf, done);
			if (status < 0)
				return status;
		}
	}

	/* write up to 3 trailing bytes, if any */
	mask = BIT(remaining * 8) - 1;
	if (mask)
		conf->write_data(conf, *data & mask);

	if (altera_cvp_chkcfg)
		status = altera_cvp_chk_error(conf, count);

	return status;
}

static int altera_cvp_write_complete(struct altera_cvp_conf *conf)
{
	int ret;
	u32 mask;
	u32 val;

	ret = altera_cvp_teardown(conf);
	if (ret)
		return ret;

	/* STEP 16 - check CVP_CONFIG_ERROR_LATCHED bit */
	pci_read_config32(conf->pci_dev, VSE_UNCOR_ERR_STATUS, &val);
	if (val & VSE_UNCOR_ERR_CVP_CFG_ERR) {
		printf("detected CVP_CONFIG_ERROR_LATCHED!\n");
		return -EPROTO;
	}

	/* STEP 17 - reset CVP_MODE and HIP_CLK_SEL bit */
	pci_read_config32(conf->pci_dev, VSE_CVP_MODE_CTRL, &val);
	val &= ~VSE_CVP_MODE_CTRL_HIP_CLK_SEL;
	val &= ~VSE_CVP_MODE_CTRL_CVP_MODE;
	pci_write_config32(conf->pci_dev, VSE_CVP_MODE_CTRL, val);

	/* STEP 18 - poll PLD_CLK_IN_USE and USER_MODE bits */
	mask = VSE_CVP_STATUS_PLD_CLK_IN_USE | VSE_CVP_STATUS_USERMODE;
	ret = altera_cvp_wait_status(conf, mask, mask, TIMEOUT_US);
	if (ret)
		printf("PLD_CLK_IN_USE|USERMODE timeout\n");

	return ret;
}

/*
 * This is the interface used by FPGA driver.
 * Return 0 for sucess, non-zero for error.
 */
int altera_cvp_load(Altera_desc *desc, const void *rbf_data, size_t rbf_size)
{
	pci_dev_t devno;
	int ret;
	struct altera_cvp_conf conf;

	devno = pci_find_devices(supported, 0);
	if (devno < 0) {
		puts("altera-cvp not found!\n");
		return -ENODEV;
	}

	ret = altera_cvp_probe(devno, &conf);
	if (ret)
		return ret;

	ret = altera_cvp_write_init(&conf);
	if (ret)
		return ret;

	ret = altera_cvp_write(&conf, rbf_data, rbf_size);
	if (ret)
		return ret;

	return altera_cvp_write_complete(&conf);
}
