
#include "USB_EP.h"
#ifdef USB_ENABLE

#include "USB_CTL.h"
#include "USB_PCD.h"
#include "USB_Defs.h"
#include "USB_Class.h"

/*
 * PRIVATE DEFINITIONS
 */

#define BTABLE_SIZE			(USB_ENDPOINTS * 8)

#define PMA_SIZE			1024
#define PMA_BASE			(((uint32_t)USB) + 0x400)

/*
 * PRIVATE TYPES
 */

typedef struct {
	uint8_t num;
	bool is_in;
	bool is_stall;
	uint8_t type;
	uint16_t  pmaadress;
#ifdef USB_USE_DOUBLEBUFFER
	uint16_t  pmaaddr0;
	uint16_t  pmaaddr1;
	uint32_t xfer_len_db;
	bool xfer_fill_db;
	uint8_t   doublebuffer;
#endif
	uint32_t maxpacket;
	uint8_t * xfer_buff;
	uint32_t xfer_len;
	uint32_t xfer_count;
	USB_EP_Callback_t callback;
} USB_EP_t;

/*
 * PRIVATE PROTOTYPES
 */

static uint16_t USB_PMA_Alloc(uint16_t size);
static void USB_PMA_Write(uint16_t address, uint8_t * data, uint16_t count);
static void USB_PMA_Read(uint16_t address, uint8_t * data, uint16_t count);
static void USB_EP_Activate(USB_EP_t *ep);
static void USB_EP_Deactivate(USB_EP_t *ep);

static USB_EP_t * USB_EP_GetEP(uint8_t endpoint);
static void USB_EP_StartIn(USB_EP_t *ep);
static void USB_EP_StartOut(USB_EP_t *ep);

#ifdef USE_EP_DOUBLEBUFFER
static uint16_t USB_EP_ReceiveDB(USB_EP_t *ep, uint16_t wEPVal);
static void USB_EP_TransmitDB(USB_EP_t *ep, uint16_t wEPVal);
#endif //USE_EP_DOUBLEBUFFER

/*
 * PRIVATE VARIABLES
 */

static struct {
	uint16_t pma_head;
	USB_EP_t in_ep[USB_ENDPOINTS];
	USB_EP_t out_ep[USB_ENDPOINTS];
}gEP;

/*
 * PUBLIC FUNCTIONS
 */

void USB_EP_Init(void)
{
	gEP.pma_head = BTABLE_SIZE;

	for (uint32_t i = 0U; i < USB_ENDPOINTS; i++)
	{
		USB_EP_t * ep = &gEP.in_ep[i];
		ep->is_in = true;
		ep->num = i;
		ep->type = USB_EP_TYPE_NONE;
		ep->maxpacket = 0;
		ep->xfer_buff = 0;
		ep->xfer_len = 0;
	}
	for (uint32_t i = 0U; i < USB_ENDPOINTS; i++)
	{
		USB_EP_t * ep = &gEP.out_ep[i];
		ep->is_in = false;
		ep->num = i;
		ep->type = USB_EP_TYPE_NONE;
		ep->maxpacket = 0;
		ep->xfer_buff = 0;
		ep->xfer_len = 0;
	}
}

void USB_EP_Reset(void)
{
	gEP.pma_head = BTABLE_SIZE;
}

void USB_EP_Deinit(void)
{

}

void USB_EP_Open(uint8_t endpoint, uint8_t type, uint16_t size, USB_EP_Callback_t callback)
{
	USB_EP_t * ep = USB_EP_GetEP(endpoint);
	ep->maxpacket = size;
	ep->type = type;
	ep->callback = callback;
#ifdef USE_EP_DOUBLEBUFFER
	if (doublebuffer)
	{
		ep->doublebuffer = 1;
		ep->pmaaddr0 = USB_PMA_Alloc(size);
		ep->pmaaddr1 = USB_PMA_Alloc(size);
	}
	else
	{
		ep->pmaadress = USB_PMA_Alloc(size);
	}
#else // USE_EP_DOUBLEBUFFER
	ep->pmaadress = USB_PMA_Alloc(size);
#endif
	USB_EP_Activate(ep);
}

