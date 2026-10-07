/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#ifndef MXGPU_APERTURE_H
#define MXGPU_APERTURE_H
#include "mxgpu_wire.h"

#define MXGPU_POWER_ON_SNAPSHOT_SIZE (MXGPU_AREG_POWER_ON_FORMAT + 4u - MXGPU_AREG_POWER_ON_BASE_LOW)
struct mxgpu_power_on_aperture {
    uint64_t base;
    uint64_t bytes;
    uint32_t stride;
    uint32_t width;
    uint32_t height;
    uint32_t format;
};

/* Input holds the six read-only register words in ascending register order,
 * serialized little-endian by the caller after a coherent snapshot. Zero words
 * describe absence and decode successfully to an empty result. Capacity is the
 * number of bytes the caller can map at the published base. Failure preserves out. */
#ifdef __cplusplus
extern "C" {
#endif
int mxgpu_power_on_aperture_decode(const uint8_t *in, uint32_t length,
    uint64_t capacity, struct mxgpu_power_on_aperture *out);
#ifdef __cplusplus
}
#endif
#endif
