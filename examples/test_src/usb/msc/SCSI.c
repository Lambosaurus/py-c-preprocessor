#include "SCSI.h"
#include "USB_MSC.h"
#include <string.h>

/*
 * PRIVATE DEFINITIONS
 */

// Response sizes
#define INQUIRY_PAGE00_LEN              			0x07
#define MODE_SENSE10_LEN                   			0x08
#define MODE_SENSE6_LEN                    			0x08
#define REQUEST_SENSE_LEN                      		0x12
#define READ_FORMAT_CAPACITY_LEN               		0x0C
#define READ_CAPACITY10_LEN                    		0x08


// SCSI Commands
#define SCSI_FORMAT_UNIT                            0x04
#define SCSI_INQUIRY                                0x12
#define SCSI_MODE_SELECT6                           0x15
#define SCSI_MODE_SELECT10                          0x55
#define SCSI_MODE_SENSE6                            0x1A
#define SCSI_MODE_SENSE10                           0x5A
#define SCSI_ALLOW_MEDIUM_REMOVAL                   0x1E
#define SCSI_READ6                                  0x08
#define SCSI_READ10                                 0x28
#define SCSI_READ12                                 0xA8
#define SCSI_READ16                                 0x88

#define SCSI_READ_CAPACITY10                        0x25
#define SCSI_READ_CAPACITY16                        0x9E

#define SCSI_REQUEST_SENSE                          0x03
#define SCSI_START_STOP_UNIT                        0x1B
#define SCSI_TEST_UNIT_READY                        0x00
#define SCSI_WRITE6                                 0x0A
#define SCSI_WRITE10                                0x2A
#define SCSI_WRITE12                                0xAA
#define SCSI_WRITE16                                0x8A

#define SCSI_VERIFY10                               0x2F
#define SCSI_VERIFY12                               0xAF
#define SCSI_VERIFY16                               0x8F

#define SCSI_SEND_DIAGNOSTIC                        0x1D
#define SCSI_READ_FORMAT_CAPACITIES                 0x23

// SCSI errors
#define SCSI_SKEY_NO_SENSE                                    0
#define SCSI_SKEY_RECOVERED_ERROR                             1
#define SCSI_SKEY_NOT_READY                                   2
#define SCSI_SKEY_MEDIUM_ERROR                                3
#define SCSI_SKEY_HARDWARE_ERROR                              4
#define SCSI_SKEY_ILLEGAL_REQUEST                             5
#define SCSI_SKEY_UNIT_ATTENTION                              6
#define SCSI_SKEY_DATA_PROTECT                                7
#define SCSI_SKEY_BLANK_CHECK                                 8
#define SCSI_SKEY_VENDOR_SPECIFIC                             9
#define SCSI_SKEY_COPY_ABORTED                                10
#define SCSI_SKEY_ABORTED_COMMAND                             11
#define SCSI_SKEY_VOLUME_OVERFLOW                             13
#define SCSI_SKEY_MISCOMPARE                                  14

#define SCSI_ASQ_INVALID_CDB                                 0x20
#define SCSI_ASQ_INVALID_FIELD_IN_COMMAND                    0x24
#define SCSI_ASQ_PARAMETER_LIST_LENGTH_ERROR                 0x1A
#define SCSI_ASQ_INVALID_FIELD_IN_PARAMETER_LIST             0x26
#define SCSI_ASQ_ADDRESS_OUT_OF_RANGE                        0x21
#define SCSI_ASQ_MEDIUM_NOT_PRESENT                          0x3A
#define SCSI_ASQ_MEDIUM_HAVE_CHANGED                         0x28
#define SCSI_ASQ_WRITE_PROTECTED                             0x27
#define SCSI_ASQ_UNRECOVERED_READ_ERROR                      0x11
#define SCSI_ASQ_WRITE_FAULT                                 0x03

/*
 * PRIVATE TYPES
 */

/*
 * PRIVATE PROTOTYPES
 */

static SCSI_State_t SCSI_SenseCode(SCSI_t * scsi, uint8_t sKey, uint8_t ASC);

