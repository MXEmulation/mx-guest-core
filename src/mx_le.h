/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#ifndef MX_LE_H
#define MX_LE_H

#include <stdint.h>

static inline void mx_w16(uint8_t *p, uint32_t off, uint16_t v)
{
    p[off] = (uint8_t)v;
    p[off + 1] = (uint8_t)(v >> 8);
}

static inline void mx_w32(uint8_t *p, uint32_t off, uint32_t v)
{
    p[off] = (uint8_t)v;
    p[off + 1] = (uint8_t)(v >> 8);
    p[off + 2] = (uint8_t)(v >> 16);
    p[off + 3] = (uint8_t)(v >> 24);
}

static inline void mx_w64(uint8_t *p, uint32_t off, uint64_t v)
{
    mx_w32(p, off, (uint32_t)v);
    mx_w32(p, off + 4, (uint32_t)(v >> 32));
}

static inline uint16_t mx_r16(const uint8_t *p, uint32_t off)
{
    return (uint16_t)p[off] | ((uint16_t)p[off + 1] << 8);
}

static inline uint32_t mx_r32(const uint8_t *p, uint32_t off)
{
    return (uint32_t)p[off] | ((uint32_t)p[off + 1] << 8) | ((uint32_t)p[off + 2] << 16) |
           ((uint32_t)p[off + 3] << 24);
}

static inline uint64_t mx_r64(const uint8_t *p, uint32_t off)
{
    return (uint64_t)mx_r32(p, off) | ((uint64_t)mx_r32(p, off + 4) << 32);
}

#endif
