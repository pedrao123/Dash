#ifndef CONFIG_CMD_H
#define CONFIG_CMD_H

#include <stdint.h>

/* Text protocol for runtime configuration. One command per line (\r or \n).
 * Every command ends with a final "OK" or "ERRO <motivo>" line, optionally
 * preceded by key=value lines:
 *
 *   GET <key>            SET <key> <value>     DUMP
 *   DEFAULTS             INFO                  SAVE
 *
 * SET and DEFAULTS apply immediately (RAM only); nothing is persisted outside SAVE.
 * Knows nothing about the UART: output goes through the write callback. */

typedef void (*config_cmd_write_fn)(const char *data, uint16_t len);

void config_cmd_init(config_cmd_write_fn write);

/* Task context. Feed raw bytes; each complete line is executed immediately. */
void config_cmd_input(const uint8_t *data, uint16_t len);

#endif /* CONFIG_CMD_H */
