
#ifndef USB_DEFS_H
#define USB_DEFS_H

#include "STM32X.h"

/*
 * PUBLIC DEFINITIONS: USB CONFIGURATION
 * These may be overridden in the user Board.h
 */

#ifndef USB_VID
#define USB_VID						0x0483
#endif
#ifndef USB_PID
#define USB_PID						0x5740
#endif

#define USB_LANGID					0x0409

#ifndef USB_PRODUCT_STRING
#define USB_PRODUCT_STRING			"STM32X"
#endif
#ifndef USB_INTERFACE_STRING
#define USB_INTERFACE_STRING		"STM32X Interface"
#endif
#ifndef USB_CONFIGURATION_STRING
#define USB_CONFIGURATION_STRING	"STM32X Config"
#endif
#ifndef USB_MANUFACTURER_STRING
#define USB_MANUFACTURER_STRING		"Lambosaurus"
#endif
// Ensure that this is updated if string sizes are increased.
#ifndef USB_MAX_STRING_SIZE
#define USB_MAX_STRING_SIZE			64
#endif

#ifndef USB_MAX_POWER_MA
#define USB_MAX_POWER_MA 			100
#endif

#define USB_MAX_POWER				(USB_MAX_POWER_MA/2)

#ifndef USB_MAX_NUM_CONFIGURATION
#define USB_MAX_NUM_CONFIGURATION   1
#endif

// USB_SELF_POWERED
// TODO: handle this in device descriptor

#define USB_MAX_EP0_SIZE            64

#ifdef USB_SPEED_FULL
#define USB_PACKET_SIZE		512
#else
#define USB_PACKET_SIZE		64
#endif

#if (BTABLE_ADDRESS != 0)
#error "Btable is expected to be 0x0000"
#endif

/*
 * PUBLIC DEFINITIONS: HELPER MACROS
 */

#define SWAPBYTE(addr)       	(((uint16_t)(*(addr))) | (((uint16_t)(*((addr) + 1))) << 8))

#define LOBYTE(x)  				((uint8_t)(x))
#define HIBYTE(x)  				((uint8_t)((x) >> 8))
#define MIN(a, b)  				(((a) < (b)) ? (a) : (b))
#define MAX(a, b)  				(((a) > (b)) ? (a) : (b))

#ifndef __ALIGNED
#define __ALIGNED(n)			__attribute__((aligned(n)))
#endif

/*
 * PUBLIC DEFINITIONS: USB DESCRIPTOR DEFS
 */

#define USB_FEATURE_EP_HALT                             0x00
#define USB_FEATURE_REMOTE_WAKEUP                       0x01
#define USB_FEATURE_TEST_MODE                           0x02

#define USB_CONFIG_REMOTE_WAKEUP                        0x02
#define USB_CONFIG_SELF_POWERED                         0x01


#define  USB_LEN_DEV_QUALIFIER_DESC                     0x0A
#define  USB_LEN_DEV_DESC                               0x12
#define  USB_LEN_CFG_DESC                               0x09
#define  USB_LEN_IF_DESC                                0x09
#define  USB_LEN_EP_DESC                                0x07
#define  USB_LEN_OTG_DESC                               0x03
#define  USB_LEN_LANGID_STR_DESC                        0x04
#define  USB_LEN_OTHER_SPEED_DESC_SIZ                   0x09

#define  USB_IDX_LANGID_STR                             0x00
#define  USB_IDX_MFC_STR                                0x01
#define  USB_IDX_PRODUCT_STR                            0x02
#define  USB_IDX_SERIAL_STR                             0x03
#define  USB_IDX_CONFIG_STR                             0x04
#define  USB_IDX_INTERFACE_STR                          0x05

#define  USB_REQ_TYPE_STANDARD                          0x00
#define  USB_REQ_TYPE_CLASS                             0x20
#define  USB_REQ_TYPE_VENDOR                            0x40
#define  USB_REQ_TYPE_MASK                              0x60

#define  USB_REQ_RECIPIENT_DEVICE                       0x00
#define  USB_REQ_RECIPIENT_INTERFACE                    0x01
#define  USB_REQ_RECIPIENT_ENDPOINT                     0x02
#define  USB_REQ_RECIPIENT_MASK                         0x03

#define  USB_REQ_GET_STATUS                             0x00
#define  USB_REQ_CLEAR_FEATURE                          0x01
#define  USB_REQ_SET_FEATURE                            0x03
#define  USB_REQ_SET_ADDRESS                            0x05
#define  USB_REQ_GET_DESCRIPTOR                         0x06
#define  USB_REQ_SET_DESCRIPTOR                         0x07
#define  USB_REQ_GET_CONFIGURATION                      0x08
#define  USB_REQ_SET_CONFIGURATION                      0x09
#define  USB_REQ_GET_INTERFACE                          0x0A
#define  USB_REQ_SET_INTERFACE                          0x0B
#define  USB_REQ_SYNCH_FRAME                            0x0C

