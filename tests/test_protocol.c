/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxgpu_wire.h"
#include "mxsb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fixture_inst(uint32_t *dst, uint32_t *n, uint16_t opcode, const uint32_t *ops,
                         uint32_t count)
{
    uint32_t i;
    dst[0] = ((count + 1u) << 16) | opcode;
    for (i = 0; i < count; i++)
        dst[1 + i] = ops[i];
    *n = count + 1u;
}

static int fixture_textured_program_encode(uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint32_t words[256];
    struct mxsb_writer writer;
    uint32_t recs[12][8];
    uint32_t lens[12];
    const uint32_t *ptrs[12];
    uint32_t frecs[3][8];
    uint32_t flens[3];
    const uint32_t *fptrs[3];
    uint32_t ops[6];
    uint32_t i;
    if (mxsb_writer_init(&writer, words, 256, 2) != MXSB_OK)
        return MXSB_ERR_LIMIT;
    if (mxsb_writer_binding(&writer, 1, 0, MXSB_BINDING_UNIFORM, MXSB_ACCESS_READ, MXSB_TYPE_F32X4,
                            3) != MXSB_OK)
        return MXSB_ERR_BINDING;
    if (mxsb_writer_binding(&writer, 2, 1, MXSB_BINDING_TEXTURE_2D, MXSB_ACCESS_READ,
                            MXSB_TYPE_F32X4, 0) != MXSB_OK)
        return MXSB_ERR_BINDING;
    if (mxsb_writer_entry(&writer, 1, MXSB_STAGE_VERTEX, 1) != MXSB_OK)
        return MXSB_ERR_STRUCTURE;
    if (mxsb_writer_entry(&writer, 2, MXSB_STAGE_FRAGMENT, 2) != MXSB_OK)
        return MXSB_ERR_STRUCTURE;
    ops[0] = 1;
    ops[1] = MXSB_TYPE_U32;
    ops[2] = MXSB_BUILTIN_VERTEX_ID;
    fixture_inst(recs[0], &lens[0], MXSB_OP_BUILTIN, ops, 3);
    ops[0] = 2;
    ops[1] = MXSB_TYPE_F32X4;
    ops[2] = 1;
    ops[3] = 1;
    fixture_inst(recs[1], &lens[1], MXSB_OP_BUFFER_LOAD, ops, 4);
    for (i = 0; i < 4; i++) {
        ops[0] = 3 + i;
        ops[1] = MXSB_TYPE_F32;
        ops[2] = 2;
        ops[3] = i;
        fixture_inst(recs[2 + i], &lens[2 + i], MXSB_OP_EXTRACT, ops, 4);
    }
    ops[0] = 7;
    ops[1] = MXSB_TYPE_F32;
    ops[2] = 0;
    fixture_inst(recs[6], &lens[6], MXSB_OP_CONSTANT, ops, 3);
    ops[0] = 8;
    ops[1] = MXSB_TYPE_F32;
    ops[2] = 0x3f800000u;
    fixture_inst(recs[7], &lens[7], MXSB_OP_CONSTANT, ops, 3);
    {
        uint32_t wide[8];
        wide[0] = (8u << 16) | MXSB_OP_CONSTRUCT;
        wide[1] = 9;
        wide[2] = MXSB_TYPE_F32X4;
        wide[3] = 4;
        wide[4] = 3;
        wide[5] = 4;
        wide[6] = 7;
        wide[7] = 8;
        memcpy(recs[8], wide, sizeof wide);
        lens[8] = 8;
    }
    {
        uint32_t wide[6];
        wide[0] = (6u << 16) | MXSB_OP_CONSTRUCT;
        wide[1] = 10;
        wide[2] = MXSB_TYPE_F32X2;
        wide[3] = 2;
        wide[4] = 5;
        wide[5] = 6;
        memcpy(recs[9], wide, sizeof wide);
        lens[9] = 6;
    }
    ops[0] = 0;
    ops[1] = 10;
    fixture_inst(recs[10], &lens[10], MXSB_OP_STAGE_OUTPUT, ops, 2);
    ops[0] = 9;
    fixture_inst(recs[11], &lens[11], MXSB_OP_RETURN_VALUE, ops, 1);
    for (i = 0; i < 12; i++)
        ptrs[i] = recs[i];
    if (mxsb_writer_block(&writer, 1, 1, ptrs, lens, 12) != MXSB_OK)
        return MXSB_ERR_STRUCTURE;
    ops[0] = 1;
    ops[1] = MXSB_TYPE_F32X2;
    ops[2] = 0;
    ops[3] = MXSB_INTERP_PERSPECTIVE;
    fixture_inst(frecs[0], &flens[0], MXSB_OP_STAGE_INPUT, ops, 4);
    {
        uint32_t wide[6];
        wide[0] = (6u << 16) | MXSB_OP_TEXTURE_SAMPLE;
        wide[1] = 2;
        wide[2] = MXSB_TYPE_F32X4;
        wide[3] = 2;
        wide[4] = 1;
        wide[5] = 1u | (1u << 4) | (1u << 8) | (1u << 12);
        memcpy(frecs[1], wide, sizeof wide);
        flens[1] = 6;
    }
    ops[0] = 2;
    fixture_inst(frecs[2], &flens[2], MXSB_OP_RETURN_VALUE, ops, 1);
    for (i = 0; i < 3; i++)
        fptrs[i] = frecs[i];
    if (mxsb_writer_block(&writer, 2, 2, fptrs, flens, 3) != MXSB_OK)
        return MXSB_ERR_STRUCTURE;
    return mxsb_writer_finish(&writer, out, cap, out_len);
}