void USB_EP_Close(uint8_t endpoint)
{
	USB_EP_t * ep = USB_EP_GetEP(endpoint);
	ep->type = USB_EP_TYPE_NONE;
	USB_EP_Deactivate(ep);
}

bool USB_EP_IsOpen(uint8_t endpoint)
{
	USB_EP_t * ep = USB_EP_GetEP(endpoint);
	return ep->type != USB_EP_TYPE_NONE;
}

void USB_EP_Read(uint8_t endpoint, uint8_t * data, uint32_t count)
{
	USB_EP_t * ep = &gEP.out_ep[endpoint & EP_ADDR_MSK];
	ep->xfer_buff = data;
	ep->xfer_len = count;
	ep->xfer_count = 0;
	USB_EP_StartOut(ep);
}

void USB_EP_Write(uint8_t endpoint, const uint8_t * data, uint32_t count)
{
	USB_EP_t * ep = &gEP.in_ep[endpoint & EP_ADDR_MSK];
	ep->xfer_buff = (uint8_t *)data;
	ep->xfer_len = count;
#ifdef USB_USE_DOUBLEBUFFER
	ep->xfer_fill_db = 1;
	ep->xfer_len_db = count;
#endif
	ep->xfer_count = 0;
	USB_EP_StartIn(ep);
}

void USB_EP_WriteZLP(uint8_t endpoint)
{
	uint8_t epnum = endpoint & EP_ADDR_MSK;
	PCD_SET_EP_TX_CNT(USB, epnum, 0);
	PCD_SET_EP_TX_STATUS(USB, epnum, USB_EP_TX_VALID);
}

void USB_EP_Stall(uint8_t endpoint)
{
	USB_EP_t * ep = USB_EP_GetEP(endpoint);
	ep->is_stall = true;
	if (ep->is_in)
	{
		PCD_SET_EP_TX_STATUS(USB, ep->num, USB_EP_TX_STALL);
	}
	else
	{
		PCD_SET_EP_RX_STATUS(USB, ep->num, USB_EP_RX_STALL);
	}
}

void USB_EP_Destall(uint8_t endpoint)
{
	USB_EP_t *ep = USB_EP_GetEP(endpoint);
	ep->is_stall = false;
#ifdef USB_USE_DOUBLEBUFFER
	if (!ep->doublebuffer)
#endif
	{
		if (ep->is_in)
		{
			PCD_CLEAR_TX_DTOG(USB, ep->num);
			if (ep->type != USB_EP_TYPE_ISOC)
			{
				PCD_SET_EP_TX_STATUS(USB, ep->num, USB_EP_TX_NAK);
			}
		}
		else
		{
			PCD_CLEAR_RX_DTOG(USB, ep->num);
			PCD_SET_EP_RX_STATUS(USB, ep->num, USB_EP_RX_VALID);
		}
	}
}

bool USB_EP_IsStalled(uint8_t endpoint)
{
	USB_EP_t * ep = USB_EP_GetEP(endpoint);
	return ep->is_stall;
}

/*
 * PRIVATE FUNCTIONS
 */

static USB_EP_t * USB_EP_GetEP(uint8_t endpoint)
{
	if (endpoint & 0x80U)
	{
		return &gEP.in_ep[endpoint & EP_ADDR_MSK];
	}
	else
	{
		return &gEP.out_ep[endpoint & EP_ADDR_MSK];
	}
}

static uint16_t USB_PMA_Alloc(uint16_t size)
{
	uint16_t head = gEP.pma_head;
	gEP.pma_head += size;
	if (gEP.pma_head > PMA_SIZE)
	{
		// TODO: handle this better.
		__BKPT();
	}
	return head;
}

