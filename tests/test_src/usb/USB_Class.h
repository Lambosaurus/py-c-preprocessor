#ifndef USB_CLASS_H
#define USB_CLASS_H

#include "STM32X.h"

/*
 * PUBLIC DEFINITIONS
 */

#if defined(USB_CLASS_CDC)
#include "cdc/USB_CDC.h"

#define USB_CLASS_CLASSID				USB_CDC_CLASSID
#define USB_CLASS_SUBCLASSID			USB_CDC_SUBCLASSID
#define USB_CLASS_PROTOCOLID			USB_CDC_PROTOCOLID
#define USB_CLASS_DEVICE_DESCRIPTOR 	USB_CDC_CONFIG_DESC


#define USB_CLASS_INIT(config)			USB_CDC_Init(config)
#define USB_CLASS_DEINIT()				USB_CDC_Deinit()
#define USB_CLASS_SETUP(request) 		USB_CDC_Setup(request)
#define USB_CLASS_CTL_RXREADY()			USB_CDC_CtlRxReady()
//#define USB_CLASS_CTL_TXDONE

#define USB_ENDPOINTS					USB_CDC_ENDPOINTS
#define USB_INTERFACES					USB_CDC_INTERFACES

#elif defined(USB_CLASS_MSC)
#include "msc/USB_MSC.h"

#define USB_CLASS_DEVICE_DESCRIPTOR 	USB_MSC_CONFIG_DESC

#define USB_CLASS_INIT(config)			USB_MSC_Init(config)
#define USB_CLASS_DEINIT()				USB_MSC_Deinit()
#define USB_CLASS_SETUP(request) 		USB_MSC_Setup(request)

#define USB_ENDPOINTS					USB_MSC_ENDPOINTS
#define USB_INTERFACES					USB_MSC_INTERFACES

#else
#error "No USB Class defined"
#endif

/*
 * PUBLIC TYPES
 */

/*
 * PUBLIC FUNCTIONS
 */

/*
 * EXTERN DECLARATIONS
 */

#endif //USB_CLASS_H
