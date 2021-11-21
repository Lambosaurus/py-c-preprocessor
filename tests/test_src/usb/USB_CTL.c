
#include "USB_CTL.h"
#ifdef USB_ENABLE

#include "USB_Defs.h"
#include "USB_Class.h"
#include "USB_EP.h"
#include "USB_PCD.h"


/*
 * PRIVATE DEFINITIONS
 */

#ifndef USB_CLASS_CLASSID
// Most classes are Interface defined, and so these should all be zero.
#define USB_CLASS_CLASSID				0x00
#define USB_CLASS_SUBCLASSID			0x00
#define USB_CLASS_PROTOCOLID			0x00
#endif

#define CTL_IN_EP		0x80
#define CTL_OUT_EP		0x00

#define CTL_EP_SIZE		USB_MAX_EP0_SIZE

typedef enum {
	USB_STATE_DEFAULT,
	USB_STATE_ADDRESSED,
	USB_STATE_CONFIGURED,
	USB_STATE_SUSPENDED
} USB_State_t;

typedef enum {
	CTL_STATE_IDLE,
	CTL_STATE_SETUP,
	CTL_STATE_DATA_IN,
	CTL_STATE_DATA_OUT,
	CTL_STATE_STATUS_IN,
	CTL_STATE_STATUS_OUT,
	CTL_STATE_STALL,
} CTL_State_t;

/*
 * PRIVATE TYPES
 */

/*
 * PUBLIC TYPES
 */

/*
 * PRIVATE PROTOTYPES
 */

static void USB_CTL_EndpointRequest(USB_SetupRequest_t  *req);
static void USB_CTL_DeviceRequest(USB_SetupRequest_t *req);
static void USB_CTL_InterfaceRequest(USB_SetupRequest_t  *req);

static void USB_CTL_SetAddress(USB_SetupRequest_t * req);
static void USB_CTL_SetFeature(USB_SetupRequest_t *req);
static void USB_CTL_ClearFeature(USB_SetupRequest_t *req);
static void USB_CTL_GetConfig(USB_SetupRequest_t *req);
static void USB_CTL_SetConfig(USB_SetupRequest_t *req);
static void USB_CTL_GetStatus(USB_SetupRequest_t *req);
static void USB_CTL_GetDescriptor(USB_SetupRequest_t *req);

static uint16_t USB_CTL_GetLangIdDescriptor(uint8_t * data);
static uint16_t USB_CTL_GetStrDescriptor(uint8_t * data, const char * str);
static uint16_t USB_CTL_GetSerialDescriptor(uint8_t * data);

static void USB_CTL_SendStatus(void);
static void USB_CTL_ReceiveStatus(void);
static void USB_CTL_Error(void);

static void USB_CTL_DataOut(uint32_t count);
static void USB_CTL_DataIn(uint32_t count);


/*
 * PRIVATE VARIABLES
 */

#define CTL_BUFFER_SIZE		(MAX((USB_MAX_STRING_SIZE+1)*2, CTL_EP_SIZE))

static struct {
	uint8_t address;
	uint8_t class_config;
	uint8_t usb_state;
	bool remote_wakeup;
	uint8_t ctl_state;
	uint16_t ctl_len;
	uint8_t buffer[CTL_BUFFER_SIZE];
} gCTL;

__ALIGNED(4) const uint8_t cUsbDeviceDescriptor[USB_LEN_DEV_DESC] =
{
	USB_LEN_DEV_DESC,           // bLength
	USB_DESC_TYPE_DEVICE,       // bDescriptorType
	0x00,                       // bcdUSB
	0x02,
	USB_CLASS_CLASSID,          // bDeviceClass
	USB_CLASS_SUBCLASSID,       // bDeviceSubClass
	USB_CLASS_PROTOCOLID,       // bDeviceProtocol
	CTL_EP_SIZE,                // bMaxPacketSize
	LOBYTE(USB_VID),            // idVendor
	HIBYTE(USB_VID),            // idVendor
	LOBYTE(USB_PID),            // idProduct
	HIBYTE(USB_PID),        	// idProduct
	0x00,                       // bcdDevice rel. 2.00
	0x02,
	USB_IDX_MFC_STR,           // Index of manufacturer  string
	USB_IDX_PRODUCT_STR,       // Index of product string
	USB_IDX_SERIAL_STR,        // Index of serial number string
	USB_MAX_NUM_CONFIGURATION  // bNumConfigurations
};

