#include <linux/init.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/ath9k_platform.h>
#include <linux/etherdevice.h>
#include <linux/ar8216_platform.h>
#include <linux/rle.h>
#include <linux/routerboot.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/gpio.h>

#include <asm/mach-ath79/irq.h>
#include <asm/mach-ath79/ath79.h>
#include <asm/mach-ath79/ar71xx_regs.h>

#include "common.h"
#include "dev-ap9x-pci.h"
#include "dev-eth.h"
#include "dev-spi.h"
#include "dev-gpio-buttons.h"
#include "dev-leds-gpio.h"
#include "dev-m25p80.h"
#include "dev-usb.h"
#include "dev-wmac.h"
#include "machtypes.h"
#include "routerboot.h"

#define RB_ROUTERBOOT_OFFSET    0x0000
#define RB_ROUTERBOOT_SIZE      0xe000
#define RB_HARD_CFG_OFFSET      0xe000
#define RB_HARD_CFG_SIZE        0x1000

#define RB_ART_SIZE             0x10000

#define RB_MAP_KEYS_POLL_INTERVAL 20 /* msecs */
#define RB_MAP_KEYS_DEBOUNCE_INTERVAL (3 * RB_MAP_KEYS_POLL_INTERVAL)

static struct gpio_led rb_map_leds_gpio[] __initdata = {
        {
                .name           = "rb:user",
                .gpio           = 21,
                .active_low     = 1,
        }, {
                .name           = "rb:led:eth1",
                .gpio           = 18,
                .active_low     = 1,
        }, {
                .name           = "rb:led:eth2",
                .gpio           = 20,
                .active_low     = 1,
        }, {
                .name           = "rb:led:poe",
                .gpio           = 19,
                .active_low     = 1,
        //.default_state  = LEDS_GPIO_DEFSTATE_ON
        }, {
                .name           = "rb:led:wlan",
                .gpio           = 22,
                .active_low     = 1,
        }, {
                .name           = "rb:led5",
                .gpio           = 23,
                .active_low     = 1,
        }
};

static struct gpio_keys_button rb_map_gpio_keys[] __initdata = {
	{
		.desc = "mode",
		.type = EV_KEY,
		.code = BTN_0,
		.debounce_interval = RB_MAP_KEYS_DEBOUNCE_INTERVAL,
		.gpio = 6,
		.active_low = 1,
	}
};

static struct mtd_partition rbmap_spi_partitions[] = {
        {
                .name = "RouterBoot",
                .offset = 0,
                .size = 128 * 1024,
                .mask_flags = MTD_WRITEABLE,
        },
        {
                .name = "kernel",
                .offset = 0xa00000,
                .size = MTDPART_SIZ_FULL
        },
        {
                .name = "rootfs",
                .offset = 128 * 1024,
                .size = 0x9e0000
        }
};

static struct flash_platform_data rbmap_spi_flash_data = {
        .parts          = rbmap_spi_partitions,
        .nr_parts       = ARRAY_SIZE(rbmap_spi_partitions),
};

static void __init rbmap_wlan_init(void)
{
        u8 *hard_cfg = (u8 *) KSEG1ADDR(0x1f000000 + RB_HARD_CFG_OFFSET);
        u16 tag_len;
        u8 *tag;
        char *art_buf;
        u8 wlan_mac[ETH_ALEN];
        int err;

        err = routerboot_find_tag(hard_cfg, RB_HARD_CFG_SIZE, RB_ID_WLAN_DATA,
                                  &tag, &tag_len);
        if (err) {
                pr_err("no calibration data found\n");
                return;
        }

        art_buf = kmalloc(RB_ART_SIZE, GFP_KERNEL);
        if (art_buf == NULL) {
                pr_err("no memory for calibration data\n");
                return;
        }

        err = rle_decode((char *) tag, tag_len, art_buf, RB_ART_SIZE,
                         NULL, NULL);
        if (err) {
                pr_err("unable to decode calibration data\n");
                goto free;
        }

        ath79_init_mac(wlan_mac, ath79_mac_base, 11);
        ath79_register_wmac(art_buf + 0x1000, wlan_mac);

free:
        kfree(art_buf);
}

static void __init rbmap_setup(void)
{
    	ath79_register_leds_gpio(-1, ARRAY_SIZE(rb_map_leds_gpio), rb_map_leds_gpio);
	ath79_register_gpio_keys_polled(-1, RB_MAP_KEYS_POLL_INTERVAL, ARRAY_SIZE(rb_map_gpio_keys), rb_map_gpio_keys);

	ath79_register_m25p80(&rbmap_spi_flash_data);

	/* disable PHY_SWAP and PHY_ADDR_SWAP bits */
	ath79_setup_ar933x_phy4_switch(true, true);

	ath79_register_mdio(0, 0x0);

	ath79_init_mac(ath79_eth0_data.mac_addr, ath79_mac_base, 0);
	ath79_register_eth(0);

	ath79_init_mac(ath79_eth1_data.mac_addr, ath79_mac_base, 1);
	ath79_register_eth(1);
	
        ath79_register_usb();

        rbmap_wlan_init();

    	gpio_request_one(26, GPIOF_OUT_INIT_HIGH | GPIOF_EXPORT_DIR_CHANGEABLE, "rb:poe");
}

MIPS_MACHINE(ATH79_MACH_RB_MAP, "mAP", "Mikrotik mAP2n",
             rbmap_setup);

MIPS_MACHINE(ATH79_MACH_RB_CAP, "cm2n", "Mikrotik cAP2n",
             rbmap_setup);