static void USB_PMA_Write(uint16_t address, uint8_t * data, uint16_t count)
{
	volatile uint16_t * pma = (volatile uint16_t *)(PMA_BASE + ((uint32_t)address * PMA_ACCESS));
	uint32_t words = (count + 1) / 2;
	while(words--)
	{
		uint32_t b1 = *data++;
		uint32_t b2 = *data++;
		*pma = b1 | (b2 << 8);
		pma += PMA_ACCESS;
	}
}

static void USB_PMA_Read(uint16_t address, uint8_t * data, uint16_t count)
{
	volatile uint16_t * pma = (volatile uint16_t *)(PMA_BASE + ((uint32_t)address * PMA_ACCESS));
	uint32_t words = count / 2;

	while (words--)
	{
		uint32_t word = *pma;
		pma += PMA_ACCESS;
		*data++ = (uint8_t)word;
		*data++ = (uint8_t)(word >> 8);
	}

	if (count & 0x01)
	{
		uint32_t word = *pma;
		*data = (uint8_t)word;
	}
}

static void USB_EP_Activate(USB_EP_t *ep)
{
	uint16_t epReg = PCD_GET_ENDPOINT(USB, ep->num) & USB_EP_T_MASK;

	switch (ep->type)
	{
	case USB_EP_TYPE_CTRL:
		epReg |= USB_EP_CONTROL;
	  break;
	case USB_EP_TYPE_BULK:
		epReg |= USB_EP_BULK;
	  break;
	case USB_EP_TYPE_INTR:
		epReg |= USB_EP_INTERRUPT;
	  break;
	case USB_EP_TYPE_ISOC:
		epReg |= USB_EP_ISOCHRONOUS;
	  break;
	}

	PCD_SET_ENDPOINT(USB, ep->num, (epReg | USB_EP_CTR_RX | USB_EP_CTR_TX));
	PCD_SET_EP_ADDRESS(USB, ep->num, ep->num);

#ifdef USE_EP_DOUBLEBUFFER
	if (ep->doublebuffer)
	{
		PCD_SET_EP_DBUF(USB, ep->num);
		PCD_SET_EP_DBUF_ADDR(USB, ep->num, ep->pmaaddr0, ep->pmaaddr1);
		PCD_CLEAR_RX_DTOG(USB, ep->num);
		PCD_CLEAR_TX_DTOG(USB, ep->num);

		if (ep->is_in)
		{
			if (ep->type != EP_TYPE_ISOC)
			{
				PCD_SET_EP_TX_STATUS(USB, ep->num, USB_EP_TX_NAK);
			}
			PCD_SET_EP_RX_STATUS(USB, ep->num, USB_EP_RX_DIS);
		}
		else
		{
			// Clear the data toggle bits for the endpoint IN/OUT
			PCD_SET_EP_RX_STATUS(USB, ep->num, USB_EP_RX_VALID);
			PCD_SET_EP_TX_STATUS(USB, ep->num, USB_EP_TX_DIS);
		}
		return
	}
#endif //USE_EP_DOUBLEBUFFER
	if (ep->is_in)
	{
		PCD_SET_EP_TX_ADDRESS(USB, ep->num, ep->pmaadress);
		PCD_CLEAR_TX_DTOG(USB, ep->num);
		// Isochronos should leave their TX EP disabled.
		if (ep->type != USB_EP_TYPE_ISOC)
		{
			PCD_SET_EP_TX_STATUS(USB, ep->num, USB_EP_TX_NAK);
		}
	}
	else
	{
		PCD_SET_EP_RX_ADDRESS(USB, ep->num, ep->pmaadress);
		PCD_SET_EP_RX_CNT(USB, ep->num, ep->maxpacket);
		PCD_CLEAR_RX_DTOG(USB, ep->num);
		PCD_SET_EP_RX_STATUS(USB, ep->num, USB_EP_RX_VALID);
	}
}