static SCSI_State_t SCSI_TestUnitReady(SCSI_t * scsi, SCSI_CBW_t * cbw);
static SCSI_State_t SCSI_Inquiry(SCSI_t * scsi, SCSI_CBW_t * cbw);
static SCSI_State_t SCSI_ReadFormatCapacity(SCSI_t * scsi, SCSI_CBW_t * cbw);
static SCSI_State_t SCSI_ReadCapacity10(SCSI_t * scsi, SCSI_CBW_t * cbw);
static SCSI_State_t SCSI_RequestSense(SCSI_t * scsi, SCSI_CBW_t * cbw);
static SCSI_State_t SCSI_StartStopUnit(SCSI_t * scsi, SCSI_CBW_t * cbw);
static SCSI_State_t SCSI_ModeSense6(SCSI_t * scsi, SCSI_CBW_t * cbw);
static SCSI_State_t SCSI_ModeSense10(SCSI_t * scsi, SCSI_CBW_t * cbw);
static SCSI_State_t SCSI_Write10(SCSI_t * scsi, SCSI_CBW_t * cbw);
static SCSI_State_t SCSI_Read10(SCSI_t * scsi, SCSI_CBW_t * cbw);
static SCSI_State_t SCSI_Verify10(SCSI_t * scsi, SCSI_CBW_t * cbw);
static SCSI_State_t SCSI_CheckAddressRange(SCSI_t * scsi, uint32_t blk_offset, uint32_t blk_nbr);

static SCSI_State_t SCSI_ProcessRead(SCSI_t * scsi);
static SCSI_State_t SCSI_ProcessWrite(SCSI_t * scsi);

/*
 * PRIVATE VARIABLES
 */

const uint8_t  cSCSI_InquiryPage00[] =
{
	0x00,
	0x00,
	0x00,
	(INQUIRY_PAGE00_LEN - 4),
	0x00,
	0x80,
	0x83
};

const uint8_t cSCSI_InquiryPage[] = {/* 36 */
	0x00,
	0x80,
	0x02,
	0x02,
	(0x24 - 5),
	0x00,
	0x00,
	0x00,
	'L', 'a', 'm', 'b', 'o', ' ', ' ', ' ', /* Manufacturer : 8 bytes */
	'S', 'T', 'M', '3', '2', 'X', ' ', 'M', /* Product      : 16 Bytes */
	'S', 'C', ' ', ' ', ' ', ' ', ' ', ' ',
	'1', '.', '0' ,'0'                      /* Version      : 4 Bytes */
};

/*
 * PUBLIC FUNCTIONS
 */

SCSI_State_t SCSI_Init(SCSI_t * scsi, const USB_Storage_t * storage)
{
	scsi->sense.head = 0;
	scsi->sense.tail = 0;
	scsi->storage = NULL; // NULL storage indicates no disk.
	if (storage != NULL && storage->open(&scsi->block_count))
	{
		scsi->storage = storage;
	}
	return SCSI_State_Ok;
}

SCSI_State_t SCSI_ProcessCmd(SCSI_t * scsi, SCSI_CBW_t * cbw)
{
	switch (cbw->CB[0])
	{
	case SCSI_TEST_UNIT_READY:
		return SCSI_TestUnitReady(scsi, cbw);
	case SCSI_REQUEST_SENSE:
		return SCSI_RequestSense(scsi, cbw);
	case SCSI_INQUIRY:
		return SCSI_Inquiry(scsi, cbw);
	case SCSI_START_STOP_UNIT:
		return SCSI_StartStopUnit(scsi, cbw);
	case SCSI_ALLOW_MEDIUM_REMOVAL:
		return SCSI_StartStopUnit(scsi, cbw);
	case SCSI_MODE_SENSE6:
		return SCSI_ModeSense6(scsi, cbw);
	case SCSI_MODE_SENSE10:
		return SCSI_ModeSense10(scsi, cbw);
	case SCSI_READ_FORMAT_CAPACITIES:
		return SCSI_ReadFormatCapacity(scsi, cbw);
	case SCSI_READ_CAPACITY10:
		return SCSI_ReadCapacity10(scsi, cbw);
	case SCSI_READ10:
		return SCSI_Read10(scsi, cbw);
	case SCSI_WRITE10:
		return SCSI_Write10(scsi, cbw);
	case SCSI_VERIFY10:
		return SCSI_Verify10(scsi, cbw);
	default:
		return SCSI_SenseCode(scsi, SCSI_SKEY_ILLEGAL_REQUEST, SCSI_ASQ_INVALID_CDB);
	}
}