static int failures;

static void expect(int cond, const char *what)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", what);
        failures++;
    }
}

static uint8_t *load(const char *dir, const char *name, uint32_t *len)
{
    char path[512];
    FILE *file;
    long size;
    uint8_t *buf;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "missing fixture %s\n", path);
        failures++;
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    buf = malloc((size_t)size);
    if (!buf || fread(buf, 1, (size_t)size, file) != (size_t)size) {
        fprintf(stderr, "read %s\n", path);
        failures++;
        fclose(file);
        free(buf);
        return NULL;
    }
    fclose(file);
    *len = (uint32_t)size;
    return buf;
}

static void expect_bytes(const char *dir, const char *name, const uint8_t *got, uint32_t got_len)
{
    uint32_t len = 0;
    uint8_t *want = load(dir, name, &len);
    if (!want)
        return;
    if (got_len != len || memcmp(got, want, len) != 0) {
        fprintf(stderr, "FAIL bytes %s got %u want %u\n", name, got_len, len);
        failures++;
    }
    free(want);
}

static void test_roundtrip(const char *dir)
{
    struct mxgpu_negotiation_request request;
    struct mxgpu_negotiated caps;
    struct mxgpu_resource_create resource;
    struct mxgpu_resource_bind bind;
    struct mxgpu_command_header header;
    struct mxgpu_descriptor descriptor;
    struct mxgpu_transfer transfer;
    struct mxgpu_execution_binding buffer_binding;
    struct mxgpu_execution_binding texture_binding;
    struct mxgpu_render_submit submit;
    struct mxgpu_present present;
    struct mxgpu_present_rect damage;
    struct mxsb_limits limits;
    uint8_t out[1024];
    uint8_t poison[1024];
    uint8_t payload[128];
    uint32_t n = 0;
    uint32_t fixture_len = 0;
    uint8_t *fixture;
    const uint8_t *decoded_payload = NULL;
    uint32_t decoded_len = 0;
    uint8_t pixel[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    int status;

    memset(&request, 0, sizeof request);
    request.minimum_major = 1;
    request.maximum_major = 1;
    request.minimum_minor = 0;
    request.maximum_minor = MXGPU_PROTOCOL_VERSION_MINOR;
    request.requested_features = MXGPU_FEAT_RENDER | MXGPU_FEAT_SCANOUT | MXGPU_FEAT_CURSOR |
                                 MXGPU_FEAT_FENCE_SIGNAL | MXGPU_FEAT_RESOURCE_PLACEMENT |
                                 MXGPU_FEAT_MULTI_QUEUE | MXGPU_FEAT_COLOR_CLEAR;
    request.required_features = MXGPU_FEAT_RENDER | MXGPU_FEAT_RESOURCE_PLACEMENT;
    status = mxgpu_negotiation_request_encode(&request, out, sizeof out, &n);
    expect(status == MX_OK, "request encode");
    expect_bytes(dir, "negotiation-request.bin", out, n);
    expect(mxgpu_negotiation_request_decode(out, n, &request) == MX_OK, "request decode");

    memset(&caps, 0, sizeof caps);
    caps.major = 1;
    caps.minor = MXGPU_PROTOCOL_VERSION_MINOR;
    caps.features = request.requested_features;
    caps.host_event_features = MXGPU_HOST_EVENT_SCANOUT_MODE_REQUEST;
    caps.limits.max_queues = 6;
    caps.limits.max_scanouts = 1;
    caps.limits.max_contexts = 64;
    caps.limits.max_resources = 4096;
    caps.limits.max_descriptors_per_queue = 256;
    caps.limits.max_command_bytes = 65536;
    caps.limits.max_inline_bytes = 4096;
    caps.limits.max_transfer_to_host_bytes = 4016;
    caps.limits.max_transfer_from_host_bytes = 4096;
    caps.limits.max_resource_bytes = 64ull * 1024ull * 1024ull;
    caps.limits.max_resident_bytes = 256ull * 1024ull * 1024ull;
    status = mxgpu_negotiation_response_encode(&caps, out, sizeof out, &n);
    expect(status == MX_OK, "response encode");
    expect_bytes(dir, "negotiation-response.bin", out, n);
    expect(mxgpu_negotiation_response_decode(out, n, &caps) == MX_OK, "response decode");
    expect(caps.minor == MXGPU_PROTOCOL_VERSION_MINOR, "response minor");

    caps.status = 5;
    caps.minor = 0;
    status = mxgpu_negotiation_response_encode(&caps, out, sizeof out, &n);
    expect(status == MX_OK, "reject encode");
    expect_bytes(dir, "negotiation-reject.bin", out, n);

    fixture = load(dir, "negotiation-bad-magic.bin", &fixture_len);
    expect(fixture &&
               mxgpu_negotiation_request_decode(fixture, fixture_len, &request) == MX_ERR_MAGIC,
           "bad magic refused");
    free(fixture);

    memset(&resource, 0, sizeof resource);
    resource.resource_id = 7;
    resource.kind = MXGPU_KIND_TEXTURE_2D;
    resource.format = MXGPU_FMT_RGBA8_UNORM;
    resource.usage =
        MXGPU_USAGE_TRANSFER_DESTINATION | MXGPU_USAGE_SAMPLED | MXGPU_USAGE_COLOR_TARGET;
    resource.width = 64;
    resource.height = 64;
    resource.depth = 1;
    resource.array_layers = 1;
    resource.mip_levels = 1;
    resource.sample_count = 1;
    resource.byte_size = 64ull * 64ull * 4ull;
    status =
        mxgpu_resource_create_encode(&resource, 64ull * 1024ull * 1024ull, out, sizeof out, &n);
    expect(status == MX_OK, "resource encode");
    expect_bytes(dir, "resource-create.bin", out, n);
    expect(mxgpu_resource_create_decode(out, n, 64ull * 1024ull * 1024ull, &resource) == MX_OK,
           "resource decode");

    memset(poison, 0xa5, sizeof poison);
    resource.resource_id = 0;
    status = mxgpu_resource_create_encode(&resource, 64ull * 1024ull * 1024ull, poison,
                                          sizeof poison, &n);
    expect(status == MX_ERR_RESOURCE && n == 0 && poison[0] == 0xa5,
           "zero resource id emits nothing");
    fixture = load(dir, "resource-create-zero-id.bin", &fixture_len);
    expect(fixture && mxgpu_resource_create_decode(fixture, fixture_len, UINT64_MAX, &resource) ==
                          MX_ERR_RESOURCE,
           "zero id fixture refused");
    free(fixture);

    memset(&bind, 0, sizeof bind);
    bind.resource_id = 7;
    bind.gpu_va = MXGPU_PLACEMENT_APERTURE_BASE;
    bind.byte_size = 0x1000;
    status = mxgpu_resource_bind_encode(&bind, out, sizeof out, &n);
    expect(status == MX_OK, "bind encode");
    expect_bytes(dir, "resource-bind.bin", out, n);
    expect(mxgpu_resource_bind_decode(out, n, &bind) == MX_OK, "bind decode");
    bind.byte_size = 0;
    memset(poison, 0xa5, 32);
    status = mxgpu_resource_bind_encode(&bind, poison, 32, &n);
    expect(status != MX_OK && n == 0 && poison[0] == 0xa5, "empty bind emits nothing");

    status = mxgpu_resource_id_encode(7, out, sizeof out, &n);
    expect(status == MX_OK, "id encode");
    expect_bytes(dir, "resource-id.bin", out, n);

    resource.resource_id = 7;
    mxgpu_resource_create_encode(&resource, 64ull * 1024ull * 1024ull, payload, sizeof payload, &n);
    memset(&header, 0, sizeof header);
    header.opcode = MXGPU_OP_RESOURCE_CREATE;
    header.flags = MXGPU_CMD_SIGNAL_FENCE | MXGPU_CMD_RESPONSE_REQUIRED;
    header.context_id = 3;
    header.queue = MXGPU_QUEUE_CONTROL;
    header.sequence = 1;
    header.fence_value = 9;
    status = mxgpu_command_encode(&header, payload, n, 65536, out, sizeof out, &decoded_len);
    expect(status == MX_OK, "command encode");
    expect_bytes(dir, "command-resource-create.bin", out, decoded_len);
    expect(mxgpu_command_decode(out, decoded_len, 65536, &header, &decoded_payload, &n) == MX_OK,
           "command decode");
    expect(n == MXGPU_RESOURCE_CREATE_SIZE && decoded_payload &&
               memcmp(decoded_payload, payload, n) == 0,
           "command payload");

    fixture = load(dir, "command-zero-sequence.bin", &fixture_len);
    expect(fixture && mxgpu_command_decode(fixture, fixture_len, 65536, &header, NULL, NULL) ==
                          MX_ERR_SEQUENCE,
           "zero sequence refused");
    free(fixture);
    header.sequence = 0;
    memset(poison, 0xa5, sizeof poison);
    status = mxgpu_command_encode(&header, payload, MXGPU_RESOURCE_CREATE_SIZE, 65536, poison,
                                  sizeof poison, &n);
    expect(status == MX_ERR_SEQUENCE && n == 0 && poison[0] == 0xa5,
           "illegal command emits nothing");

    memset(&descriptor, 0, sizeof descriptor);
    descriptor.address = 0x1000;
    descriptor.byte_len = 64;
    descriptor.flags = MXGPU_DESC_DEVICE_WRITE;
    status = mxgpu_descriptor_encode(&descriptor, out, sizeof out, &n);
    expect(status == MX_OK, "descriptor encode");
    expect_bytes(dir, "descriptor.bin", out, n);
    expect(mxgpu_descriptor_decode(out, n, &descriptor) == MX_OK, "descriptor decode");
    fixture = load(dir, "descriptor-empty.bin", &fixture_len);
    expect(fixture && mxgpu_descriptor_decode(fixture, fixture_len, &descriptor) == MX_ERR_RANGE,
           "empty descriptor refused");
    free(fixture);
    descriptor.byte_len = 0;
    memset(poison, 0xa5, 16);
    status = mxgpu_descriptor_encode(&descriptor, poison, 16, &n);
    expect(status == MX_ERR_RANGE && n == 0 && poison[0] == 0xa5, "empty descriptor emits nothing");

    memset(&transfer, 0, sizeof transfer);
    transfer.resource_id = 8;
    transfer.width = 2;
    transfer.height = 2;
    transfer.depth = 1;
    transfer.row_bytes = 8;
    transfer.data_bytes = 16;
    status = mxgpu_transfer_encode(&transfer, pixel, out, sizeof out, &n);
    expect(status == MX_OK, "transfer encode");
    expect_bytes(dir, "transfer-to-host-payload.bin", out, n);

    status = fixture_textured_program_encode(out, sizeof out, &n);
    expect(status == MXSB_OK, "mxsb encode");
    expect_bytes(dir, "textured-mxsb.bin", out, n);
    mxsb_limits_default(&limits);
    expect(mxsb_verify(out, n, &limits) == MXSB_OK, "mxsb verify");
    fixture = load(dir, "textured-mxsb-bad-magic.bin", &fixture_len);
    expect(fixture && mxsb_verify(fixture, fixture_len, &limits) == MXSB_ERR_MAGIC,
           "bad mxsb refused");
    free(fixture);
    status = mxgpu_shader_create_encode(4, out, n, poison, sizeof poison, &decoded_len);
    expect(status == MX_OK, "shader create");
    expect_bytes(dir, "shader-create.bin", poison, decoded_len);

    status = mxgpu_pipeline_create_encode(5, MXGPU_PIPELINE_RENDER, MXGPU_FMT_RGBA8_UNORM, 4, 1, 2,
                                          out, sizeof out, &n);
    expect(status == MX_OK, "pipeline encode");
    expect_bytes(dir, "pipeline-create.bin", out, n);
    memset(poison, 0xa5, 32);
    status = mxgpu_pipeline_create_encode(0, MXGPU_PIPELINE_RENDER, MXGPU_FMT_RGBA8_UNORM, 4, 1, 2,
                                          poison, 32, &n);
    expect(status != MX_OK && n == 0 && poison[0] == 0xa5, "zero pipeline emits nothing");

    memset(&buffer_binding, 0, sizeof buffer_binding);
    buffer_binding.slot = 0;
    buffer_binding.access = MXGPU_BIND_ACCESS_READ;
    buffer_binding.kind = MXGPU_BIND_KIND_BUFFER;
    buffer_binding.resource_id = 9;
    buffer_binding.size = 48;
    memset(&texture_binding, 0, sizeof texture_binding);
    texture_binding.slot = 1;
    texture_binding.access = MXGPU_BIND_ACCESS_READ;
    texture_binding.kind = MXGPU_BIND_KIND_TEXTURE_2D;
    texture_binding.resource_id = 8;
    status = mxgpu_binding_encode(&buffer_binding, out, sizeof out, &n);
    expect(status == MX_OK, "buffer binding");
    expect_bytes(dir, "binding-buffer.bin", out, n);
    status = mxgpu_binding_encode(&texture_binding, out + 32, sizeof out - 32, &n);
    expect(status == MX_OK, "texture binding");
    expect_bytes(dir, "binding-texture.bin", out + 32, n);

    memset(&submit, 0, sizeof submit);
    submit.pipeline_id = 5;
    submit.color_target_id = 7;
    submit.binding_count = 2;
    submit.load_action = MXGPU_LOAD_CLEAR;
    submit.store_action = MXGPU_STORE_STORE;
    submit.draw_kind = MXGPU_DRAW_NON_INDEXED;
    submit.primitive = MXGPU_PRIM_TRIANGLE;
    submit.element_count = 3;
    submit.instance_count = 1;
    {
        struct mxgpu_execution_binding both[2];
        both[0] = buffer_binding;
        both[1] = texture_binding;
        status = mxgpu_render_submit_encode(&submit, both, out, sizeof out, &n);
        expect(status == MX_OK, "render encode");
        expect_bytes(dir, "render-submit.bin", out, n);
        expect(mxgpu_render_submit_decode(out, n, &submit, both, 2) == MX_OK, "render decode");
    }

    damage.x = 0;
    damage.y = 0;
    damage.width = 64;
    damage.height = 64;
    memset(&present, 0, sizeof present);
    present.resource_id = 7;
    present.source = damage;
    present.damage_count = 1;
    present.damage = &damage;
    status = mxgpu_present_encode(&present, out, sizeof out, &n);
    expect(status == MX_OK, "present encode");
    expect_bytes(dir, "present.bin", out, n);
    expect(mxgpu_present_decode(out, n, &present, &damage, 1) == MX_OK, "present decode");

    expect(mxgpu_format_bytes_per_pixel(MXGPU_FMT_RGBA8_UNORM) == 4, "rgba8 size");
    expect(MXGPU_FEAT_KNOWN == 0xfffffffffffull, "feature mask");
    expect(MXGPU_PROTOCOL_MAGIC == 0x5047584du, "magic");
    expect(MXSB_VERSION_MINOR == 72, "mxsb minor");
    expect(MX_PCI_VENDOR_ID == 0x4d58u && MXGPU_PCI_DEVICE_ID == 0x4750u, "pci");
}

static void test_native_state(void)
{
    const uint8_t blend_golden[24] = {0x44, 0x33, 0x22, 0x11, 1, 0, 0, 0, 1, 15, 0, 0,
                                      2,    6,    1,    2,    6, 1, 0, 0, 0, 0,  0, 0};
    const uint8_t raster_golden[24] = {3,    0,    0,    0,    1, 1, 2, 1, 1, 0, 0, 0,
                                       0xfe, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0};
    struct mxgpu_blend_state blend = {0}, decoded_blend;
    struct mxgpu_rasterizer_state raster = {0}, decoded_raster;
    struct mxgpu_render_extended draw = {0}, decoded_draw;
    struct mxgpu_execution_binding binding = {0}, decoded_binding;
    uint8_t bytes[512], golden[232] = {0};
    uint32_t n, i;
    uint16_t queue;
    uint64_t features = MXGPU_FEAT_RENDER | MXGPU_FEAT_RASTERIZER_STATE | MXGPU_FEAT_BLEND_STATE |
                        MXGPU_FEAT_VIEWPORT_SCISSOR | MXGPU_FEAT_VERTEX_INPUT_LAYOUT |
                        MXGPU_FEAT_VIEWPORT_Y_FLIP;
    blend.state_id = 0x11223344;
    blend.target_count = 1;
    blend.targets[0] = (struct mxgpu_blend_target){1, 15, 2, 6, 1, 2, 6, 1};
    expect(mxgpu_blend_state_encode(&blend, bytes, sizeof bytes, &n) == MX_OK && n == 24 &&
               !memcmp(bytes, blend_golden, 24),
           "native source-over golden bytes");
    expect(mxgpu_blend_state_decode(bytes, n, &decoded_blend) == MX_OK &&
               decoded_blend.state_id == blend.state_id,
           "native blend decode");
    bytes[10] = 1;
    expect(mxgpu_blend_state_decode(bytes, n, &decoded_blend) == MX_ERR_RESERVED,
           "native blend reserved");
    blend.targets[0].src_color = 0;
    bytes[0] = 0xa5;
    n = 9;
    expect(mxgpu_blend_state_encode(&blend, bytes, sizeof bytes, &n) != MX_OK && n == 0 &&
               bytes[0] == 0xa5,
           "native invalid blend emits nothing");
    raster.state_id = 3;
    raster.fill_mode = 1;
    raster.cull_mode = 1;
    raster.front_face = 2;
    raster.depth_clip_enable = 1;
    raster.scissor_enable = 1;
    raster.depth_bias = -2;
    expect(mxgpu_rasterizer_state_encode(&raster, bytes, sizeof bytes, &n) == MX_OK && n == 24 &&
               !memcmp(bytes, raster_golden, 24),
           "native raster golden bytes");
    expect(mxgpu_rasterizer_state_decode(bytes, n, &decoded_raster) == MX_OK &&
               decoded_raster.depth_bias == -2,
           "native raster signed bias");
    raster.depth_bias_clamp = 0x7fc00000;
    expect(mxgpu_rasterizer_state_encode(&raster, bytes, sizeof bytes, &n) != MX_OK && n == 0,
           "native raster NaN refused");
    draw.pipeline_id = 1;
    draw.rasterizer_state_id = 3;
    draw.blend_state_id = 4;
    draw.vertex_layout_id = 5;
    draw.color_target_count = 1;
    draw.viewport_count = 1;
    draw.scissor_count = 1;
    draw.vertex_buffer_count = 1;
    draw.binding_count = 1;
    draw.draw_kind = MXGPU_DRAW_NON_INDEXED;
    draw.primitive = MXGPU_PRIM_TRIANGLE;
    draw.element_count = 3;
    draw.instance_count = 1;
    draw.color_targets[0] = (struct mxgpu_color_attachment){
        7, 0, MXGPU_FMT_RGBA8_UNORM, MXGPU_LOAD_LOAD, MXGPU_STORE_STORE, {0}};
    draw.viewports[0] =
        (struct mxgpu_viewport){0, 0x42800000, 0x42800000, 0xc2800000, 0, 0x3f800000};
    draw.scissors[0] = (struct mxgpu_scissor){0, 0, 64, 64};
    draw.vertex_buffers[0] = (struct mxgpu_vertex_buffer_binding){9, 16, 0x1122334455667788ull};
    binding.slot = 0;
    binding.access = MXGPU_BIND_ACCESS_READ;
    binding.kind = MXGPU_BIND_KIND_BUFFER;
    binding.resource_id = 9;
    binding.size = 48;
    golden[0] = 1;
    golden[4] = 3;
    golden[12] = 4;
    golden[16] = 5;
    golden[60] = 1;
    golden[61] = 1;
    golden[62] = 1;
    golden[63] = 1;
    golden[64] = 1;
    golden[66] = MXGPU_DRAW_NON_INDEXED;
    golden[67] = MXGPU_PRIM_TRIANGLE;
    golden[76] = 3;
    golden[80] = 1;
    golden[104] = 7;
    golden[110] = 2;
    golden[112] = MXGPU_LOAD_LOAD;
    golden[113] = MXGPU_STORE_STORE;
    golden[142] = 0x80;
    golden[143] = 0x42;
    golden[146] = 0x80;
    golden[147] = 0x42;
    golden[150] = 0x80;
    golden[151] = 0xc2;
    golden[158] = 0x80;
    golden[159] = 0x3f;
    golden[168] = 64;
    golden[172] = 64;
    golden[176] = 9;
    golden[184] = 16;
    for (i = 0; i < 8; i++)
        golden[192 + i] = (uint8_t)(0x1122334455667788ull >> (i * 8));
    golden[202] = MXGPU_BIND_ACCESS_READ;
    golden[204] = MXGPU_BIND_KIND_BUFFER;
    golden[208] = 9;
    golden[224] = 48;
    expect(mxgpu_render_extended_encode(&draw, &binding, bytes, sizeof bytes, &n) == MX_OK &&
               n == sizeof golden && !memcmp(bytes, golden, sizeof golden),
           "extended render golden bytes including vertex stride and offset");
    expect(mxgpu_render_extended_decode(bytes, n, &decoded_draw, &decoded_binding, 1) == MX_OK &&
               decoded_draw.vertex_buffers[0].offset == draw.vertex_buffers[0].offset &&
               decoded_draw.viewports[0].height == 0xc2800000,
           "extended render decode");
    for (i = 0; i < n; i++)
        expect(mxgpu_render_extended_decode(bytes, i, &decoded_draw, &decoded_binding, 1) != MX_OK,
               "extended truncated input");
    expect(mxgpu_render_extended_features(&draw, features) == MX_OK &&
               mxgpu_render_extended_features(&draw, features & ~MXGPU_FEAT_BLEND_STATE) != MX_OK &&
               mxgpu_render_extended_features(&draw, features & ~MXGPU_FEAT_VIEWPORT_Y_FLIP) !=
                   MX_OK,
           "extended state feature gates");
    bytes[180] = 1;
    expect(mxgpu_render_extended_decode(bytes, n, &decoded_draw, &decoded_binding, 1) ==
               MX_ERR_RESERVED,
           "extended vertex reserved");
    draw.viewports[0].width = 0x80000000;
    expect(mxgpu_render_extended_encode(&draw, &binding, bytes, sizeof bytes, &n) != MX_OK &&
               n == 0,
           "extended invalid viewport");
    draw.viewports[0].width = 0x42800000;
    draw.element_start = UINT32_MAX;
    expect(mxgpu_render_extended_encode(&draw, &binding, bytes, sizeof bytes, &n) != MX_OK &&
               n == 0,
           "extended draw overflow");
    expect(mxgpu_opcode_queue(MXGPU_OP_RENDER_SUBMIT_EXTENDED, &queue) == MX_OK &&
               queue == MXGPU_QUEUE_RENDER,
           "extended render queue");
    expect(mxgpu_opcode_queue(MXGPU_OP_BLEND_STATE_CREATE, &queue) == MX_OK &&
               queue == MXGPU_QUEUE_CONTROL,
           "blend state queue");
}

static void test_depth_stencil_state(void)
{
    const uint8_t golden[24] = {0x78, 0x56, 0x34, 0x12, 1, 1, 4, 1, 0xaa, 0x55, 0, 0,
                                1,    4,    3,    8,    6, 7, 8, 2, 0,    0,    0, 0};
    struct mxgpu_depth_stencil_state
        state = {.state_id = 0x12345678,
                 .depth_test_enable = 1,
                 .depth_write_enable = 1,
                 .depth_compare = MXGPU_COMPARE_LESS_EQUAL,
                 .stencil_enable = 1,
                 .stencil_read_mask = 0xaa,
                 .stencil_write_mask = 0x55,
                 .front = {MXGPU_STENCIL_KEEP, MXGPU_STENCIL_INCREMENT_CLAMP, MXGPU_STENCIL_REPLACE,
                           MXGPU_COMPARE_ALWAYS},
                 .back = {MXGPU_STENCIL_INVERT, MXGPU_STENCIL_INCREMENT_WRAP,
                          MXGPU_STENCIL_DECREMENT_WRAP, MXGPU_COMPARE_LESS}},
        decoded;
    uint8_t bytes[25];
    uint32_t n, i;
    uint16_t queue;
    expect(mxgpu_depth_stencil_state_encode(&state, bytes, sizeof bytes, &n) == MX_OK &&
               n == sizeof golden && !memcmp(bytes, golden, n),
           "depth stencil golden payload");
    expect(mxgpu_depth_stencil_state_decode(bytes, n, &decoded) == MX_OK &&
               decoded.state_id == state.state_id &&
               decoded.back.pass == MXGPU_STENCIL_DECREMENT_WRAP,
           "depth stencil decode");
    for (i = 0; i < 24; i++)
        expect(mxgpu_depth_stencil_state_decode(bytes, i, &decoded) == MX_ERR_LENGTH,
               "depth stencil truncated");
    expect(mxgpu_depth_stencil_state_decode(bytes, 25, &decoded) == MX_ERR_LENGTH,
           "depth stencil trailing byte");
    const unsigned reserved[] = {10, 11, 20, 21, 22, 23};
    for (i = 0; i < sizeof reserved / sizeof reserved[0]; i++) {
        memcpy(bytes, golden, 24);
        bytes[reserved[i]] = 1;
        decoded.state_id = 99;
        expect(mxgpu_depth_stencil_state_decode(bytes, 24, &decoded) == MX_ERR_RESERVED &&
                   decoded.state_id == 99,
               "depth stencil reserved rejection leaves output unchanged");
    }
    const unsigned enums[] = {6, 12, 13, 14, 15, 16, 17, 18, 19};
    for (i = 0; i < sizeof enums / sizeof enums[0]; i++) {
        memcpy(bytes, golden, 24);
        bytes[enums[i]] = 0;
        expect(mxgpu_depth_stencil_state_decode(bytes, 24, &decoded) == MX_ERR_STATE,
               "depth stencil enum zero");
        bytes[enums[i]] = 9;
        expect(mxgpu_depth_stencil_state_decode(bytes, 24, &decoded) == MX_ERR_STATE,
               "depth stencil enum unknown");
    }
    const unsigned bools[] = {4, 5, 7};
    for (i = 0; i < sizeof bools / sizeof bools[0]; i++) {
        memcpy(bytes, golden, 24);
        bytes[bools[i]] = 2;
        expect(mxgpu_depth_stencil_state_decode(bytes, 24, &decoded) == MX_ERR_STATE,
               "depth stencil noncanonical boolean");
    }
    memset(bytes, 77, sizeof bytes);
    expect(mxgpu_depth_stencil_state_encode(&state, bytes, 23, &n) == MX_ERR_LENGTH && !n &&
               bytes[0] == 77,
           "depth stencil encoder insufficient capacity is atomic");
    state.state_id = 0;
    expect(mxgpu_depth_stencil_state_encode(&state, bytes, sizeof bytes, &n) == MX_ERR_STATE &&
               !n && bytes[0] == 77,
           "depth stencil zero object rejected");
    memcpy(bytes, golden, 24);
    memset(bytes, 0, 4);
    expect(mxgpu_depth_stencil_state_decode(bytes, 24, &decoded) == MX_ERR_STATE,
           "depth stencil zero decoded object");
    expect(mxgpu_opcode_queue(MXGPU_OP_DEPTH_STENCIL_STATE_CREATE, &queue) == MX_OK &&
               queue == MXGPU_QUEUE_CONTROL &&
               mxgpu_opcode_queue(MXGPU_OP_DEPTH_STENCIL_STATE_DESTROY, &queue) == MX_OK &&
               queue == MXGPU_QUEUE_CONTROL,
           "depth stencil object control queue admission");
    uint8_t command[64];
    struct mxgpu_command_header header = {.opcode = MXGPU_OP_DEPTH_STENCIL_STATE_CREATE,
                                          .queue = MXGPU_QUEUE_CONTROL,
                                          .context_id = 1,
                                          .sequence = 1,
                                          .flags = MXGPU_CMD_SIGNAL_FENCE,
                                          .fence_value = 1};
    expect(mxgpu_command_encode(&header, golden, sizeof golden, sizeof command, command,
                                sizeof command, &n) == MX_OK &&
               n == 56,
           "depth stencil canonical command accepted");
    header.queue = MXGPU_QUEUE_RENDER;
    expect(mxgpu_command_encode(&header, golden, sizeof golden, sizeof command, command,
                                sizeof command, &n) != MX_OK &&
               !n,
           "depth stencil wrong queue refused before transport");
    header.opcode = MXGPU_OP_DEPTH_STENCIL_STATE_DESTROY;
    header.queue = MXGPU_QUEUE_CONTROL;
    expect(mxgpu_resource_id_encode(4, bytes, sizeof bytes, &n) == MX_OK && n == 8 &&
               mxgpu_command_encode(&header, bytes, n, sizeof command, command, sizeof command,
                                    &n) == MX_OK &&
               n == 40,
           "depth stencil destroy uses canonical object identifier");
    expect(mxgpu_format_bytes_per_pixel(MXGPU_FMT_DEPTH32_FLOAT) == 4 &&
               mxgpu_format_bytes_per_pixel(MXGPU_FMT_DEPTH32_FLOAT_STENCIL8) == 8 &&
               mxgpu_format_bytes_per_pixel(MXGPU_FMT_DEPTH24_UNORM_STENCIL8) == 4,
           "named depth stencil formats use canonical transfer storage");
    expect(mxgpu_depth_stencil_state_features(MXGPU_FEAT_DEPTH_STENCIL_TARGET) == MX_OK &&
               mxgpu_depth_stencil_state_features(MXGPU_FEAT_RENDER) == MX_ERR_STATE,
           "depth stencil feature admission");
}

static void test_format_capabilities(void)
{
    struct mxgpu_format_capabilities c = {2, 2, 2, 1u << 18, 0, 2 | (1u << 18), 2 | (1u << 18), 2},
                                     decoded;
    const uint8_t golden[32] = {2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 0, 0, 4, 0,
                                0, 0, 0, 0, 2, 0, 4, 0, 2, 0, 4, 0, 2, 0, 0, 0};
    uint8_t bytes[64];
    uint32_t n;
    uint16_t queue;
    expect(mxgpu_format_capabilities_encode(&c, 2 | (1u << 18), bytes, sizeof bytes, &n) == MX_OK &&
               n == 32 && !memcmp(bytes, golden, 32),
           "format capabilities golden eight masks");
    expect(mxgpu_format_capabilities_decode(bytes, n, 2 | (1u << 18), &decoded) == MX_OK &&
               decoded.depth_stencil_target == (1u << 18),
           "format capabilities decode");
    for (unsigned i = 0; i < 32; i++)
        expect(mxgpu_format_capabilities_decode(bytes, i, UINT32_MAX, &decoded) == MX_ERR_LENGTH,
               "format capabilities truncation");
    expect(mxgpu_format_capabilities_decode(bytes, 33, UINT32_MAX, &decoded) == MX_ERR_LENGTH,
           "format capabilities trailing byte");
    decoded.sampled = 123;
    expect(mxgpu_format_capabilities_decode(bytes, 32, 2, &decoded) == MX_ERR_STATE &&
               decoded.sampled == 123,
           "format capabilities unsupported format leaves output unchanged");
    c.color_target |= 1u << 18;
    expect(mxgpu_format_capabilities_encode(&c, UINT32_MAX, bytes, sizeof bytes, &n) ==
                   MX_ERR_STATE &&
               !n,
           "format capabilities depth cannot be color");
    c = (struct mxgpu_format_capabilities){0};
    expect(mxgpu_format_capabilities_encode(&c, 0, bytes, sizeof bytes, &n) == MX_OK && n == 32,
           "format capabilities zero usage valid");
    expect(mxgpu_format_capabilities_features(MXGPU_FEAT_EXTENDED_PIXEL_FORMATS) == MX_OK &&
               mxgpu_format_capabilities_features(MXGPU_FEAT_RENDER) == MX_ERR_STATE,
           "format query feature admission");
    expect(mxgpu_opcode_queue(MXGPU_OP_QUERY_FORMAT_CAPABILITIES, &queue) == MX_OK &&
               queue == MXGPU_QUEUE_CONTROL,
           "format query control admission");
    struct mxgpu_command_header h = {
        .opcode = MXGPU_OP_QUERY_FORMAT_CAPABILITIES, .queue = MXGPU_QUEUE_CONTROL, .sequence = 1};
    expect(mxgpu_command_encode(&h, NULL, 0, sizeof bytes, bytes, sizeof bytes, &n) == MX_OK &&
               n == 32,
           "format query empty global command");
    h.context_id = 1;
    expect(mxgpu_command_encode(&h, NULL, 0, sizeof bytes, bytes, sizeof bytes, &n) ==
                   MX_ERR_CONTEXT &&
               !n,
           "format query cannot use owned context");
    h.context_id = 0;
    expect(mxgpu_command_encode(&h, golden, 1, sizeof bytes, bytes, sizeof bytes, &n) ==
                   MX_ERR_LENGTH &&
               !n,
           "format query requires empty payload");
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: test_protocol FIXTURE_DIR\n");
        return 2;
    }
    test_roundtrip(argv[1]);
    test_native_state();
    test_depth_stencil_state();
    test_format_capabilities();
    if (failures) {
        fprintf(stderr, "%d failures\n", failures);
        return 1;
    }
    printf("core protocol tests passed\n");
    return 0;
}
