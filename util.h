#ifndef yaspl_util_h
#define yaspl_uitl_h

#include <string.h>
#include <stdio.h>

#include "commontypes.h"

typedef struct {
    uint8_t b0;
    uint8_t b1;
} SplittedLong;

#define join_bytes(b0, b1) ((uint16_t) (((uint16_t) (b0)) << 8) | ((uint16_t) (b1)))
static inline SplittedLong split_long(uint16_t l) {
    return (SplittedLong) {.b0 = (uint8_t) ((l & 0xff00) >> 8), .b1 = (uint8_t) (l & 0xff)};
}

static inline uint32_t hash_int(uint32_t a) {
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}
#define hash_pointer(p) hash_int((uint32_t) (p))
#define hash_double(v) hash_int(float_to_bits((float) v))

static inline uint32_t float_to_bits(float v) {
    uint32_t i = 0;
    memcpy(&i, &v, sizeof(float));
    return i;
}

static inline int is_integer(double n) {
    return (int) n == n;
}

#endif