SCSI_State_t SCSI_ResumeCmd(SCSI_t * scsi, SCSI_State_t state)
{
	switch (state)
	{
	case SCSI_State_DataOut:
		return SCSI_ProcessWrite(scsi);

	case SCSI_State_DataIn:
		return SCSI_ProcessRead(scsi);

	case SCSI_State_SendData:
	case SCSI_State_LastDataIn:
		return SCSI_State_Ok;

	default:
		return SCSI_State_Error;
	}
}

/*
 * PRIVATE FUNCTIONS
 */

static SCSI_State_t SCSI_TestUnitReady(SCSI_t * scsi, SCSI_CBW_t * cbw)
{
	if (cbw->dDataLength != 0)
	{
		return SCSI_SenseCode(scsi, SCSI_SKEY_ILLEGAL_REQUEST, SCSI_ASQ_INVALID_CDB);
	}
	if (scsi->storage == NULL)
	{
		return SCSI_SenseCode(scsi, SCSI_SKEY_NOT_READY, SCSI_ASQ_MEDIUM_NOT_PRESENT);
	}
	return SCSI_State_Ok;
}

static SCSI_State_t  SCSI_Inquiry(SCSI_t * scsi, SCSI_CBW_t * cbw)
{
	uint16_t len;
	const uint8_t * page;

	if (cbw->CB[1] & 0x01U) // Evpd is set
	{
		page = cSCSI_InquiryPage00;
		len = INQUIRY_PAGE00_LEN;
	}
	else
	{
		page = cSCSI_InquiryPage;
		len = (uint16_t)page[4] + 5U;

		if (cbw->CB[4] <= len)
		{
			len = cbw->CB[4];
		}
	}

	memcpy(scsi->bfr, page, len);
	scsi->data_len = len;
	return SCSI_State_SendData;
}

static SCSI_State_t SCSI_ReadCapacity10(SCSI_t * scsi, SCSI_CBW_t * cbw)
{
	if (scsi->storage == NULL)
	{
		return SCSI_SenseCode(scsi, SCSI_SKEY_NOT_READY, SCSI_ASQ_MEDIUM_NOT_PRESENT);
	}
	else
	{
		uint32_t blk_nbr = scsi->block_count - 1;

		scsi->bfr[0] = (uint8_t)(blk_nbr >> 24);
		scsi->bfr[1] = (uint8_t)(blk_nbr >> 16);
		scsi->bfr[2] = (uint8_t)(blk_nbr >>  8);
		scsi->bfr[3] = (uint8_t)(blk_nbr);

		scsi->bfr[4] = (uint8_t)(SCSI_BLOCK_SIZE >>  24);
		scsi->bfr[5] = (uint8_t)(SCSI_BLOCK_SIZE >>  16);
		scsi->bfr[6] = (uint8_t)(SCSI_BLOCK_SIZE >>  8);
		scsi->bfr[7] = (uint8_t)(SCSI_BLOCK_SIZE);

		scsi->data_len = READ_CAPACITY10_LEN;
		return SCSI_State_SendData;
	}
}

static SCSI_State_t SCSI_ReadFormatCapacity(SCSI_t * scsi, SCSI_CBW_t * cbw)
{
	if (scsi->storage == NULL)
	{
		return SCSI_SenseCode(scsi, SCSI_SKEY_NOT_READY, SCSI_ASQ_MEDIUM_NOT_PRESENT);
	}
	else
	{
		uint32_t blk_nbr = scsi->block_count - 1;

		scsi->bfr[0] = 0x00;
		scsi->bfr[1] = 0x00;
		scsi->bfr[2] = 0x00;
	    scsi->bfr[3] = 0x08U;
	    scsi->bfr[4] = (uint8_t)(blk_nbr >> 24);
	    scsi->bfr[5] = (uint8_t)(blk_nbr >> 16);
		scsi->bfr[6] = (uint8_t)(blk_nbr >>  8);
		scsi->bfr[7] = (uint8_t)blk_nbr;

		scsi->bfr[8] = 0x02U;
		scsi->bfr[9] = (uint8_t)(SCSI_BLOCK_SIZE >>  16);
		scsi->bfr[10] = (uint8_t)(SCSI_BLOCK_SIZE >>  8);
		scsi->bfr[11] = (uint8_t)(SCSI_BLOCK_SIZE);

		scsi->data_len = READ_FORMAT_CAPACITY_LEN;
		return SCSI_State_SendData;
	}
}