/*
 * PUBLIC FUNCTIONS
 */


void USB_CTL_Init(void)
{
	USB_EP_Open(CTL_IN_EP, USB_EP_TYPE_CTRL, CTL_EP_SIZE, USB_CTL_DataIn);
	USB_EP_Open(CTL_OUT_EP, USB_EP_TYPE_CTRL, CTL_EP_SIZE, USB_CTL_DataOut);
	gCTL.address = 0;
	gCTL.class_config = 0;
	gCTL.usb_state = USB_STATE_DEFAULT;
	gCTL.remote_wakeup = false;
	gCTL.ctl_state = CTL_STATE_IDLE;
}

void USB_CTL_Deinit(void)
{
	// Dont bother closing the CTL EP's
	if (gCTL.class_config != 0)
	{
		gCTL.class_config = 0;
		USB_CLASS_DEINIT();
	}
}

void USB_CTL_HandleSetup(uint8_t * data)
{
	USB_SetupRequest_t req;
	req.bmRequest = *(data);
	req.bRequest = *(data + 1U);
	req.wValue = SWAPBYTE(data + 2U);
	req.wIndex = SWAPBYTE(data + 4U);
	req.wLength = SWAPBYTE(data + 6U);

	gCTL.ctl_state = CTL_STATE_SETUP;
	gCTL.ctl_len = req.wLength;

	switch (req.bmRequest & 0x1F)
	{
	case USB_REQ_RECIPIENT_DEVICE:
		USB_CTL_DeviceRequest(&req);
		break;
	case USB_REQ_RECIPIENT_INTERFACE:
		USB_CTL_InterfaceRequest(&req);
		break;
	case USB_REQ_RECIPIENT_ENDPOINT:
		USB_CTL_EndpointRequest(&req);
		break;
	default:
		USB_EP_Stall(req.bmRequest & 0x80);
		break;
	}
}

void USB_CTL_Send(uint8_t * data, uint16_t size)
{
	gCTL.ctl_state = CTL_STATE_DATA_IN;
	USB_EP_Write(CTL_IN_EP, data, size);
}

void USB_CTL_Receive(uint8_t * data, uint16_t size)
{
	gCTL.ctl_state = CTL_STATE_DATA_OUT;
	USB_EP_Read(CTL_OUT_EP, data, size);
}

/*
 * PRIVATE FUNCTIONS
 */


static void USB_CTL_ReceiveStatus(void)
{
	gCTL.ctl_state = CTL_STATE_STATUS_OUT;
	USB_EP_Read(CTL_OUT_EP, NULL, 0);
}

static void USB_CTL_SendStatus(void)
{
	gCTL.ctl_state = CTL_STATE_STATUS_IN;
	USB_EP_Write(0x00U, NULL, 0U);
}

static void USB_CTL_EndpointRequest(USB_SetupRequest_t  *req)
{
	uint8_t endpoint = LOBYTE(req->wIndex);

	switch (req->bmRequest & USB_REQ_TYPE_MASK)
	{
#ifdef USB_CLASS_CUSTOM_SETUP
	case USB_REQ_TYPE_CLASS:
	case USB_REQ_TYPE_VENDOR:
		USB_CLASS_SETUP(req);
		return;
#endif
	case USB_REQ_TYPE_STANDARD:
		switch (req->bRequest)
		{
		case USB_REQ_SET_FEATURE:
			switch (gCTL.usb_state)
			{
			case USB_STATE_ADDRESSED:
				if ((endpoint != CTL_OUT_EP) && (endpoint != CTL_IN_EP))
				{
					USB_EP_Stall(endpoint);
					USB_EP_Stall(CTL_IN_EP);
					return;
				}
				break;
			case USB_STATE_CONFIGURED:
				if (req->wValue == USB_FEATURE_EP_HALT)
				{
					if ((endpoint != CTL_OUT_EP) && (endpoint != CTL_IN_EP) && (req->wLength == 0x00U))
					{
						USB_EP_Stall(endpoint);
					}
				}
				USB_CTL_SendStatus();
				return;
			}
			break;
		case USB_REQ_CLEAR_FEATURE:
			switch (gCTL.usb_state)
			{
			case USB_STATE_ADDRESSED:
				if ((endpoint & 0x7FU) != 0x00U)
				{
					USB_EP_Stall(endpoint);
					USB_EP_Stall(CTL_IN_EP);
				}
				break;
			case USB_STATE_CONFIGURED:
				if (req->wValue == USB_FEATURE_EP_HALT)
				{
					if ((endpoint & 0x7FU) != 0x00U)
					{
						USB_EP_Destall(endpoint);
					}
					USB_CTL_SendStatus();
					return;
				}
				break;
			}
			break;
		case USB_REQ_GET_STATUS:
			switch (gCTL.usb_state)
			{
			case USB_STATE_ADDRESSED:
			case USB_STATE_CONFIGURED:
				if (USB_EP_IsOpen(endpoint))
				{
					uint16_t status = USB_EP_IsStalled(endpoint) ? 0x0001 : 0x0000;
					USB_CTL_Send((uint8_t *)&status, 2);
					return;
				}
				break;
			}
			break;
		}
		break;
	}
	USB_CTL_Error();
}

