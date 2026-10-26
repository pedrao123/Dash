#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <stdint.h>
#include <stdbool.h>

/* Append-only log of config records in two alternating flash areas.
 * Stores an opaque blob plus its format version — knows nothing about
 * dash_config_t, the parser or the rest of the firmware.
 *
 * Record: magic | version | len | seq | payload (padded to 4) | CRC32.
 * At boot the valid record (CRC ok) with the highest seq wins. When the
 * active area is full the other one is erased and the new record goes there,
 * so a power loss at any point leaves the previous record intact.
 *
 * Not thread-safe: call from a single task. */

#define CONFIG_STORE_MAX_LEN  512u   /* largest payload, bytes */

/* Scans the flash. Call once before load/save. */
void config_store_init(void);

/* Copies the newest valid record into out (at most max bytes). *len receives
 * the stored length (may exceed max), *version its format version.
 * Returns false if the flash holds no valid record. */
bool config_store_load(void *out, uint16_t max, uint16_t *len, uint16_t *version);

/* Appends a new record. May erase a 128 KB sector first (~1 s, CPU stalls). */
bool config_store_save(const void *data, uint16_t len, uint16_t version);

#endif /* CONFIG_STORE_H */