static SCSI_State_t SCSI_ModeSense6(SCSI_t * scsi, SCSI_CBW_t * cbw)
{
	memset(scsi->bfr, 0, MODE_SENSE6_LEN);
	scsi->data_len = MODE_SENSE6_LEN;
	return SCSI_State_SendData;
}

static SCSI_State_t SCSI_ModeSense10(SCSI_t * scsi, SCSI_CBW_t * cbw)
{
	memset(scsi->bfr, 0x00, MODE_SENSE10_LEN);
	// Byte 2 is constant 0x06. I dont know how these commands are formatted.
	scsi->bfr[2] = 0x06;
	scsi->data_len = MODE_SENSE10_LEN;
	return SCSI_State_SendData;
}

static SCSI_State_t SCSI_RequestSense(SCSI_t * scsi, SCSI_CBW_t * cbw)
{
	memset(scsi->bfr, 0x00, REQUEST_SENSE_LEN);

	scsi->bfr[0] = 0x70U;
	scsi->bfr[7] = REQUEST_SENSE_LEN - 6U;

	if ((scsi->sense.head != scsi->sense.tail))
	{
		scsi->bfr[2]     = scsi->sense.stack[scsi->sense.head].Skey;
		//scsi->bfr[12]   // Leave ASCQ zero
		scsi->bfr[13]    = scsi->sense.stack[scsi->sense.head].ASC;
		scsi->sense.head++;

		if (scsi->sense.head == SCSI_SENSE_DEPTH)
		{
			scsi->sense.head = 0U;
		}
	}
	scsi->data_len = REQUEST_SENSE_LEN;

	if (cbw->CB[4] <= REQUEST_SENSE_LEN)
	{
		scsi->data_len = cbw->CB[4];
	}
	return SCSI_State_SendData;
}

SCSI_State_t SCSI_SenseCode(SCSI_t  * scsi, uint8_t sKey, uint8_t ASC)
{
	scsi->sense.stack[scsi->sense.tail].Skey  = sKey;
	scsi->sense.stack[scsi->sense.tail].ASC   = ASC;
	scsi->sense.tail++;
	if (scsi->sense.tail == SCSI_SENSE_DEPTH)
	{
		scsi->sense.tail = 0U;
	}
	return SCSI_State_Error;
}

static SCSI_State_t SCSI_StartStopUnit(SCSI_t  * scsi, SCSI_CBW_t * cbw)
{
	return SCSI_State_Ok;
}

static SCSI_State_t SCSI_Read10(SCSI_t * scsi, SCSI_CBW_t * cbw)
{
	if ((cbw->bmFlags & 0x80U) != 0x80U)
	{
		return SCSI_SenseCode(scsi, SCSI_SKEY_ILLEGAL_REQUEST, SCSI_ASQ_INVALID_CDB);
	}

	if (scsi->storage == NULL)
	{
		return SCSI_SenseCode(scsi, SCSI_SKEY_NOT_READY, SCSI_ASQ_MEDIUM_NOT_PRESENT);
	}

	scsi->block_addr = ((uint32_t)cbw->CB[2] << 24)
					 | ((uint32_t)cbw->CB[3] << 16)
					 | ((uint32_t)cbw->CB[4] <<  8)
					 | (uint32_t)cbw->CB[5];

	scsi->block_len = ((uint32_t)cbw->CB[7] <<  8) | (uint32_t)cbw->CB[8];

	if (SCSI_CheckAddressRange(scsi, scsi->block_addr, scsi->block_len) != SCSI_State_Ok)
	{
		return SCSI_State_Error;
	}

	/* cases 4,5 : Hi <> Dn */
	if (cbw->dDataLength != (scsi->block_len * SCSI_BLOCK_SIZE))
	{
		return SCSI_SenseCode(scsi, SCSI_SKEY_ILLEGAL_REQUEST, SCSI_ASQ_INVALID_CDB);
	}

	return SCSI_ProcessRead(scsi);
}

