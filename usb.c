#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>

#include <FreeRTOS.h>

#include "cmd.h"


/*
Endpoint Address
Bits 0..3b Endpoint Number.
Bits 4..6b Reserved. Set to Zero
Bits 7 Direction 0 = In, 1 = Out
*/

#define USB_DATAR1_END_ADDR 0x01
#define USB_DATAW1_END_ADDR 0x82
#define USB_COM1_END_ADDR   0x83

#define USB_DATAR2_END_ADDR 0x04
#define USB_DATAW2_END_ADDR 0x85
#define USB_COM2_END_ADDR   0x86

#define USB_DATAR3_END_ADDR 0x07
#define USB_DATAW3_END_ADDR 0x88
#define USB_COM3_END_ADDR   0x89


#define USB_DATA_PCK_SZ    64
#define USB_COM_PCK_SZ     16

static const char *usb_strings[] = { "Devtank Ltd", "IO Board", "Prototype" };

static uint8_t usbd_control_buffer[128];

static usbd_device *usbd_dev = NULL;


static struct
{
    uint8_t r_addr;
    uint8_t w_addr;
    uint8_t com_addr;
} end_addrs[3] = { { USB_DATAR1_END_ADDR,
                     USB_DATAW1_END_ADDR,
                     USB_COM1_END_ADDR},
                   { USB_DATAR2_END_ADDR,
                     USB_DATAW2_END_ADDR,
                     USB_COM2_END_ADDR},
                   { USB_DATAR3_END_ADDR,
                     USB_DATAW3_END_ADDR,
                     USB_COM3_END_ADDR},
                 };


static const struct usb_device_descriptor usb_dev_desc =
{
    .bLength            = USB_DT_DEVICE_SIZE,
    .bDescriptorType    = USB_DT_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = USB_CLASS_CDC,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = USB_DATA_PCK_SZ,
    .idVendor           = 0x0483,
    .idProduct          = 0x5740,
    .bcdDevice          = 0x0200,
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1,
};



static const struct usb_endpoint_descriptor usb_endp[] =
{
    {
        .bLength          = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType  = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_COM1_END_ADDR,
        .bmAttributes     = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize   = USB_COM_PCK_SZ,
        .bInterval        = 255,
    },
    {
        .bLength          = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType  = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_DATAR1_END_ADDR,
        .bmAttributes     = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize   = USB_DATA_PCK_SZ,
        .bInterval        = 1,
    },
    {
        .bLength          = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType  = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_DATAW1_END_ADDR,
        .bmAttributes     = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize   = USB_DATA_PCK_SZ,
        .bInterval        = 1,
    },
    {
        .bLength          = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType  = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_COM2_END_ADDR,
        .bmAttributes     = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize   = USB_COM_PCK_SZ,
        .bInterval        = 255,
    },
    {
        .bLength          = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType  = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_DATAR2_END_ADDR,
        .bmAttributes     = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize   = USB_DATA_PCK_SZ,
        .bInterval        = 1,
    },
    {
        .bLength          = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType  = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_DATAW2_END_ADDR,
        .bmAttributes     = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize   = USB_DATA_PCK_SZ,
        .bInterval        = 1,
    },
    {
        .bLength          = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType  = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_COM3_END_ADDR,
        .bmAttributes     = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize   = USB_COM_PCK_SZ,
        .bInterval        = 255,
    },
    {
        .bLength          = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType  = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_DATAR3_END_ADDR,
        .bmAttributes     = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize   = USB_DATA_PCK_SZ,
        .bInterval        = 1,
    },
    {
        .bLength          = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType  = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_DATAW3_END_ADDR,
        .bmAttributes     = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize   = USB_DATA_PCK_SZ,
        .bInterval        = 1,
    }
};



