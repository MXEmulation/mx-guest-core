/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxgpu_aperture.h"
#include "mx_le.h"
#include <assert.h>
#include <string.h>

_Static_assert(MXGPU_FIRMWARE_SCANOUT_ID == 0u, "firmware scanout ordinal");

int main(void)
{
    uint8_t bytes[MXGPU_POWER_ON_SNAPSHOT_SIZE]={0};
    struct mxgpu_power_on_aperture result,saved;
    assert(mxgpu_power_on_aperture_decode(bytes,sizeof(bytes),0,&result) == MX_OK);
    assert(!result.base && !result.bytes && !result.format);
    mx_w64(bytes,0,0x123450000ull);
    mx_w32(bytes,8,2560); mx_w32(bytes,12,640); mx_w32(bytes,16,480);
    mx_w32(bytes,20,MXGPU_FMT_BGRA8_UNORM);
    assert(mxgpu_power_on_aperture_decode(bytes,sizeof(bytes),1228800,&result) == MX_OK);
    assert(result.base == 0x123450000ull && result.bytes == 1228800 && result.width == 640);
    saved=result;
    assert(mxgpu_power_on_aperture_decode(bytes,sizeof(bytes),1228799,&result) == MX_ERR_LIMIT);
    assert(!memcmp(&result,&saved,sizeof(result)));
    assert(mxgpu_power_on_aperture_decode(bytes,sizeof(bytes)-1,UINT64_MAX,&result) == MX_ERR_LENGTH);
    mx_w32(bytes,8,2559);
    assert(mxgpu_power_on_aperture_decode(bytes,sizeof(bytes),UINT64_MAX,&result) == MX_ERR_SHAPE);
    mx_w32(bytes,8,2560); mx_w32(bytes,20,MXGPU_FMT_RGBA8_UNORM);
    assert(mxgpu_power_on_aperture_decode(bytes,sizeof(bytes),UINT64_MAX,&result) == MX_ERR_FEATURE);
    mx_w32(bytes,20,MXGPU_FMT_BGRA8_UNORM); mx_w64(bytes,0,UINT64_MAX-4095);
    assert(mxgpu_power_on_aperture_decode(bytes,sizeof(bytes),UINT64_MAX,&result) == MX_ERR_RANGE);
    mx_w64(bytes,0,0x1001);
    assert(mxgpu_power_on_aperture_decode(bytes,sizeof(bytes),UINT64_MAX,&result) == MX_ERR_RANGE);
    mx_w64(bytes,0,0x1000); mx_w32(bytes,12,MXGPU_MAX_APERTURE_EDGE+1);
    assert(mxgpu_power_on_aperture_decode(bytes,sizeof(bytes),UINT64_MAX,&result) == MX_ERR_SHAPE);
    mx_w32(bytes,12,1); mx_w32(bytes,16,2); mx_w32(bytes,8,MXGPU_MAX_APERTURE_BYTES);
    assert(mxgpu_power_on_aperture_decode(bytes,sizeof(bytes),UINT64_MAX,&result) == MX_ERR_LIMIT);
    assert(!memcmp(&result,&saved,sizeof(result)));
    memset(bytes,0,sizeof(bytes)); mx_w32(bytes,20,MXGPU_FMT_BGRA8_UNORM);
    assert(mxgpu_power_on_aperture_decode(bytes,sizeof(bytes),UINT64_MAX,&result) == MX_ERR_RANGE);
    assert(mxgpu_power_on_aperture_decode(NULL,0,0,&result) == MX_ERR_STATE);
    return 0;
}