static void USB_EP_Deactivate(USB_EP_t *ep)
{
#ifdef USE_EP_DOUBLEBUFFER
	if (ep->doublebuffer)
	{
		PCD_CLEAR_RX_DTOG(USB, ep->num);
		PCD_CLEAR_TX_DTOG(USB, ep->num);
		if (ep->is_in)
		{
			PCD_RX_DTOG(USB, ep->num);
		}
		else
		{
			PCD_TX_DTOG(USB, ep->num);
		}
		PCD_SET_EP_RX_STATUS(USB, ep->num, USB_EP_RX_DIS);
		PCD_SET_EP_TX_STATUS(USB, ep->num, USB_EP_TX_DIS);
		return;
	}
#endif //USE_EP_DOUBLEBUFFER
	if (ep->is_in)
	{
		PCD_CLEAR_TX_DTOG(USB, ep->num);
		PCD_SET_EP_TX_STATUS(USB, ep->num, USB_EP_TX_DIS);
	}
	else
	{
		PCD_CLEAR_RX_DTOG(USB, ep->num);
		PCD_SET_EP_RX_STATUS(USB, ep->num, USB_EP_RX_DIS);
	}
}


#ifdef USE_EP_DOUBLEBUFFER
static uint16_t USB_EP_ReceiveDB(USB_EP_t *ep, uint16_t epReg)
{
	bool db0 = epReg & USB_EP_DTOG_RX;
	uint16_t count;
	uint16_t pmaaddr;
	if (db0)
	{
		count = PCD_GET_EP_DBUF0_CNT(USB, ep->num);
		pmaaddr = ep->pmaaddr0;
	}
	else
	{
		count = PCD_GET_EP_DBUF1_CNT(USB, ep->num);
		pmaaddr = ep->pmaaddr1;
	}

	if (ep->type == EP_TYPE_BULK)
	{
		ep->xfer_len = ep->xfer_len >= count ? ep->xfer_len - count : 0;
		PCD_SET_EP_RX_STATUS(USB, ep->num, USB_EP_RX_NAK);

		bool db1 = epReg & USB_EP_DTOG_TX;
		if (db0 == db1)
		{
			// I dont quite understand this logic.
			// It seems the buffers are swapped in this case.
			PCD_TX_DTOG(USB, ep->num);
		}
	}
	else
	{
		PCD_TX_DTOG(USB, ep->num);
	}

	USB_PMA_Read(pmaaddr, ep->xfer_buff, count);
	return count;
}

static void USB_EP_TransmitDB(USB_EP_t *ep, uint16_t epReg)
{
	bool db0 = epReg & USB_EP_DTOG_TX;
	uint16_t count;
	if (db0)
	{
		count = PCD_GET_EP_DBUF0_CNT(USB, ep->num);
	}
	else
	{
		count = PCD_GET_EP_DBUF1_CNT(USB, ep->num);
	}

	if (ep->xfer_len == 0)
	{
		HAL_PCD_DataInStageCallback(hpcd, ep->num);
	}

	bool db1 = epReg & USB_EP_DTOG_RX;
	if (db0 == db1)
	{
		PCD_RX_DTOG(USB, ep->num);
	}

	if (ep->xfer_len && ep->xfer_fill_db)
	{
		ep->xfer_buff += count;
		ep->xfer_count += count;

		uint32_t nextCount;
		if (ep->xfer_len_db >= ep->maxpacket)
		{
			nextCount = ep->maxpacket;
			ep->xfer_len_db -= nextCount;
		}
		else if (ep->xfer_len_db == 0)
		{
			nextCount = count;
			ep->xfer_fill_db = 0;
		}
		else
		{
			ep->xfer_fill_db = 0;
			nextCount = ep->xfer_len_db;
			ep->xfer_len_db = 0;
		}

		if (db0)
		{
			PCD_SET_EP_DBUF0_CNT(hpcd->Instance, ep->num, ep->is_in, nextCount);
			USB_PMA_Write(ep->pmaaddr0, ep->xfer_buff, nextCount);
		}
		else
		{
			PCD_SET_EP_DBUF1_CNT(hpcd->Instance, ep->num, ep->is_in, nextCount);
			USB_PMA_Write(ep->pmaaddr1, ep->xfer_buff, nextCount);
		}
	}
	PCD_SET_EP_TX_STATUS(hpcd->Instance, ep->num, USB_EP_TX_VALID);
}
#endif //USE_EP_DOUBLEBUFFER

