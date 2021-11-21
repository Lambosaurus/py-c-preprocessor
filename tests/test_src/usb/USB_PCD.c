
#include "USB_PCD.h"

#ifdef USB_ENABLE
#include "USB_EP.h"
#include "USB_CTL.h"
#include "USB_CLASS.h"

/*
 * PRIVATE DEFINITIONS
 */

#define USB_GET_IRQ() 		(USB->ISTR)
#define USB_CLR_IRQ(flag)	(USB->ISTR &= ~flag)

/*
 * PRIVATE TYPES
 */

/*
 * PRIVATE PROTOTYPES
 */

static void USB_PCD_Reset(void);

/*
 * PRIVATE VARIABLES
 */

#ifdef USB_USE_LPM
static struct {
	uint8_t lowPowerMode;
} gPCD;
#endif

/*
 * PUBLIC FUNCTIONS
 */

void USB_PCD_Init(void)
{
	USB->CNTR = USB_CNTR_FRES; // Issue reset
	USB->CNTR = 0;
	USB->ISTR = 0;
	USB->BTABLE = BTABLE_ADDRESS;

	USB_EP_Init();

#ifdef USB_USE_LPM
	gPCD.lowPowerMode = LPM_L0;
	USB->LPMCSR |= USB_LPMCSR_LMPEN;
	USB->LPMCSR |= USB_LPMCSR_LPMACK;
#endif
}

void USB_PCD_Start(void)
{
	// Enable interrupt sources
	USB->CNTR = USB_CNTR_CTRM | USB_CNTR_RESETM
#ifdef USB_USE_LPM
			  | USB_CNTR_WKUPM | USB_CNTR_SUSPM | USB_CNTR_L1REQM
#endif
			  // | USB_CNTR_SOFM | USB_CNTR_ESOFM | USB_CNTR_ERRM
			  // | USB_CNTR_RESUME remote wakeup mode?
			  ;

	USB->BCDR |= USB_BCDR_DPPU; // Enable DP pullups
}

void USB_PCD_Stop(void)
{
	// disable all interrupts and force USB reset
	USB->CNTR = USB_CNTR_FRES;
	USB->ISTR = 0U;
	// switch-off device
	USB->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
	// Disable DP pullups
	USB->BCDR &= ~USB_BCDR_DPPU;
}

void USB_PCD_SetAddress(uint8_t address)
{
	USB->DADDR = address | USB_DADDR_EF;
}

/*
 * PRIVATE FUNCTIONS
 */

static void USB_PCD_Reset(void)
{
	// Clear existing endpoint layouts.
	USB_CTL_Deinit();
	USB_EP_Reset();
	USB_PCD_SetAddress(0);

	// Reinit the CTRL EP's
	USB_CTL_Init();
}

/*
 * INTERRUPT ROUTINES
 */

void USB_IRQHandler(void)
{
	uint32_t istr = USB_GET_IRQ();

	if (istr & USB_ISTR_CTR)
	{
		USB_EP_IRQHandler();
	}
	else if (istr & USB_ISTR_RESET)
	{
		USB_CLR_IRQ(USB_ISTR_RESET);
		USB_PCD_Reset();
	}
	else if (istr & USB_ISTR_PMAOVR)
	{
		USB_CLR_IRQ(USB_ISTR_PMAOVR);
	}
#ifdef USB_USE_LPM
	else if (istr & USB_ISTR_SUSP)
	{
		// Force low-power mode in the peripheral
		USB->CNTR |= USB_CNTR_FSUSP;
		// clear of the ISTR bit must be done after setting of CNTR_FSUSP
		USB_CLR_IRQ(USB_ISTR_SUSP);
		USB->CNTR |= USB_CNTR_LPMODE;
	}
	else if (istr & USB_ISTR_WKUP)
	{
		// Clear LP & suspend modes.
		USB->CNTR &= ~(USB_CNTR_LPMODE | USB_CNTR_FSUSP);
		gPCD.lowPowerMode = LPM_L0;
		USB_CLR_IRQ(USB_ISTR_WKUP);
	}
	else if (istr & USB_ISTR_L1REQ)
	{
		USB_CLR_IRQ(USB_ISTR_L1REQ);

		if (gPCD.lowPowerMode == LPM_L0)
		{
			// Force suspend and low-power mode before going to L1 state
			USB->CNTR |= USB_CNTR_LPMODE | USB_CNTR_FSUSP;
			gPCD.lowPowerMode = LPM_L1;
			hpcd->BESL = ((uint32_t)USB->LPMCSR & USB_LPMCSR_BESL) >> 2;
		}
	}
#endif
}

#endif //USB_ENABLE
