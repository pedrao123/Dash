#include "config_store.h"
#include "config_flash.h"
#include <string.h>

#define REC_MAGIC   0x47464344u   /* "DCFG" little-endian */
#define ERASED_WORD 0xFFFFFFFFu
#define NO_SPACE    0xFFFFFFFFu

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t len;      /* payload bytes, without padding */
    uint32_t seq;
} rec_hdr_t;           /* followed by payload (padded to 4) and CRC32 of hdr+payload */

#define PAD4(n)        (((uint32_t)(n) + 3u) & ~3u)
#define REC_BYTES(len) ((uint32_t)sizeof(rec_hdr_t) + PAD4(len) + 4u)

static struct {
    bool     found;    /* a valid record exists */
    uint8_t  area;     /* area of the newest record */
    uint32_t offset;   /* its offset */
    uint32_t seq;      /* its sequence number */
    uint8_t  active;   /* area being appended to */
    uint32_t free;     /* append offset in active, or NO_SPACE */
} s;

/* Record is assembled here before programming (word-aligned). */
static uint32_t s_rec[REC_BYTES(CONFIG_STORE_MAX_LEN) / 4u];

//  CRC32 (IEEE 802.3, reflected) — bitwise, records are small

static uint32_t crc32(const uint8_t *p, uint32_t n) {
    uint32_t crc = 0xFFFFFFFFu;
    while (n--) {
        crc ^= *p++;
        for (uint8_t b = 0; b < 8u; b++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

//  Scan

/* Walks one area. Updates s.found/area/offset/seq with any newer valid record.
 * Returns the offset where the next record can be appended, or NO_SPACE if
 * the log is damaged (e.g. a header torn by a power loss). */
static uint32_t scan_area(uint8_t area) {
    const uint8_t *base = config_flash_area_ptr(area);
    uint32_t size = config_flash_area_size();
    uint32_t pos  = 0;

    while (pos + sizeof(rec_hdr_t) <= size) {
        rec_hdr_t h;
        memcpy(&h, base + pos, sizeof(h));

        if (h.magic == ERASED_WORD) return pos;   /* end of log */
        if (h.magic != REC_MAGIC || h.len > CONFIG_STORE_MAX_LEN || pos + REC_BYTES(h.len) > size)
            return NO_SPACE;

        uint32_t body = (uint32_t)sizeof(h) + PAD4(h.len);
        uint32_t crc;
        memcpy(&crc, base + pos + body, 4);
        if (crc == crc32(base + pos, body) && (!s.found || h.seq > s.seq)) {
            s.found  = true;
            s.area   = area;
            s.offset = pos;
            s.seq    = h.seq;
        }
        pos += REC_BYTES(h.len);   /* torn records (bad CRC) are skipped */
    }
    return NO_SPACE;
}

static bool is_erased(uint8_t area, uint32_t offset, uint32_t len) {
    const uint8_t *p = config_flash_area_ptr(area) + offset;
    for (uint32_t i = 0; i < len; i++)
        if (p[i] != 0xFFu) return false;
    return true;
}

//  Public API

void config_store_init(void) {
    memset(&s, 0, sizeof(s));

    uint32_t free[CONFIG_FLASH_AREAS];
    for (uint8_t a = 0; a < CONFIG_FLASH_AREAS; a++)
        free[a] = scan_area(a);

    s.active = s.found ? s.area : 0u;
    s.free   = free[s.active];
}

bool config_store_load(void *out, uint16_t max, uint16_t *len, uint16_t *version) {
    if (!s.found) return false;

    rec_hdr_t h;
    const uint8_t *rec = config_flash_area_ptr(s.area) + s.offset;
    memcpy(&h, rec, sizeof(h));
    memcpy(out, rec + sizeof(h), h.len < max ? h.len : max);
    if (len)     *len     = h.len;
    if (version) *version = h.version;
    return true;
}

bool config_store_save(const void *data, uint16_t len, uint16_t version) {
    if (len > CONFIG_STORE_MAX_LEN) return false;

    uint32_t bytes = REC_BYTES(len);
    uint32_t body  = bytes - 4u;
    rec_hdr_t h = {
        .magic   = REC_MAGIC,
        .version = version,
        .len     = len,
        .seq     = s.found ? s.seq + 1u : 1u,
    };
    memset(s_rec, 0, bytes);   /* zero padding */
    memcpy(s_rec, &h, sizeof(h));
    memcpy((uint8_t *)s_rec + sizeof(h), data, len);
    s_rec[body / 4u] = crc32((const uint8_t *)s_rec, body);

    /* Append to the active area if the record fits in clean flash;
     * otherwise erase the other area and start a fresh log there. The
     * active area keeps the newest record until the new one is complete. */
    uint8_t  area   = s.active;
    uint32_t offset = s.free;
    if (offset == NO_SPACE || offset + bytes > config_flash_area_size() || !is_erased(area, offset, bytes)) {
        area   = (uint8_t)(s.active ^ 1u);
        offset = 0;
        if (!config_flash_erase(area)) return false;
    }

    if (!config_flash_program(area, offset, s_rec, bytes / 4u)) {
        if (area == s.active) s.free = NO_SPACE;   /* next save switches areas */
        return false;
    }

    s.found  = true;
    s.area   = area;
    s.offset = offset;
    s.seq    = h.seq;
    s.active = area;
    s.free   = offset + bytes;
    return true;
}