static void USB_EP_StartIn(USB_EP_t *ep)
{
	uint32_t len;

	if (ep->xfer_len > ep->maxpacket)
	{
		len = ep->maxpacket;
	}
	else
	{
		len = ep->xfer_len;
	}

#ifdef USE_EP_DOUBLEBUFFER
	if (ep->doublebuffer)
	{
		uint32_t pmabuffer;

		if (ep->type == EP_TYPE_BULK)
		{
			if (ep->xfer_len_db > ep->maxpacket)
			{
				PCD_SET_EP_DBUF(USB, ep->num);

				bool db1 = PCD_GET_ENDPOINT(USB, ep->num) & USB_EP_DTOG_TX;

				if (db1)
				{
					PCD_SET_EP_DBUF1_CNT(USB, ep->num, 1, len);
					pmabuffer = ep->pmaaddr1;
				}
				else
				{
					PCD_SET_EP_DBUF0_CNT(USB, ep->num, 1, len);
					pmabuffer = ep->pmaaddr0;
				}

				USB_PMA_Write(pmabuffer, ep->xfer_buff, len);
				ep->xfer_len_db -= len;
				ep->xfer_buff += len;
				if (ep->xfer_len_db > ep->maxpacket)
				{
					ep->xfer_len_db -= len;
				}
				else
				{
					len = ep->xfer_len_db;
					ep->xfer_len_db = 0;
				}

				if (db1)
				{
					PCD_SET_EP_DBUF0_CNT(USB, ep->num, 1, len);
					pmabuffer = ep->pmaaddr0;
				}
				else
				{
					PCD_SET_EP_DBUF1_CNT(USB, ep->num, 1, len);
					pmabuffer = ep->pmaaddr1;
				}

				USB_PMA_Write(pmabuffer, ep->xfer_buff, len);
			}
			else
			{
				// Double buffer not required for this payload
				len = ep->xfer_len_db;
				PCD_CLEAR_EP_DBUF(USB, ep->num);
				PCD_SET_EP_TX_CNT(USB, ep->num, len);
				pmabuffer = ep->pmaaddr0;
				USB_PMA_Write(pmabuffer, ep->xfer_buff, len);
			}
		}
		else // ISO Doublebuffer
		{
			if ((PCD_GET_ENDPOINT(USB, ep->num) & USB_EP_DTOG_TX) != 0U)
			{
				PCD_SET_EP_DBUF1_CNT(USB, ep->num, 1, len);
				pmabuffer = ep->pmaaddr1;
			}
			else
			{
				PCD_SET_EP_DBUF0_CNT(USB, ep->num, 1, len);
				pmabuffer = ep->pmaaddr0;
			}

			USB_PMA_Write(pmabuffer, ep->xfer_buff, (uint16_t)len);
			PCD_RX_DTOG(USB, ep->num);
		}
	}
	else
#endif //USE_EP_DOUBLEBUFFER
	{
		USB_PMA_Write(ep->pmaadress, ep->xfer_buff, len);
		PCD_SET_EP_TX_CNT(USB, ep->num, len);
	}

	PCD_SET_EP_TX_STATUS(USB, ep->num, USB_EP_TX_VALID);
}

