#ifndef CONFIG_FLASH_H
#define CONFIG_FLASH_H

#include <stdint.h>
#include <stdbool.h>

/* Low-level access to the flash areas reserved for the config log
 * (linker region CONFIG = sectors 6 and 7, 128 KB each).
 * Host tests link a RAM fake with the same interface instead of config_flash.c.
 *
 * Erase and program stall the CPU while the flash is busy — sector erase
 * takes ~1 s. Not thread-safe: call from a single task. */

#define CONFIG_FLASH_AREAS  2u

uint32_t       config_flash_area_size(void);
const uint8_t *config_flash_area_ptr(uint8_t area);   /* memory-mapped, read-only */

bool config_flash_erase(uint8_t area);

/* Programs count words at offset (multiple of 4) and verifies them by read-back. */
bool config_flash_program(uint8_t area, uint32_t offset, const uint32_t *words, uint32_t count);

#endif /* CONFIG_FLASH_H */
