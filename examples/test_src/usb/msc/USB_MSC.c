
#include "USB_MSC.h"

#ifdef USB_CLASS_MSC
#include "../USB_EP.h"
#include "../USB_CTL.h"
#include "SCSI.h"

#include <string.h>


/*
 * PRIVATE DEFINITIONS
 */

#define MSC_PACKET_SIZE						USB_PACKET_SIZE

#define MSC_CBW_SIGNATURE             		0x43425355U
#define MSC_CSW_SIGNATURE             		0x53425355U
#define MSC_CBW_LENGTH                		31
#define MSC_CSW_LENGTH                		13
#define MSC_MAX_DATA                  		256

#define MSC_STATUS_NORMAL             		0
#define MSC_STATUS_RECOVERY           		1
#define MSC_STATUS_ERROR              		2

#define MSC_LUN_COUNT						1
#define MSC_MAX_LUN							(MSC_LUN_COUNT-1)

#define MSC_REQ_GET_MAX_LUN              	0xFE
#define MSC_REQ_RESET                    	0xFF

#define MSC_CSW_CMD_PASSED             		0x00
#define MSC_CSW_CMD_FAILED             		0x01

/*
 * PRIVATE TYPES
 */

/*
 * PRIVATE PROTOTYPES
 */

static void USB_MSC_Reset(void);
static void USB_MSC_TransmitDone(uint32_t size);
static void USB_MSC_Receive(uint32_t size);

static void USB_MSC_HandleCBW(uint32_t size);
static void USB_MSC_SendData(uint8_t *pbuf, uint16_t len);
static void USB_MSC_Abort(void);
static void USB_MSC_HandleTransfer(SCSI_State_t state);

static void USB_MSC_SendCSW(uint8_t CSW_Status);

/*
 * PRIVATE VARIABLES
 */

__ALIGNED(4) const uint8_t cUSB_MSC_ConfigDescriptor[USB_MSC_CONFIG_DESC_SIZE] =
{
	USB_DESCR_BLOCK_CONFIGURATION(
			USB_MSC_CONFIG_DESC_SIZE,
			0x01, // 1 interfaces available
			0x01
			),
	USB_DESCR_BLOCK_INTERFACE(
			0x00,
			0x02, // 2 endpoints used
			0x08, // Mass Storage Class
			0x06, // SCSI transparent
			0x50 // Unknown protocol
	),
	USB_DESCR_BLOCK_ENDPOINT( MSC_IN_EP, 0x02, MSC_PACKET_SIZE, 0x00 ),
	USB_DESCR_BLOCK_ENDPOINT( MSC_OUT_EP, 0x02, MSC_PACKET_SIZE, 0x00 ),
};

static struct {
	SCSI_t scsi;
	uint8_t status;
	int8_t state;

	SCSI_CBW_t cbw;
	SCSI_CSW_t csw;

	const USB_Storage_t * storage;
} gMSC;

/*
 * PUBLIC FUNCTIONS
 */

void USB_MSC_Mount(const USB_Storage_t * storage)
{
	gMSC.storage = storage;
}

void USB_MSC_Init(uint8_t config)
{
	// Data endpoints
	USB_EP_Open(MSC_IN_EP, USB_EP_TYPE_BULK, MSC_PACKET_SIZE, USB_MSC_TransmitDone);
	USB_EP_Open(MSC_OUT_EP, USB_EP_TYPE_BULK, MSC_PACKET_SIZE, USB_MSC_Receive);

	gMSC.state = SCSI_Init(&gMSC.scsi, gMSC.storage);
	gMSC.status = MSC_STATUS_NORMAL;

	USB_EP_Read(MSC_OUT_EP, (uint8_t *)&gMSC.cbw, MSC_CBW_LENGTH);
}

void USB_MSC_Deinit(void)
{
	USB_EP_Close(MSC_IN_EP);
	USB_EP_Close(MSC_OUT_EP);
}

void USB_MSC_Setup(USB_SetupRequest_t * req)
{
	switch (req->bRequest)
	{
	case MSC_REQ_GET_MAX_LUN:
	  if (req->wValue == 0 && req->wLength == 1 && req->bmRequest & 0x80)
	  {
		  uint8_t max_lun = MSC_MAX_LUN;
		  USB_CTL_Send(&max_lun, sizeof(max_lun));
		  return;
	  }
	  break;
	case MSC_REQ_RESET:
	  if (req->wValue == 0U && req->wLength == 0 && !(req->bmRequest & 0x80U))
	  {
		  USB_MSC_Reset();
		  return;
	  }
	  break;
	}
}

/*
 * PRIVATE FUNCTIONS
 */

static void USB_MSC_Reset(void)
{
	gMSC.status = MSC_STATUS_RECOVERY;
	USB_EP_Read(MSC_OUT_EP, (uint8_t *)&gMSC.cbw, MSC_CBW_LENGTH);
}