static const struct
{
    struct usb_cdc_header_descriptor          header_desc;
    struct usb_cdc_call_management_descriptor call_management_desc;
    struct usb_cdc_acm_descriptor             acm_desc;
    struct usb_cdc_union_descriptor           cdc_union_desc;
} __attribute__((packed)) usb_functional_descriptors =
{
    .header_desc =
    {
        .bFunctionLength    = sizeof(struct usb_cdc_header_descriptor),
        .bDescriptorType    = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_HEADER,
        .bcdCDC             = 0x0110,
    },
    .call_management_desc =
    {
        .bFunctionLength    = sizeof(struct usb_cdc_call_management_descriptor),
        .bDescriptorType    = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
        .bmCapabilities     = 0,
        .bDataInterface     = 1,
    },
    .acm_desc =
    {
        .bFunctionLength    = sizeof(struct usb_cdc_acm_descriptor),
        .bDescriptorType    = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_ACM,
        .bmCapabilities     = 0,
    },
    .cdc_union_desc =
    {
        .bFunctionLength        = sizeof(struct usb_cdc_union_descriptor),
        .bDescriptorType        = CS_INTERFACE,
        .bDescriptorSubtype     = USB_CDC_TYPE_UNION,
        .bControlInterface      = 0,
        .bSubordinateInterface0 = 1,
    },
};


static const struct usb_interface_descriptor usb_ifaces_desc[] =
{
    {
        .bLength            = USB_DT_INTERFACE_SIZE,
        .bDescriptorType    = USB_DT_INTERFACE,
        .bInterfaceNumber   = 0,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = USB_CLASS_CDC,
        .bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
        .bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
        .iInterface         = 0,
        .endpoint           = &usb_endp[0],
        .extra              = &usb_functional_descriptors,
        .extralen           = sizeof(usb_functional_descriptors),
    },
    {
        .bLength            = USB_DT_INTERFACE_SIZE,
        .bDescriptorType    = USB_DT_INTERFACE,
        .bInterfaceNumber   = 1,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 2,
        .bInterfaceClass    = USB_CLASS_DATA,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = 0,
        .endpoint           = &usb_endp[1],
    },
    {
        .bLength            = USB_DT_INTERFACE_SIZE,
        .bDescriptorType    = USB_DT_INTERFACE,
        .bInterfaceNumber   = 0,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = USB_CLASS_CDC,
        .bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
        .bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
        .iInterface         = 0,
        .endpoint           = &usb_endp[3],
        .extra              = &usb_functional_descriptors,
        .extralen           = sizeof(usb_functional_descriptors),
    },
    {
        .bLength            = USB_DT_INTERFACE_SIZE,
        .bDescriptorType    = USB_DT_INTERFACE,
        .bInterfaceNumber   = 1,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 2,
        .bInterfaceClass    = USB_CLASS_DATA,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = 0,
        .endpoint           = &usb_endp[4],
    },
    {
        .bLength            = USB_DT_INTERFACE_SIZE,
        .bDescriptorType    = USB_DT_INTERFACE,
        .bInterfaceNumber   = 0,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = USB_CLASS_CDC,
        .bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
        .bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
        .iInterface         = 0,
        .endpoint           = &usb_endp[6],
        .extra              = &usb_functional_descriptors,
        .extralen           = sizeof(usb_functional_descriptors),
    },
    {
        .bLength            = USB_DT_INTERFACE_SIZE,
        .bDescriptorType    = USB_DT_INTERFACE,
        .bInterfaceNumber   = 1,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 2,
        .bInterfaceClass    = USB_CLASS_DATA,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = 0,
        .endpoint           = &usb_endp[7],
    }
};



static const struct usb_interface usb_ifaces[] =
{
    {
        .altsetting     = &usb_ifaces_desc[0],
        .num_altsetting = 1,
    },
    {
        .altsetting     = &usb_ifaces_desc[1],
        .num_altsetting = 1,
    },
    {
        .altsetting     = &usb_ifaces_desc[2],
        .num_altsetting = 1,
    },
    {
        .altsetting     = &usb_ifaces_desc[3],
        .num_altsetting = 1,
    },
    {
        .altsetting     = &usb_ifaces_desc[4],
        .num_altsetting = 1,
    },
    {
        .altsetting     = &usb_ifaces_desc[5],
        .num_altsetting = 1,
    },
};