static SCSI_State_t SCSI_Write10(SCSI_t * scsi, SCSI_CBW_t * cbw)
{
	if ((cbw->bmFlags & 0x80U) == 0x80U)
	{
		return SCSI_SenseCode(scsi, SCSI_SKEY_ILLEGAL_REQUEST, SCSI_ASQ_INVALID_CDB);
	}

	if (scsi->storage == NULL)
	{
		return SCSI_SenseCode(scsi, SCSI_SKEY_NOT_READY, SCSI_ASQ_MEDIUM_NOT_PRESENT);
	}

	if (scsi->storage->write == NULL)
	{
		return SCSI_SenseCode(scsi, SCSI_SKEY_NOT_READY, SCSI_ASQ_WRITE_PROTECTED);
	}

	scsi->block_addr = ((uint32_t)cbw->CB[2] << 24) |
						  ((uint32_t)cbw->CB[3] << 16) |
						  ((uint32_t)cbw->CB[4] << 8) |
						  (uint32_t)cbw->CB[5];

	scsi->block_len = ((uint32_t)cbw->CB[7] << 8) |
						 (uint32_t)cbw->CB[8];

	// check if LBA address is in the right range
	if (SCSI_CheckAddressRange(scsi, scsi->block_addr, scsi->block_len) != SCSI_State_Ok)
	{
		return SCSI_State_Error;
	}

	if (cbw->dDataLength != (scsi->block_len * SCSI_BLOCK_SIZE))
	{
		return SCSI_SenseCode(scsi, SCSI_SKEY_ILLEGAL_REQUEST, SCSI_ASQ_INVALID_CDB);
	}

	scsi->data_len = SCSI_BLOCK_SIZE;
	return SCSI_State_DataOut;
}

static SCSI_State_t SCSI_Verify10(SCSI_t * scsi, SCSI_CBW_t * cbw)
{
	if ((cbw->CB[1] & 0x02U) == 0x02U)
	{
		// Verify mode not supported
		return SCSI_SenseCode(scsi, SCSI_SKEY_ILLEGAL_REQUEST, SCSI_ASQ_INVALID_FIELD_IN_COMMAND);
	}

	if (SCSI_CheckAddressRange(scsi, scsi->block_addr, scsi->block_len) < 0)
	{
		return SCSI_State_Error;
	}
	return SCSI_State_Ok;
}

static SCSI_State_t SCSI_CheckAddressRange(SCSI_t * scsi, uint32_t blk_offset, uint32_t blk_nbr)
{
	if ((blk_offset + blk_nbr) > scsi->block_count)
	{
		return SCSI_SenseCode(scsi, SCSI_SKEY_ILLEGAL_REQUEST, SCSI_ASQ_ADDRESS_OUT_OF_RANGE);
	}
	return SCSI_State_Ok;
}

static SCSI_State_t SCSI_ProcessRead(SCSI_t * scsi)
{
	if (!scsi->storage->read(scsi->bfr, scsi->block_addr, 1))
	{
		return SCSI_SenseCode(scsi, SCSI_SKEY_HARDWARE_ERROR, SCSI_ASQ_UNRECOVERED_READ_ERROR);
	}

	scsi->block_addr += 1;
	scsi->block_len -= 1;
	scsi->data_len = SCSI_BLOCK_SIZE;

	if (scsi->block_len == 0)
	{
		return SCSI_State_LastDataIn;
	}
	return SCSI_State_DataIn;
}

static SCSI_State_t SCSI_ProcessWrite(SCSI_t  * scsi)
{
	if (!scsi->storage->write(scsi->bfr, scsi->block_addr, 1))
	{
		return SCSI_SenseCode(scsi, SCSI_SKEY_HARDWARE_ERROR, SCSI_ASQ_WRITE_FAULT);
	}

	scsi->block_addr += 1;
	scsi->block_len -= 1;

	if (scsi->block_len == 0)
	{
		return SCSI_State_Ok;
	}
	else
	{
		scsi->data_len = SCSI_BLOCK_SIZE;
		return SCSI_State_DataOut;
	}
}

