#ifndef USB_CDC_H
#define USB_CDC_H

#include "STM32X.h"
#include "../USB_Defs.h"

/*
 * FUNCTIONAL TESTING
 * STM32L0: Y
 * STM32F0: N
 */

/*
 * PUBLIC DEFINITIONS
 */

#define USB_CDC_INTERFACES			2
#define USB_CDC_ENDPOINTS			3

#define USB_CDC_CLASSID				0x02
#define USB_CDC_SUBCLASSID			0x02
#define USB_CDC_PROTOCOLID			0x00

#define USB_CDC_CONFIG_DESC_SIZE	67
#define USB_CDC_CONFIG_DESC			cUSB_CDC_ConfigDescriptor

/*
 * PUBLIC TYPES
 */

/*
 * PUBLIC FUNCTIONS
 */

// Callbacks for USB_CTL.
// These should be referenced in USB_Class.h
void USB_CDC_Init(uint8_t config);
void USB_CDC_Deinit(void);
void USB_CDC_CtlRxReady(void);
void USB_CDC_Setup(USB_SetupRequest_t * req);

// Interface to user
uint32_t USB_CDC_ReadReady(void);
uint32_t USB_CDC_Read(uint8_t * data, uint32_t count);
void USB_CDC_Write(const uint8_t * data, uint32_t count);

/*
 * EXTERN DECLARATIONS
 */

extern const uint8_t cUSB_CDC_ConfigDescriptor[USB_CDC_CONFIG_DESC_SIZE];


#endif //USB_CDC_H