/*
static void USB_CTL_StandardClassRequest(USB_SetupRequest_t  *req)
{
	if (gCTL.usb_state == USB_STATE_CONFIGURED)
	{
		switch (req->bRequest)
		{
			case USB_REQ_GET_STATUS:
			{
				// status is always 0 for classes
				uint16_t status_info = 0;
				USB_CTL_Send((uint8_t *)&status_info, 2U);
				return;
			}
			case USB_REQ_GET_INTERFACE:
			{
				// Alternate interfaces not supported.
				uint8_t ifalt = 0;
				USB_CTL_Send(&ifalt, 1);
				return;
			}
			case USB_REQ_SET_INTERFACE:
				return;
		}
	}
	USB_CTL_Error();
}
*/

static void USB_CTL_DeviceRequest(USB_SetupRequest_t *req)
{
	switch (req->bmRequest & USB_REQ_TYPE_MASK)
	{
#ifdef USB_CLASS_CUSTOM_SETUP
	case USB_REQ_TYPE_CLASS:
	case USB_REQ_TYPE_VENDOR:
		USB_CLASS_SETUP(req);
		return;
#endif
	case USB_REQ_TYPE_STANDARD:
		switch (req->bRequest)
		{
		case USB_REQ_GET_DESCRIPTOR:
			USB_CTL_GetDescriptor(req);
			return;
		case USB_REQ_SET_ADDRESS:
			USB_CTL_SetAddress(req);
			return;
		case USB_REQ_SET_CONFIGURATION:
			USB_CTL_SetConfig(req);
			return;
		case USB_REQ_GET_CONFIGURATION:
			USB_CTL_GetConfig(req);
			return;
		case USB_REQ_GET_STATUS:
			USB_CTL_GetStatus(req);
			return;
		case USB_REQ_SET_FEATURE:
			USB_CTL_SetFeature(req);
			return;
		case USB_REQ_CLEAR_FEATURE:
			USB_CTL_ClearFeature(req);
			return;
		}
		break;
	}
	USB_CTL_Error();
}


static void USB_CTL_SetFeature(USB_SetupRequest_t * req)
{
	if (req->wValue == USB_FEATURE_REMOTE_WAKEUP)
	{
		// Support for issuing a remote wakeup is missing.
		// USB_CNTR_RESUME should be set when resume is requested.
		gCTL.remote_wakeup = true;
		USB_CTL_SendStatus();
	}
}

static void USB_CTL_ClearFeature(USB_SetupRequest_t * req)
{
	switch (gCTL.usb_state)
	{
	case USB_STATE_DEFAULT:
	case USB_STATE_ADDRESSED:
	case USB_STATE_CONFIGURED:
		if (req->wValue == USB_FEATURE_REMOTE_WAKEUP)
		{
			gCTL.remote_wakeup = false;
			USB_CTL_SendStatus();
			return;
		}
	}
	USB_CTL_Error();
}

static void USB_CTL_SetAddress(USB_SetupRequest_t * req)
{
	if ((req->wIndex == 0) && (req->wLength == 0) && (req->wValue < 128))
	{
		if (gCTL.usb_state != USB_STATE_CONFIGURED)
		{
			uint8_t address = (uint8_t)(req->wValue) & 0x7FU;
			gCTL.usb_state = (address != 0) ? USB_STATE_ADDRESSED : USB_STATE_DEFAULT;
			gCTL.address = address;
			USB_CTL_SendStatus();
			return;
		}
	}
	USB_CTL_Error();
}