static void USB_EP_StartOut(USB_EP_t *ep)
{
	uint32_t len;
#ifdef USE_EP_DOUBLEBUFFER
	if (ep->doublebuffer)
	{
		if (ep->type == EP_TYPE_BULK)
		{
			PCD_SET_EP_DBUF_CNT(USB, ep->num, 0, ep->maxpacket);

			// Coming from ISR
			if (ep->xfer_count != 0U)
			{
				// Check if buffers are blocked.
				uint16_t epReg = PCD_GET_ENDPOINT(USB, ep->num);
				if ((((epReg & USB_EP_DTOG_RX) != 0U) && ((epReg & USB_EP_DTOG_TX) != 0U)) ||
						(((epReg & USB_EP_DTOG_RX) == 0U) && ((epReg & USB_EP_DTOG_TX) == 0U)))
				{
					PCD_TX_DTOG(USB, ep->num);
				}
			}
		}
		else if (ep->type == EP_TYPE_ISOC)
		{
			// ISO out doublebuffer
			if (ep->xfer_len > ep->maxpacket)
			{
				len = ep->maxpacket;
				ep->xfer_len -= len;
			}
			else
			{
				len = ep->xfer_len;
				ep->xfer_len = 0;
			}
			PCD_SET_EP_DBUF_CNT(USB, ep->num, 0, len);
		}
	}
	else
#endif //USE_EP_DOUBLEBUFFER
	{
		if (ep->xfer_len > ep->maxpacket)
		{
			len = ep->maxpacket;
			ep->xfer_len -= len;
		}
		else
		{
			len = ep->xfer_len;
			ep->xfer_len = 0U;
		}
		PCD_SET_EP_RX_CNT(USB, ep->num, len);
	}

	PCD_SET_EP_RX_STATUS(USB, ep->num, USB_EP_RX_VALID);
}

/*
 * INTERRUPT ROUTINES
 */


void USB_EP_IRQHandler(void)
{
	while (USB->ISTR & USB_ISTR_CTR)
	{
		uint32_t istr = USB->ISTR;
		uint8_t epnum = (uint8_t)(istr & USB_ISTR_EP_ID);
		uint16_t epReg = PCD_GET_ENDPOINT(USB, epnum);

		// Control endpoint
		if (epReg & USB_EP_CTR_TX)
		{
			// IN endpoint
			USB_EP_t * ep = &gEP.in_ep[epnum];
			PCD_CLEAR_TX_EP_CTR(USB, epnum);

#ifdef USE_EP_DOUBLEBUFFER
			if (epReg & USB_EP_KIND && ep->type == USB_EP_TYPE_BULK)
			{
				USB_EP_TransmitDB(ep, epReg);
			}
			else
#endif
			{
				uint16_t count = (uint16_t)PCD_GET_EP_TX_CNT(USB, ep->num);
				ep->xfer_len = ep->xfer_len > count ? ep->xfer_len - count : 0;
				if (ep->xfer_len == 0)
				{
					// Transfer is complete
					ep->callback(ep->xfer_count);
				}
				else
				{
					// Transfer is not yet done
					ep->xfer_buff += count;
					ep->xfer_count += count;
					USB_EP_StartIn(ep);
				}
			}
		}
		else
		{
			// OUT endpoint
			USB_EP_t * ep = &gEP.out_ep[epnum];
			if (epReg & USB_EP_SETUP)
			{
				ep->xfer_count = PCD_GET_EP_RX_CNT(USB, ep->num);

				// Handle this transfer in a local buffer. The xfer_buff will not be allocated.
				uint8_t setup[8];
				USB_PMA_Read(ep->pmaadress, setup, ep->xfer_count);

				// SETUP bit kept frozen while CTR_RX
				PCD_CLEAR_RX_EP_CTR(USB, 0);
				USB_CTL_HandleSetup(setup);
			}
			else if (epReg & USB_EP_CTR_RX)
			{
				PCD_CLEAR_RX_EP_CTR(USB, epnum);

				uint16_t count;
#ifdef USE_EP_DOUBLEBUFFER
				if (ep->doublebuffer)
				{
					count = USB_EP_ReceiveDB(ep, epReg);
				}
				else
#endif //USE_EP_DOUBLEBUFFER
				{
					count = PCD_GET_EP_RX_CNT(USB, ep->num);
					if (count)
					{
						USB_PMA_Read(ep->pmaadress, ep->xfer_buff, count);
					}
				}

				ep->xfer_count += count;
				ep->xfer_buff += count;

				if (count < ep->maxpacket || ep->xfer_len == 0)
				{
					ep->callback(ep->xfer_count);
				}
				else
				{
					USB_EP_StartOut(ep);
				}
			}
		}
	}
}

#endif //USB_ENABLE
