#ifndef USB_EP_H
#define USB_EP_H

#include "STM32X.h"

/*
 * PUBLIC DEFINITIONS
 */

/*
 * PUBLIC TYPES
 */

typedef enum {
	USB_EP_TYPE_NONE,
	USB_EP_TYPE_CTRL,
	USB_EP_TYPE_BULK,
	USB_EP_TYPE_INTR,
	USB_EP_TYPE_ISOC,
} USB_EP_Type_t;

typedef void (*USB_EP_Callback_t)(uint32_t count);

/*
 * PUBLIC FUNCTIONS
 */

void USB_EP_Init(void);
void USB_EP_Deinit(void);
void USB_EP_Reset(void);

void USB_EP_Open(uint8_t endpoint, uint8_t type, uint16_t size, USB_EP_Callback_t callback);
void USB_EP_Close(uint8_t endpoint);
bool USB_EP_IsOpen(uint8_t endpoint);
void USB_EP_Read(uint8_t endpoint, uint8_t *data, uint32_t count);
void USB_EP_Write(uint8_t endpoint, const uint8_t * data, uint32_t count);
void USB_EP_WriteZLP(uint8_t endpoint);
void USB_EP_Stall(uint8_t endpoint);
void USB_EP_Destall(uint8_t endpoint);
bool USB_EP_IsStalled(uint8_t endpoint);

void USB_EP_IRQHandler(void);

/*
 * EXTERN DECLARATIONS
 */

#endif //USB_EP_H