static const struct usb_config_descriptor usb_config =
{
    .bLength             = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType     = USB_DT_CONFIGURATION,
    .wTotalLength        = 0,
    .bNumInterfaces      = 6,
    .bConfigurationValue = 1,
    .iConfiguration      = 0,
    .bmAttributes        = USB_CONFIG_ATTR_DEFAULT,
    .bMaxPower           = 50,
    .interface           = usb_ifaces,
};


static enum usbd_request_return_codes usb_control_request(usbd_device                    *_   __attribute((unused)),
                                                          struct usb_setup_data          *req,
                                                          uint8_t                       **__  __attribute((unused)),
                                                          uint16_t                       *len,
                                                          usbd_control_complete_callback *___ __attribute((unused)))
{
    switch (req->bRequest)
    {
        case USB_CDC_REQ_SET_CONTROL_LINE_STATE:
            return USBD_REQ_HANDLED;
        case USB_CDC_REQ_SET_LINE_CODING:
            if (*len < sizeof(struct usb_cdc_line_coding))
                return USBD_REQ_NOTSUPP;
            return USBD_REQ_HANDLED;
    }

    return USBD_REQ_NOTSUPP;
}


static void usb_data_rx_cb(usbd_device *_ __attribute((unused)), uint8_t end_point)
{
    char buf[USB_DATA_PCK_SZ+1];
    int len = usbd_ep_read_packet(usbd_dev, end_point, buf, USB_DATA_PCK_SZ);

    if (!len)
        return;
}


static void usb_set_config_cb(usbd_device *usb_dev,
                              uint16_t     wValue __attribute((unused)))
{
    for(unsigned n = 0; n < 3; n++)
    {
        usbd_ep_setup(usb_dev, end_addrs[n].com_addr,   USB_ENDPOINT_ATTR_INTERRUPT, USB_COM_PCK_SZ, NULL);
        usbd_ep_setup(usb_dev, end_addrs[n].r_addr, USB_ENDPOINT_ATTR_BULK,      USB_DATA_PCK_SZ, usb_data_rx_cb);
        usbd_ep_setup(usb_dev, end_addrs[n].w_addr, USB_ENDPOINT_ATTR_BULK,      USB_DATA_PCK_SZ, NULL);
    }

    usbd_register_control_callback(
                usb_dev,
                USB_REQ_TYPE_CLASS |
                USB_REQ_TYPE_INTERFACE,
                USB_REQ_TYPE_TYPE |
                USB_REQ_TYPE_RECIPIENT,
                usb_control_request);
}


void usb_init()
{
    usbd_dev = usbd_init(&st_usbfs_v1_usb_driver,
                         &usb_dev_desc,
                         &usb_config,
                         usb_strings, 3,
                         usbd_control_buffer, sizeof(usbd_control_buffer));

    usbd_register_set_config_callback(usbd_dev, usb_set_config_cb);

    nvic_set_priority(NVIC_USB_LP_IRQ, USB_PRIORITY);
    nvic_enable_irq(NVIC_USB_LP_IRQ);
}


void usb_lp_isr()
{
    usbd_poll(usbd_dev);
}


void raw_usb_send(unsigned uart, void * data, unsigned len)
{
    uint8_t w_addr = end_addrs[uart].w_addr;

    if (len > USB_DATA_PCK_SZ)
    {
        for(uint8_t * pos = (uint8_t*)data, * end = pos + len; pos < end;)
        {
            unsigned bytes_left = ((uintptr_t)end) - ((uintptr_t)pos);
            if (bytes_left > USB_DATA_PCK_SZ)
            {
                while( usbd_ep_write_packet(usbd_dev, w_addr, pos, USB_DATA_PCK_SZ) == 0)
                    usbd_poll(usbd_dev);
                pos += USB_DATA_PCK_SZ;
            }
            else
            {
                while( usbd_ep_write_packet(usbd_dev, w_addr, pos, bytes_left) == 0)
                    usbd_poll(usbd_dev);
                break;
            }
            usbd_poll(usbd_dev);
        }
    }
    else while( usbd_ep_write_packet(usbd_dev, w_addr, data, len) == 0)
        usbd_poll(usbd_dev);
}