void USB_MSC_TransmitDone(uint32_t size)
{
	switch (gMSC.state)
	{
	case SCSI_State_DataIn:
	case SCSI_State_SendData:
	case SCSI_State_LastDataIn:
		gMSC.state = SCSI_ResumeCmd(&gMSC.scsi, gMSC.state);
		USB_MSC_HandleTransfer(gMSC.state);
		break;
	}
}

void USB_MSC_Receive(uint32_t size)
{
	switch (gMSC.state)
	{
	case SCSI_State_DataOut:
		gMSC.state = SCSI_ResumeCmd(&gMSC.scsi, gMSC.state);
		USB_MSC_HandleTransfer(gMSC.state);
		break;

	default:
		USB_MSC_HandleCBW(size);
		break;
	}
}

// Handle a new CBW message.
static void  USB_MSC_HandleCBW(uint32_t size)
{
	gMSC.csw.dTag = gMSC.cbw.dTag;
	gMSC.csw.dDataResidue = gMSC.cbw.dDataLength;

	if ((size != MSC_CBW_LENGTH) ||
	  (gMSC.cbw.dSignature != MSC_CBW_SIGNATURE) ||
	  (gMSC.cbw.bLUN > 0) ||
	  (gMSC.cbw.bCBLength < 1) || (gMSC.cbw.bCBLength > 16))
	{
		// ST had a SCSI_SenseCode here. Seems redundant.

		gMSC.state = SCSI_State_Error;
		gMSC.status = MSC_STATUS_ERROR;
		USB_MSC_Abort();
	}
	else
	{
		gMSC.state = SCSI_ProcessCmd(&gMSC.scsi, &gMSC.cbw);
		USB_MSC_HandleTransfer(gMSC.state);
	}
}

// Does the IO for the SCSI state machine
static void USB_MSC_HandleTransfer(SCSI_State_t state)
{
	switch(state)
	{
	case SCSI_State_Error:
		USB_MSC_SendCSW(MSC_CSW_CMD_FAILED);
		break;
	case SCSI_State_Ok:
		USB_MSC_SendCSW(MSC_CSW_CMD_PASSED);
		break;
	case SCSI_State_SendData:
		USB_MSC_SendData(gMSC.scsi.bfr, gMSC.scsi.data_len);
		break;
	case SCSI_State_DataOut:
		USB_EP_Read(MSC_OUT_EP, gMSC.scsi.bfr, gMSC.scsi.data_len);
		gMSC.csw.dDataResidue -= gMSC.scsi.data_len;
		break;
	case SCSI_State_DataIn:
	case SCSI_State_LastDataIn:
		USB_EP_Write(MSC_IN_EP, gMSC.scsi.bfr, gMSC.scsi.data_len);
		gMSC.csw.dDataResidue -= gMSC.scsi.data_len;
		break;
	default:
		break;
	}
}

static void  USB_MSC_SendData(uint8_t *pbuf, uint16_t len)
{
	uint16_t length = (uint16_t)MIN(gMSC.cbw.dDataLength, len);

	gMSC.csw.dDataResidue -= len;
	gMSC.csw.bStatus = MSC_CSW_CMD_PASSED;

	USB_EP_Write( MSC_IN_EP, pbuf, length );
}

static void  USB_MSC_SendCSW(uint8_t CSW_Status)
{
	gMSC.csw.dSignature = MSC_CSW_SIGNATURE;
	gMSC.csw.bStatus = CSW_Status;

	USB_EP_Write(MSC_IN_EP, (uint8_t *)&gMSC.csw, MSC_CSW_LENGTH);
	// Recieve next CBW
	USB_EP_Read(MSC_OUT_EP, (uint8_t *)&gMSC.cbw, MSC_CBW_LENGTH);
}

// Abort the current transaction
static void USB_MSC_Abort(void)
{
	if ((gMSC.cbw.bmFlags == 0) &&
	  (gMSC.cbw.dDataLength != 0) &&
	  (gMSC.status == MSC_STATUS_NORMAL))
	{
		USB_EP_Stall(MSC_OUT_EP);
	}

	USB_EP_Stall(MSC_IN_EP);

	if (gMSC.status == MSC_STATUS_ERROR)
	{
		USB_EP_Read(MSC_OUT_EP, (uint8_t *)&gMSC.cbw, MSC_CBW_LENGTH);
	}
}

// Completes the USB Clear feature request.
// Should be done on an interface clear feature request (I think)
void  MSC_BOT_CplClrFeature(uint8_t epnum)
{
	if (gMSC.status == MSC_STATUS_ERROR)
	{
		USB_EP_Stall(MSC_IN_EP);
		gMSC.status = MSC_STATUS_NORMAL;
	}
	else if (((epnum & 0x80U) == 0x80U) && (gMSC.status != MSC_STATUS_RECOVERY))
	{
		USB_MSC_SendCSW(MSC_CSW_CMD_FAILED);
	}
}


#endif //USB_CLASS_MSC

