/* This file is loosly based off of the file found in the linux kernel,
 * modified for use with u-boot.
 * File in kernel: drivers/gpu/drm/gma500/intel_gmbus.c
 *
 * Authors:
 * Eric Schikschneit <eric.schikschneit@novatechautomation.com
 */

#include <common.h>
#include <dm.h>
#include <i2c.h>
#include <pci.h>

#include "psb_intel_reg.h"

// struct intel_gpio {
// 	struct i2c_adapter adapter;
// 	struct i2c_algo_bit_data algo;
// 	struct drm_psb_private *dev_priv;
// 	u32 reg;
// };

#define GMBUS_REG_READ(reg) ioread32(dev_priv->gmbus_reg + (reg))
#define GMBUS_REG_WRITE(reg, val) iowrite32((val), dev_priv->gmbus_reg + (reg))

void intel_gmbus_i2c_reset() {
}

static void intel_gmbus_i2c_quirk_set() {
}

static u32 intel_gmbus_get_reserved() {
    return 0;
}

static void intel_gmbus_set_clock (void *data, int state_high) {
}

static int intel_gmbus_get_clock(void *data) {
    return 0;
}

static void intel_gmbus_set_data(void *data, int state_high) {
}

static int intel_gmbus_get_data(void *data) {
    return 0;
}

static struct i2c_adapter * intel_gmbus_gpio_create() { // Use u-boot compatible struct
    return;
}

static int intel_gmbus_i2c_quirk_xfer() {
    return 0;
}

static int intel_gmbus_xfer() {
    return 0;
}

static u32 intel_gmbus_func() {
    return 0;
}

void intel_gmbus_setup() {
}

static int intel_gmbus_set_bus_speed(struct udevice *dev, unsigned int speed) {
    if (speed != (GMBUS_RATE_100KHZ || GMBUS_RATE_50KHZ	|| 
                  GMBUS_RATE_400KHZ || GMBUS_RATE_1MHZ)) {
        printf("Invalid speed request!\n");
        return 1;
    }
    //TODO: Place logic here to actually set bus speed in register GMBUS0
}

int intel_gmbus_get_speed(struct udevice *dev) {
    return 0;
}

void intel_gmbus_force_bit() {
}

void intel_gmbus_teardown() {
}

static int intel_gmbus_probe_chip(struct udevice *dev, uint chip, uint chip_flags) {
    pci_dev_t pcidev;
    u32 num_dev;

	pcidev = dm_pci_get_bdf(dev);

    num_dev = PCI_BUS(pcidev) << 8 | PCI_DEV(pcidev) << 3 |
			PCI_FUNC(pcidev);
    printf("Probe PCIDEV: %x\n", num_dev);
    printf("BDF Notation: %x:%x.%x\n", PCI_BUS(pcidev), PCI_DEV(pcidev), PCI_FUNC(pcidev));
    
    return 0;
}

static int intel_gmbus_probe(struct udevice *dev)
{
	struct intel_gmbus *priv = dev_get_priv(dev);
	ulong base;

	/* Save base address from PCI BAR */
	priv->base = (ulong)dm_pci_map_bar(dev, GMB_BASE,
					   PCI_REGION_SYS_MEMORY);
	base = priv->base;

    printf("(GMBUS) Using BAR: %08x\n", base);

	// /* Set SMBus enable. */
	// dm_pci_write_config8(dev, HOSTC, HST_EN);

	// /* Disable interrupts */
	// outb(inb(base + SMBHSTCTL) & ~SMBHSTCNT_INTREN, base + SMBHSTCTL);

	// /* Set 32-byte data buffer mode */
	// outb(inb(base + SMBAUXCTL) | SMBAUXCTL_E32B, base + SMBAUXCTL);

	return 0;
}

static int intel_gmbus_bind(struct udevice *dev)
{
	static int num_cards __attribute__ ((section(".data")));
	char name[20];

	/* Create a unique device name for PCI type devices */
	if (device_is_on_pci_bus(dev)) {
		/*
		 * ToDo:
		 * Setting req_seq in the driver is probably not recommended.
		 * But without a DT alias the number is not configured. And
		 * using this driver is impossible for PCIe I2C devices.
		 * This can be removed, once a better (correct) way for this
		 * is found and implemented.
		 */
		dev->req_seq = num_cards;
		sprintf(name, "intel_gmbus#%u", num_cards++);
		device_set_name(dev, name);
	}

	return 0;
}

static const struct dm_i2c_ops intel_gmbus_ops = {
	.xfer		= intel_gmbus_xfer,
	.probe_chip	= intel_gmbus_probe_chip,
	.set_bus_speed	= intel_gmbus_set_bus_speed,
};

static const struct udevice_id intel_gmbus_ids[] = {
    { .compatible = "intel,gmbus"},
    { }
};

U_BOOT_DRIVER(intel_gmbus) = {
	.name	= "gmbus_intel",
	.id	= UCLASS_I2C,
	.of_match = intel_gmbus_ids,
	.ops	= &intel_gmbus_ops,
	.priv_auto_alloc_size = sizeof(struct intel_gmbus),
	.bind	= intel_gmbus_bind,
	.probe	= intel_gmbus_probe,
};

static struct pci_device_id intel_gmbus_pci_supported[] = {
	/* Intel BayTrail GMBus on the PCI bus */
	{ PCI_VDEVICE(INTEL, 0x0f31) },
	{},
};

U_BOOT_PCI_DEVICE(intel_gmbus, intel_gmbus_pci_supported);

