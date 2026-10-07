/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxgpu_aperture.h"
#include "mx_le.h"

int mxgpu_power_on_aperture_decode(const uint8_t *in, uint32_t length,
    uint64_t capacity, struct mxgpu_power_on_aperture *out)
{
    struct mxgpu_power_on_aperture candidate;
    if (!in || !out) return MX_ERR_STATE;
    if (length != MXGPU_POWER_ON_SNAPSHOT_SIZE) return MX_ERR_LENGTH;
    candidate.base=mx_r64(in,0);
    candidate.stride=mx_r32(in,8);
    candidate.width=mx_r32(in,12);
    candidate.height=mx_r32(in,16);
    candidate.format=mx_r32(in,20);
    candidate.bytes=(uint64_t)candidate.stride*candidate.height;
    if (!candidate.base && !candidate.stride && !candidate.width &&
        !candidate.height && candidate.format == MXGPU_APERTURE_FORMAT_NONE) {
        *out=candidate;
        return MX_OK;
    }
    if (!candidate.base || candidate.base%MXGPU_APERTURE_BASE_ALIGNMENT)
        return MX_ERR_RANGE;
    if (!candidate.width || !candidate.height || candidate.width > MXGPU_MAX_APERTURE_EDGE ||
        candidate.height > MXGPU_MAX_APERTURE_EDGE ||
        candidate.stride < candidate.width*MXGPU_APERTURE_BYTES_PER_PIXEL ||
        candidate.stride%MXGPU_APERTURE_BYTES_PER_PIXEL) return MX_ERR_SHAPE;
    if (candidate.format != MXGPU_FMT_BGRA8_UNORM) return MX_ERR_FEATURE;
    if (candidate.bytes > MXGPU_MAX_APERTURE_BYTES || candidate.bytes > capacity)
        return MX_ERR_LIMIT;
    if (candidate.base > UINT64_MAX-candidate.bytes) return MX_ERR_RANGE;
    *out=candidate;
    return MX_OK;
}
