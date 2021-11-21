#ifndef SCSI_H
#define SCSI_H

#include <stdint.h>
#include <stdbool.h>
#include "USB_Storage.h"

/*
 * PUBLIC DEFINITIONS
 */

#define SCSI_BLOCK_SIZE								512
#define SCSI_SENSE_DEPTH                           	4

/*
 * PUBLIC TYPES
 */

typedef struct
{
  char Skey;
  uint8_t ASC;
} SCSI_Sense_t;

typedef struct
{
  uint32_t dSignature;
  uint32_t dTag;
  uint32_t dDataLength;
  uint8_t  bmFlags;
  uint8_t  bLUN;
  uint8_t  bCBLength;
  uint8_t  CB[16];
  uint8_t  ReservedForAlign;
} SCSI_CBW_t;

typedef struct
{
  uint32_t dSignature;
  uint32_t dTag;
  uint32_t dDataResidue;
  uint8_t  bStatus;
  uint8_t  ReservedForAlign[3];
} SCSI_CSW_t;

typedef struct {
	const USB_Storage_t * storage;

	uint32_t block_count;
	uint32_t block_addr;
	uint32_t block_len;

	struct {
		SCSI_Sense_t stack[SCSI_SENSE_DEPTH];
		uint8_t head;
		uint8_t tail;
	} sense;

	uint8_t bfr[SCSI_BLOCK_SIZE];
	uint16_t data_len;
} SCSI_t;

typedef enum {
	SCSI_State_Error = -1,
	SCSI_State_Ok = 0,
	SCSI_State_SendData,
	SCSI_State_DataOut,
	SCSI_State_DataIn,
	SCSI_State_LastDataIn,
} SCSI_State_t;

/*
 * PUBLIC FUNCTIONS
 */

SCSI_State_t SCSI_Init(SCSI_t * scsi, const USB_Storage_t * storage);
SCSI_State_t SCSI_ProcessCmd(SCSI_t * scsi, SCSI_CBW_t * cbw);
SCSI_State_t SCSI_ResumeCmd(SCSI_t * scsi, SCSI_State_t state);

#endif // SCSI_H