static void USB_CTL_SetConfig(USB_SetupRequest_t * req)
{
	uint8_t config = (uint8_t)(req->wValue);
	if (config <= USB_MAX_NUM_CONFIGURATION)
	{
		if (gCTL.class_config != 0)
		{
			gCTL.class_config = 0;
			USB_CLASS_DEINIT();
		}

		switch (gCTL.usb_state)
		{
		case USB_STATE_ADDRESSED:
		case USB_STATE_CONFIGURED:
			if (config == 0)
			{
				gCTL.usb_state = USB_STATE_ADDRESSED;
			}
			else
			{
				gCTL.usb_state = USB_STATE_CONFIGURED;
				gCTL.class_config = config;
				USB_CLASS_INIT(config);
			}
			USB_CTL_SendStatus();
			return;
		}
	}
	USB_CTL_Error();
}

static void USB_CTL_GetConfig(USB_SetupRequest_t * req)
{
	if (req->wLength == 1)
	{
		switch (gCTL.usb_state)
		{
		case USB_STATE_DEFAULT:
		case USB_STATE_ADDRESSED:
		case USB_STATE_CONFIGURED:
			USB_CTL_Send(&gCTL.class_config, 1);
			return;
		}
	}
	USB_CTL_Error();
}

static void USB_CTL_GetStatus(USB_SetupRequest_t * req)
{
	switch (gCTL.usb_state)
	{
	case USB_STATE_DEFAULT:
	case USB_STATE_ADDRESSED:
	case USB_STATE_CONFIGURED:
		if (req->wLength == 0x2U)
		{
#ifdef USB_SELF_POWERED
			uint16_t status = USB_CONFIG_SELF_POWERED;
#else
			uint16_t status = 0;
#endif
			if (gCTL.remote_wakeup)
			{
				status |= USB_CONFIG_REMOTE_WAKEUP;
			}
			USB_CTL_Send((uint8_t *)&status, 2);
			return;
		}
	}
	USB_CTL_Error();
}

static void USB_CTL_InterfaceRequest(USB_SetupRequest_t * req)
{
	switch (req->bmRequest & USB_REQ_TYPE_MASK)
	{
	case USB_REQ_TYPE_CLASS:
	case USB_REQ_TYPE_VENDOR:
	case USB_REQ_TYPE_STANDARD:
		switch (gCTL.usb_state)
		{
		case USB_STATE_DEFAULT:
		case USB_STATE_ADDRESSED:
		case USB_STATE_CONFIGURED:
			if (LOBYTE(req->wIndex) < USB_INTERFACES)
			{
				USB_CLASS_SETUP(req);
				if ((req->wLength == 0U))
				{
					USB_CTL_SendStatus();
				}
				return;
			}
			break;
		}
		break;
	}
	USB_CTL_Error();
}


static void USB_CTL_GetDescriptor(USB_SetupRequest_t * req)
{
	uint16_t len = 0;
	uint8_t *data = NULL;

	switch (HIBYTE(req->wValue))
	{
#ifdef USB_USE_LPM
	case USB_DESC_TYPE_BOS:
#error "This BOS descriptor must be implemented for LPM mode"
		break;
#endif
	case USB_DESC_TYPE_DEVICE:
		data = (uint8_t *)cUsbDeviceDescriptor;
		len = sizeof(cUsbDeviceDescriptor);
		break;

	case USB_DESC_TYPE_CONFIGURATION:
		data = (uint8_t *)USB_CLASS_DEVICE_DESCRIPTOR;
		len = sizeof(USB_CLASS_DEVICE_DESCRIPTOR);
		break;

	case USB_DESC_TYPE_STRING:
		// These descriptors will be copied into the CTL buffer.
		data = gCTL.buffer;
		switch ((uint8_t)(req->wValue))
		{
		case USB_IDX_LANGID_STR:
			len = USB_CTL_GetLangIdDescriptor(data);
			break;
		case USB_IDX_MFC_STR:
			len = USB_CTL_GetStrDescriptor(data, USB_MANUFACTURER_STRING);
			break;
		case USB_IDX_PRODUCT_STR:
			len = USB_CTL_GetStrDescriptor(data, USB_PRODUCT_STRING);
			break;
		case USB_IDX_SERIAL_STR:
			len = USB_CTL_GetSerialDescriptor(data);
			break;
		case USB_IDX_CONFIG_STR:
			len = USB_CTL_GetStrDescriptor(data, USB_CONFIGURATION_STRING);
			break;
		case USB_IDX_INTERFACE_STR:
			len = USB_CTL_GetStrDescriptor(data, USB_INTERFACE_STRING);
			break;
		}
		break;

	case USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION:
	case USB_DESC_TYPE_DEVICE_QUALIFIER:
		// We do not support full speed mode, so stalling on these requests is valid
		break;
	}

	if (len == 0)
	{
		USB_CTL_Error();
	}
	else if (req->wLength == 0)
	{
		// No data was requested.
		USB_CTL_SendStatus();
	}
	else
	{
		// Note: a partial descriptor may be requested
		USB_CTL_Send(data, MIN(len, req->wLength));
	}
}

