#include <zephyr/usb/usbd.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(doom_usb, LOG_LEVEL_INF);

USBD_DEVICE_DEFINE(doom_usbd,
                   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
                   0x2fe3, 0x0001);

USBD_DESC_LANG_DEFINE(doom_lang);
USBD_DESC_MANUFACTURER_DEFINE(doom_mfr, "zephyr-doom");
USBD_DESC_PRODUCT_DEFINE(doom_product, "zephyr-doom net");
USBD_DESC_CONFIG_DEFINE(doom_fs_desc, "FS Config");

USBD_CONFIGURATION_DEFINE(doom_fs_config, USB_SCD_SELF_POWERED, 125, &doom_fs_desc);

int N_usb_net_init(void)
{
    int err;

    err = usbd_add_descriptor(&doom_usbd, &doom_lang);
    if (!err) {
        err = usbd_add_descriptor(&doom_usbd, &doom_mfr);
    }
    if (!err) {
        err = usbd_add_descriptor(&doom_usbd, &doom_product);
    }
    if (!err) {
        err = usbd_add_configuration(&doom_usbd, USBD_SPEED_FS, &doom_fs_config);
    }
    if (!err) {
        err = usbd_register_all_classes(&doom_usbd, USBD_SPEED_FS, 1, NULL);
    }
    if (err) {
        LOG_ERR("usbd setup failed: %d", err);
        return err;
    }

    /* CDC-ECM exposes an Interface Association Descriptor. */
    usbd_device_set_code_triple(&doom_usbd, USBD_SPEED_FS,
                                USB_BCC_MISCELLANEOUS, 0x02, 0x01);

    err = usbd_init(&doom_usbd);
    if (!err) {
        err = usbd_enable(&doom_usbd);
    }
    if (err) {
        LOG_ERR("usbd enable failed: %d", err);
        return err;
    }

    /* net_config runs before USB is up, so apply the static IP here. */
    struct net_if *iface = net_if_get_default();
    struct in_addr addr, netmask;

    if (iface != NULL &&
        net_addr_pton(AF_INET, CONFIG_NET_CONFIG_MY_IPV4_ADDR, &addr) == 0) {
        net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);
        if (net_addr_pton(AF_INET, CONFIG_NET_CONFIG_MY_IPV4_NETMASK,
                          &netmask) == 0) {
            net_if_ipv4_set_netmask_by_addr(iface, &addr, &netmask);
        }
        net_if_up(iface);
    }

    return 0;
}
