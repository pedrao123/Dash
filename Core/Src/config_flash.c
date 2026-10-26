#include "config_flash.h"
#include "stm32f7xx_hal.h"
#include <string.h>

#define AREA_SIZE  (128u * 1024u)

/* Linker symbol: ORIGIN(CONFIG). The linker script asserts it is sector 6. */
extern const uint8_t __config_store_start[];

static const uint32_t k_sector[CONFIG_FLASH_AREAS] = { FLASH_SECTOR_6, FLASH_SECTOR_7 };

/* D-cache is off today; keep reads coherent if someone enables it later. */
static void invalidate(uint8_t area, uint32_t offset, uint32_t len) {
    if (SCB->CCR & SCB_CCR_DC_Msk) {
        uint32_t start = (uint32_t)config_flash_area_ptr(area) + offset;
        uint32_t end   = start + len;
        start &= ~31u;   /* cache line = 32 bytes */
        SCB_InvalidateDCache_by_Addr((uint32_t *)start, (int32_t)(end - start));
    }
}

uint32_t config_flash_area_size(void) {
    return AREA_SIZE;
}

const uint8_t *config_flash_area_ptr(uint8_t area) {
    return __config_store_start + (uint32_t)area * AREA_SIZE;
}

bool config_flash_erase(uint8_t area) {
    if (area >= CONFIG_FLASH_AREAS) return false;

    FLASH_EraseInitTypeDef erase = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .Sector       = k_sector[area],
        .NbSectors    = 1,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,   /* 2.7–3.6 V, x32 parallelism */
    };
    uint32_t bad_sector;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_ALL_ERRORS);
    HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&erase, &bad_sector);
    HAL_FLASH_Lock();

    invalidate(area, 0, AREA_SIZE);
    return st == HAL_OK;
}

bool config_flash_program(uint8_t area, uint32_t offset, const uint32_t *words, uint32_t count) {
    if (area >= CONFIG_FLASH_AREAS || (offset & 3u) || offset + count * 4u > AREA_SIZE) return false;

    uint32_t addr = (uint32_t)config_flash_area_ptr(area) + offset;
    bool ok = true;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_ALL_ERRORS);
    for (uint32_t i = 0; i < count && ok; i++)
        ok = (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i * 4u, words[i]) == HAL_OK);
    HAL_FLASH_Lock();

    invalidate(area, offset, count * 4u);
    return ok && memcmp((const void *)addr, words, count * 4u) == 0;
}