static uint16_t USB_CTL_GetStrDescriptor(uint8_t * data, const char * str)
{
	uint16_t i = 2;
	while (*str != 0)
	{
		data[i++] = *str++;
		data[i++] = 0;
	}
	data[0] = i;
	data[1] = USB_DESC_TYPE_STRING;
	return i;
}

static void USB_CTL_IntToUnicode(uint32_t value, uint8_t * data, uint32_t size)
{
	while (size--)
	{
		uint32_t v = value >> 28;
		value <<= 4;
		*data++ = (v < 0x0A) ? v + '0' : v + ('A' - 10);
		*data++ = 0;
	}
}

static uint16_t USB_CTL_GetSerialDescriptor(uint8_t * data)
{
	uint32_t s0 = *((uint32_t*)(UID_BASE + 0));
	uint32_t s1 = *((uint32_t*)(UID_BASE + 4));
	uint32_t s2 = *((uint32_t*)(UID_BASE + 8));
	s0 += s2;

	// We are slamming our 96bit (12) UID into a 6 byte string.
	// Is this a good idea??
	// One byte goes to 2 unicode characters 0x1F => { 0, '1', 0, 'F' }

	data[0] = 24 + 2;
	data[1] = USB_DESC_TYPE_STRING;
	USB_CTL_IntToUnicode(s0, data + 2, 8);
	USB_CTL_IntToUnicode(s1, data + 18, 4);
	return 24 + 2;
}

static uint16_t USB_CTL_GetLangIdDescriptor(uint8_t * data)
{
	data[0] = 4;
	data[1] = USB_DESC_TYPE_STRING;
	data[2] = LOBYTE(USB_LANGID);
	data[3] = HIBYTE(USB_LANGID);
	return 4;
}

static void USB_CTL_Error(void)
{
	USB_EP_Stall(CTL_IN_EP);
	USB_EP_Stall(CTL_OUT_EP);
}


static void USB_CTL_DataOut(uint32_t count)
{
	switch (gCTL.ctl_state)
	{
	case CTL_STATE_DATA_OUT:
#ifdef USB_CLASS_CTL_RXREADY
		if (gCTL.usb_state == USB_STATE_CONFIGURED)
		{
			USB_CLASS_CTL_RXREADY();
		}
#endif
		USB_CTL_SendStatus();
		break;
	case CTL_STATE_STATUS_OUT:
		if (gCTL.ctl_state == USB_STATE_CONFIGURED)
		{
			// STATUS PHASE completed, update ep0_state to idle
			gCTL.ctl_state = CTL_STATE_IDLE;
			USB_EP_Stall(CTL_OUT_EP);
		}
		break;
	}

	USB_EP_Read(CTL_OUT_EP, gCTL.buffer, CTL_EP_SIZE);
}

static void USB_CTL_DataIn(uint32_t count)
{
	switch (gCTL.ctl_state)
	{
	case CTL_STATE_DATA_IN:
#ifdef USB_CLASS_CTL_TXDONE
		if (gCTL.usb_state == USB_STATE_CONFIGURED)
		{
			USB_CLASS_CTL_TXDONE();
		}
#endif
		USB_EP_Stall(CTL_IN_EP);
		USB_CTL_ReceiveStatus();
		break;
	case CTL_STATE_STATUS_IN:
	case CTL_STATE_IDLE:
		USB_EP_Stall(CTL_IN_EP);
		break;
	}

	if (gCTL.address != 0)
	{
		USB_PCD_SetAddress(gCTL.address);
		gCTL.address = 0;
	}
}

/*
 * INTERRUPT ROUTINES
 */

#endif //USB_ENABLE
