/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxgpu_wire.h"
#include "mx_le.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    uint8_t bytes[128], saved[128], code[8] = {1,2,3,4,5,6,7,8};
    struct mxgpu_shader_create shader, shader_saved;
    struct mxgpu_pipeline_create pipeline, pipeline_saved;
    struct mxgpu_transfer transfer = {0}, decoded, decoded_saved;
    uint32_t written, i;
    memset(bytes, 0, sizeof(bytes));
    memset(&shader, 0x5a, sizeof(shader)); shader_saved = shader;
    assert(mxgpu_shader_create_encode(7, code, sizeof(code), bytes, sizeof(bytes), &written) == MX_OK);
    assert(written == 24);
    assert(mxgpu_shader_create_decode(bytes, written, 8, &shader) == MX_OK);
    assert(shader.shader_id == 7 && shader.bytecode_bytes == 8 && shader.bytecode == bytes + 16);
    assert(memcmp(shader.bytecode, code, 8) == 0);
    shader_saved = shader;
    for (i = 0; i < 24; ++i) {
        assert(mxgpu_shader_create_decode(bytes, i, 8, &shader) != MX_OK);
        assert(memcmp(&shader, &shader_saved, sizeof(shader)) == 0);
    }
    assert(mxgpu_shader_create_decode(bytes, 24, 7, &shader) == MX_ERR_LENGTH);
    for (i = 8; i < 16; ++i) {
        bytes[i] = 1;
        assert(mxgpu_shader_create_decode(bytes, 24, 8, &shader) == MX_ERR_RESERVED);
        bytes[i] = 0;
    }
    mx_w32(bytes, 4, 7);
    assert(mxgpu_shader_create_decode(bytes, 24, 8, &shader) == MX_ERR_SHADER);
    mx_w32(bytes, 4, UINT32_MAX - 3u);
    assert(mxgpu_shader_create_decode(bytes, 24, UINT32_MAX, &shader) == MX_ERR_LENGTH);
    assert(mxgpu_shader_create_encode(7, code, UINT32_MAX - 3u, bytes, sizeof(bytes), &written) == MX_ERR_LENGTH);
    mx_w32(bytes, 4, 8); mx_w32(bytes, 0, 0);
    assert(mxgpu_shader_create_decode(bytes, 24, 8, &shader) == MX_ERR_SHADER);
    memcpy(saved, bytes, sizeof(bytes));
    assert(mxgpu_shader_create_decode(bytes, 24, 8, (struct mxgpu_shader_create *)(void *)bytes) == MX_ERR_STATE);
    assert(memcmp(saved, bytes, sizeof(bytes)) == 0);
    assert(mxgpu_shader_create_decode((const uint8_t *)(UINTPTR_MAX - 3u), 24, 8, &shader) == MX_ERR_STATE);

    memset(&pipeline, 0, sizeof(pipeline));
    assert(mxgpu_pipeline_create_encode(9, MXGPU_PIPELINE_RENDER, MXGPU_FMT_RGBA8_UNORM,
        7, 1, 2, bytes, sizeof(bytes), &written) == MX_OK);
    assert(mxgpu_pipeline_create_decode(bytes, written, &pipeline) == MX_OK);
    assert(pipeline.pipeline_id == 9 && pipeline.shader_id == 7 && pipeline.first_entry == 1 && pipeline.second_entry == 2);
    pipeline_saved = pipeline;
    for (i = 0; i < 32; ++i) {
        assert(mxgpu_pipeline_create_decode(bytes, i, &pipeline) != MX_OK);
        assert(memcmp(&pipeline, &pipeline_saved, sizeof(pipeline)) == 0);
    }
    for (i = 20; i < 32; ++i) {
        bytes[i] = 1;
        assert(mxgpu_pipeline_create_decode(bytes, 32, &pipeline) == MX_ERR_RESERVED);
        bytes[i] = 0;
    }
    mx_w32(bytes, 16, 1);
    assert(mxgpu_pipeline_create_decode(bytes, 32, &pipeline) == MX_ERR_STATE);
    mx_w32(bytes, 16, 2); mx_w16(bytes, 6, UINT16_MAX);
    assert(mxgpu_pipeline_create_decode(bytes, 32, &pipeline) == MX_ERR_STATE);
    assert(mxgpu_pipeline_create_encode(9, MXGPU_PIPELINE_COMPUTE, 0, 7, 1, 0, bytes, sizeof(bytes), &written) == MX_OK);
    assert(mxgpu_pipeline_create_decode(bytes, written, &pipeline) == MX_OK);
    mx_w32(bytes, 16, 2);
    assert(mxgpu_pipeline_create_decode(bytes, written, &pipeline) == MX_ERR_STATE);
    memcpy(saved, bytes, sizeof(bytes));
    assert(mxgpu_pipeline_create_decode(bytes, 32, (struct mxgpu_pipeline_create *)(void *)bytes) == MX_ERR_STATE);
    assert(memcmp(saved, bytes, sizeof(bytes)) == 0);

    transfer.resource_id = 11; transfer.resource_offset = UINT64_MAX; transfer.data_bytes = 8;
    transfer.mip_level = UINT16_MAX; transfer.array_layer = UINT16_MAX;
    transfer.x = 1; transfer.y = 2; transfer.z = 3;
    transfer.width = 4; transfer.height = 5; transfer.depth = 6; transfer.row_bytes = 7;
    assert(mxgpu_transfer_request_encode(&transfer, bytes, sizeof(bytes), &written) == MX_OK && written == 48);
    assert(mx_r32(bytes, 0) == 11 && mx_r64(bytes, 32) == UINT64_MAX && mx_r32(bytes, 44) == 8);
    assert(mxgpu_transfer_request_decode(bytes, written, &decoded) == MX_OK);
    assert(decoded.resource_id == 11 && decoded.resource_offset == UINT64_MAX && decoded.array_layer == UINT16_MAX);
    decoded_saved = decoded;
    for (i = 0; i < 48; ++i) {
        assert(mxgpu_transfer_request_decode(bytes, i, &decoded) == MX_ERR_LENGTH);
        assert(memcmp(&decoded, &decoded_saved, sizeof(decoded)) == 0);
    }
    assert(mxgpu_transfer_request_decode(bytes, 49, &decoded) == MX_ERR_LENGTH);
    memcpy(saved, bytes, sizeof(bytes)); written = 99;
    assert(mxgpu_transfer_request_encode(&transfer, bytes, 47, &written) == MX_ERR_LENGTH);
    assert(written == 99 && memcmp(saved, bytes, sizeof(bytes)) == 0);
    assert(mxgpu_transfer_request_encode(&transfer, bytes, sizeof(bytes), (uint32_t *)(void *)bytes) == MX_ERR_STATE);
    assert(mxgpu_transfer_request_decode(bytes, 48, (struct mxgpu_transfer *)(void *)bytes) == MX_ERR_STATE);
    assert(memcmp(saved, bytes, sizeof(bytes)) == 0);
    mx_w32(bytes, 44, 0);
    assert(mxgpu_transfer_request_decode(bytes, 48, &decoded) == MX_ERR_RANGE);
    mx_w32(bytes, 44, 8); mx_w32(bytes, 0, 0);
    assert(mxgpu_transfer_request_decode(bytes, 48, &decoded) == MX_ERR_RANGE);
    assert(mxgpu_transfer_encode(&transfer, code, bytes, sizeof(bytes), &written) == MX_OK && written == 56);
    assert(mxgpu_transfer_decode(bytes, written, &decoded, 0) == MX_OK);
    assert(mxgpu_transfer_request_decode(bytes, written, &decoded) == MX_ERR_LENGTH);
    return 0;
}
