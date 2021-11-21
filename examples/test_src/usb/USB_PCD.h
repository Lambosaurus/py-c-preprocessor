#ifndef USB_PCD_H
#define USB_PCD_H

#include "STM32X.h"


/*
 * PUBLIC DEFINITIONS
 */

/*
 * PUBLIC TYPES
 */

/*
 * PUBLIC FUNCTIONS
 */

void USB_PCD_Init(void);
void USB_PCD_Start(void);
void USB_PCD_Stop(void);
void USB_PCD_Deinit(void);
void USB_PCD_SetAddress(uint8_t address);

/*
 * EXTERN DECLARATIONS
 */

#endif //USB_PCD_H
