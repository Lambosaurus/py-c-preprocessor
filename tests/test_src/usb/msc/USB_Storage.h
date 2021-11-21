#ifndef USB_STORAGE_H
#define USB_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

/*
 * PUBLIC DEFINITIONS
 */

#define USB_MSC_BLOCK_SIZE		512

/*
 * PUBLIC TYPES
 */

typedef struct {
	bool (*open)(uint32_t * blk_count);
	bool (*read)(uint8_t * bfr, uint32_t blk_addr, uint16_t blk_count);
	bool (*write)(const uint8_t * bfr, uint32_t blk_addr, uint16_t blk_count);
}USB_Storage_t;



#endif //USB_STORAGE_H