#define  USB_DESC_TYPE_DEVICE                           0x01
#define  USB_DESC_TYPE_CONFIGURATION                    0x02
#define  USB_DESC_TYPE_STRING                           0x03
#define  USB_DESC_TYPE_INTERFACE                        0x04
#define  USB_DESC_TYPE_ENDPOINT                         0x05
#define  USB_DESC_TYPE_DEVICE_QUALIFIER                 0x06
#define  USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION        0x07
#define  USB_DESC_TYPE_INTERFACE_ASSOCIATION			0x0B
#define  USB_DESC_TYPE_BOS                              0x0F


/*
 * PUBLIC DEFINITIONS: USB DESCRIPTOR BLOCKS
 */

/*
 * Descriptor block for describing endpoints
 *
 * bLength: Endpoint descriptor size
 * bDescriptorType: Endpoint
 * bEndpointAddress
 * bmAttributes: (0x02 for bulk)
 * wMaxPacketSize:
 * bInterval: (0 for for Bulk transfer)
 */

#define USB_DESCR_BLOCK_ENDPOINT(bEndpointAddress, bmAttributes, wMaxPacketSize, bInterval) \
	0x07,						\
	USB_DESC_TYPE_ENDPOINT,		\
	bEndpointAddress,			\
	bmAttributes,				\
	LOBYTE(wMaxPacketSize),		\
	HIBYTE(wMaxPacketSize),		\
	bInterval


/*
 * Descriptor block for describing interfaces
 *
 * bLength: Interface descriptor size
 * bDescriptorType: Interface
 * bInterfaceNumber: Number of Interface
 * bAlternateSetting: Alternate setting
 * bNumEndpoints:
 * bInterfaceClass:
 * bInterfaceSubClass:
 * bInterfaceProtocol:
 * iInterface: (0 for no string)
 */

#define USB_DESCR_BLOCK_INTERFACE(bInterfaceNumber, bNumEndpoints, bInterfaceClass, bInterfaceSubClass, bInterfaceProtocol)	\
	0x09,						\
	USB_DESC_TYPE_INTERFACE,	\
	bInterfaceNumber,			\
	0x00,						\
	bNumEndpoints,				\
	bInterfaceClass,			\
	bInterfaceSubClass,			\
	bInterfaceProtocol,			\
	0x00

/*
 * Descriptor block for describing configurations
 *
 * bLength: Configuration descriptor size
 * bDescriptorType: Configuration
 * wTotalLength: Total number of bytes in the configuration
 * bNumInterfaces:
 * bConfigurationValue:
 * iConfiguration: (0x00 for no string)
 * bmAttributes: 0xC0 for self powered
 * bMaxPower:
 */

#define USB_DESCR_BLOCK_CONFIGURATION(wTotalLength, bNumInterfaces, bConfigurationValue) \
	0x09,						\
	USB_DESC_TYPE_CONFIGURATION,\
	LOBYTE(wTotalLength),		\
	HIBYTE(wTotalLength),		\
	bNumInterfaces,				\
	bConfigurationValue,		\
	USB_IDX_CONFIG_STR,			\
	0xC0,						\
	USB_MAX_POWER


/*
 * Descriptor block for describing interface associations (IAD)
 *  bLength: Interface association descriptor size
 *  bDescriptorType: IAD
 *  bFirstInterface: First interface in the group
 *  bInterfaceCount: Number of interfaces in the group
 *  bFunctionClass:
 *  bFunctionSubClass:
 *  bFunctionProtocol:
 *  iFunction: (0x00 for no string)
 */
#define USB_DESC_BLOCK_INTERFACE_ASSOCIATION(bFirstInterface, bInterfaceCount, bFunctionClass, bFunctionSubClass, bFunctionProtocol) \
	0x08,								\
	USB_DESC_TYPE_INTERFACE_ASSOCIATION,\
	bFirstInterface,					\
	bInterfaceCount,					\
	bFunctionClass,						\
	bFunctionSubClass,					\
	bFunctionProtocol,					\
	0x00


/*
 * 	PUBLIC TYPES
 */

typedef struct usb_setup_req
{
  uint8_t   bmRequest;
  uint8_t   bRequest;
  uint16_t  wValue;
  uint16_t  wIndex;
  uint16_t  wLength;
} USB_SetupRequest_t;


#endif // USB_DEFS_H
