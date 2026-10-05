/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxgpu_wire.h"

#include "mx_le.h"

#include <string.h>

static int emit(uint8_t *out, uint32_t cap, uint32_t *out_len, const uint8_t *src, uint32_t n)
{
    if (out_len)
        *out_len = 0;
    if (!src || n == 0 || (out && cap < n))
        return MX_ERR_LENGTH;
    if (out)
        memcpy(out, src, n);
    if (out_len)
        *out_len = n;
    return MX_OK;
}

static int fail(uint32_t *out_len, int status)
{
    if (out_len)
        *out_len = 0;
    return status;
}

int mxgpu_f32_finite(uint32_t bits)
{
    return ((bits >> 23) & 0xffu) != 0xffu;
}

uint32_t mxgpu_format_bytes_per_pixel(uint16_t format)
{
    switch (format) {
    case 1:
    case 20:
    case 21:
        return 1;
    case 7:
    case 15:
    case 22:
    case 23:
        return 2;
    case 2:
    case 3:
    case 5:
    case 6:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 16:
    case 17:
    case 19:
    case 24:
    case 25:
    case 26:
    case 27:
        return 4;
    case 4:
    case 14:
    case 18:
    case 28:
    case 29:
        return 8;
    case 8:
    case 30:
    case 31:
        return 16;
    default:
        return 0;
    }
}

static int known_format(uint16_t format)
{
    return format >= 1 && format <= 31;
}

int mxgpu_opcode_queue(uint16_t opcode, uint16_t *queue)
{
    uint16_t q;
    if (!queue)
        return MX_ERR_STATE;
    switch (opcode) {
    case MXGPU_OP_RENDER_SUBMIT:
    case MXGPU_OP_RENDER_SUBMIT_EXTENDED:
    case MXGPU_OP_PRESENT:
    case MXGPU_OP_COLOR_CLEAR:
        q = MXGPU_QUEUE_RENDER;
        break;
    case MXGPU_OP_TRANSFER_TO_HOST:
    case MXGPU_OP_TRANSFER_FROM_HOST:
        q = MXGPU_QUEUE_TRANSFER;
        break;
    case MXGPU_OP_CURSOR_UPDATE:
        q = MXGPU_QUEUE_CURSOR;
        break;
    case MXGPU_OP_QUERY_FORMAT_CAPABILITIES:
    case MXGPU_OP_QUERY_ADAPTER:
    case MXGPU_OP_QUERY_PLACEMENT:
    case MXGPU_OP_CONTEXT_CREATE:
    case MXGPU_OP_CONTEXT_DESTROY:
    case MXGPU_OP_RESOURCE_CREATE:
    case MXGPU_OP_RESOURCE_DESTROY:
    case MXGPU_OP_RESOURCE_BIND:
    case MXGPU_OP_RESOURCE_UNBIND:
    case MXGPU_OP_SHADER_CREATE:
    case MXGPU_OP_SHADER_DESTROY:
    case MXGPU_OP_PIPELINE_CREATE:
    case MXGPU_OP_PIPELINE_DESTROY:
    case MXGPU_OP_DEPTH_STENCIL_STATE_CREATE:
    case MXGPU_OP_DEPTH_STENCIL_STATE_DESTROY:
    case MXGPU_OP_BLEND_STATE_CREATE:
    case MXGPU_OP_BLEND_STATE_DESTROY:
    case MXGPU_OP_RASTERIZER_STATE_CREATE:
    case MXGPU_OP_RASTERIZER_STATE_DESTROY:
    case MXGPU_OP_SAMPLER_CREATE:
    case MXGPU_OP_SAMPLER_DESTROY:
    case MXGPU_OP_TIMELINE_WAIT:
        q = MXGPU_QUEUE_CONTROL;
        break;
    default:
        return MX_ERR_OPCODE;
    }
    *queue = q;
    return MX_OK;
}

static int cursor_update_valid(const struct mxgpu_cursor_update *in)
{
    if (!in)
        return MX_ERR_STATE;
    if ((in->flags & ~MXGPU_CURSOR_FLAGS_KNOWN) || in->flags == MXGPU_CURSOR_FLAGS_KNOWN)
        return MX_ERR_FLAGS;
    if (in->flags == MXGPU_CURSOR_HIDE) {
        if (in->resource_id || in->x || in->y || in->hot_x || in->hot_y)
            return MX_ERR_SHAPE;
    } else if (in->flags == MXGPU_CURSOR_MOVE_ONLY) {
        if (in->resource_id || in->hot_x || in->hot_y)
            return MX_ERR_SHAPE;
    } else if (!in->resource_id) {
        return MX_ERR_RESOURCE;
    }
    return MX_OK;
}

int mxgpu_cursor_update_encode(const struct mxgpu_cursor_update *in, uint8_t *out, uint32_t cap,
                               uint32_t *out_len)
{
    uint8_t bytes[MXGPU_CURSOR_UPDATE_SIZE];
    int status = cursor_update_valid(in);

    if (status != MX_OK)
        return fail(out_len, status);
    memset(bytes, 0, sizeof bytes);
    mx_w32(bytes, 0, in->resource_id);
    mx_w16(bytes, 4, in->scanout_id);
    mx_w16(bytes, 6, in->flags);
    mx_w32(bytes, 8, (uint32_t)in->x);
    mx_w32(bytes, 12, (uint32_t)in->y);
    mx_w32(bytes, 16, in->hot_x);
    mx_w32(bytes, 20, in->hot_y);
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

static int32_t signed32_from_bits(uint32_t bits)
{
    if (bits <= 0x7fffffffu)
        return (int32_t)bits;
    return -1 - (int32_t)~bits;
}

int mxgpu_cursor_update_decode(const uint8_t *in, uint32_t len, struct mxgpu_cursor_update *out)
{
    struct mxgpu_cursor_update update;
    int status;

    if (!in || len != MXGPU_CURSOR_UPDATE_SIZE)
        return MX_ERR_LENGTH;
    if (!out)
        return MX_ERR_STATE;
    if (mx_r32(in, 24) || mx_r32(in, 28))
        return MX_ERR_RESERVED;
    update.resource_id = mx_r32(in, 0);
    update.scanout_id = mx_r16(in, 4);
    update.flags = mx_r16(in, 6);
    update.x = signed32_from_bits(mx_r32(in, 8));
    update.y = signed32_from_bits(mx_r32(in, 12));
    update.hot_x = mx_r32(in, 16);
    update.hot_y = mx_r32(in, 20);
    status = cursor_update_valid(&update);
    if (status != MX_OK)
        return status;
    *out = update;
    return MX_OK;
}

static int global_opcode(uint16_t opcode)
{
    return opcode == MXGPU_OP_QUERY_ADAPTER || opcode == MXGPU_OP_QUERY_FORMAT_CAPABILITIES ||
           opcode == MXGPU_OP_QUERY_PLACEMENT;
}

int mxgpu_negotiation_request_encode(const struct mxgpu_negotiation_request *in, uint8_t *out,
                                     uint32_t cap, uint32_t *out_len)
{
    uint8_t bytes[MXGPU_NEGOTIATION_REQUEST_SIZE];
    if (!in)
        return fail(out_len, MX_ERR_STATE);
    if (in->minimum_major != MXGPU_PROTOCOL_VERSION_MAJOR ||
        in->maximum_major != MXGPU_PROTOCOL_VERSION_MAJOR)
        return fail(out_len, MX_ERR_VERSION);
    if (in->minimum_minor > in->maximum_minor || in->maximum_minor > MXGPU_PROTOCOL_VERSION_MINOR)
        return fail(out_len, MX_ERR_VERSION);
    if ((in->requested_features & ~MXGPU_FEAT_KNOWN) || (in->required_features & ~MXGPU_FEAT_KNOWN))
        return fail(out_len, MX_ERR_FEATURE);
    if ((in->required_features & ~in->requested_features) != 0)
        return fail(out_len, MX_ERR_FEATURE);
    memset(bytes, 0, sizeof bytes);
    mx_w32(bytes, 0, MXGPU_PROTOCOL_MAGIC);
    mx_w16(bytes, 4, in->minimum_major);
    mx_w16(bytes, 6, in->minimum_minor);
    mx_w16(bytes, 8, in->maximum_major);
    mx_w16(bytes, 10, in->maximum_minor);
    mx_w64(bytes, 16, in->requested_features);
    mx_w64(bytes, 24, in->required_features);
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

int mxgpu_negotiation_request_decode(const uint8_t *in, uint32_t len,
                                     struct mxgpu_negotiation_request *out)
{
    struct mxgpu_negotiation_request req;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len != MXGPU_NEGOTIATION_REQUEST_SIZE)
        return MX_ERR_LENGTH;
    if (mx_r32(in, 0) != MXGPU_PROTOCOL_MAGIC)
        return MX_ERR_MAGIC;
    if (mx_r32(in, 12) != 0)
        return MX_ERR_RESERVED;
    req.minimum_major = mx_r16(in, 4);
    req.minimum_minor = mx_r16(in, 6);
    req.maximum_major = mx_r16(in, 8);
    req.maximum_minor = mx_r16(in, 10);
    req.requested_features = mx_r64(in, 16);
    req.required_features = mx_r64(in, 24);
    if ((req.requested_features & ~MXGPU_FEAT_KNOWN) || (req.required_features & ~MXGPU_FEAT_KNOWN))
        return MX_ERR_FEATURE;
    *out = req;
    return MX_OK;
}

int mxgpu_negotiation_response_encode(const struct mxgpu_negotiated *in, uint8_t *out, uint32_t cap,
                                      uint32_t *out_len)
{
    uint8_t bytes[MXGPU_NEGOTIATION_RESPONSE_COMPUTE_LIMIT_SIZE];
    uint32_t size;
    int extended;
    int transfer_limited;
    int compute_limited;
    if (!in)
        return fail(out_len, MX_ERR_STATE);
    if (in->major != MXGPU_PROTOCOL_VERSION_MAJOR || in->minor > MXGPU_PROTOCOL_VERSION_MINOR)
        return fail(out_len, MX_ERR_VERSION);
    if ((in->features & ~MXGPU_FEAT_KNOWN) || (in->host_event_features & ~MXGPU_HOST_EVENT_KNOWN))
        return fail(out_len, MX_ERR_FEATURE);
    extended = in->minor >= MXGPU_PROTOCOL_DYNAMIC_MODE_MINOR;
    transfer_limited = in->minor >= MXGPU_PROTOCOL_TRANSFER_LIMIT_MINOR;
    compute_limited = in->minor >= MXGPU_PROTOCOL_COMPUTE_LIMIT_MINOR;
    if (compute_limited)
        size = MXGPU_NEGOTIATION_RESPONSE_COMPUTE_LIMIT_SIZE;
    else if (transfer_limited)
        size = MXGPU_NEGOTIATION_RESPONSE_TRANSFER_LIMIT_SIZE;
    else if (extended)
        size = MXGPU_NEGOTIATION_RESPONSE_EXTENDED_SIZE;
    else
        size = MXGPU_NEGOTIATION_RESPONSE_SIZE;
    if (in->status != 0 && size != MXGPU_NEGOTIATION_RESPONSE_SIZE)
        return fail(out_len, MX_ERR_STATE);
    memset(bytes, 0, sizeof bytes);
    mx_w32(bytes, 0, MXGPU_PROTOCOL_MAGIC);
    mx_w16(bytes, 4, in->major);
    mx_w16(bytes, 6, in->minor);
    mx_w32(bytes, 8, in->status);
    mx_w32(bytes, 12, in->status == 0 ? size : MXGPU_NEGOTIATION_RESPONSE_SIZE);
    if (in->status == 0) {
        mx_w64(bytes, 16, in->features);
        mx_w16(bytes, 24, in->limits.max_queues);
        mx_w16(bytes, 26, in->limits.max_scanouts);
        mx_w32(bytes, 28, in->limits.max_contexts);
        mx_w32(bytes, 32, in->limits.max_resources);
        mx_w32(bytes, 36, in->limits.max_descriptors_per_queue);
        mx_w32(bytes, 40, in->limits.max_command_bytes);
        mx_w32(bytes, 44, in->limits.max_inline_bytes);
        mx_w64(bytes, 48, in->limits.max_resource_bytes);
        mx_w64(bytes, 56, in->limits.max_resident_bytes);
        if (extended)
            mx_w64(bytes, 64, in->host_event_features);
        if (transfer_limited) {
            mx_w32(bytes, 72, in->limits.max_transfer_to_host_bytes);
            if (in->features & MXGPU_FEAT_LARGE_READBACK)
                mx_w32(bytes, 76, in->limits.max_transfer_from_host_bytes);
        }
        if (compute_limited) {
            mx_w32(bytes, 80, in->limits.max_compute_work_group_size[0]);
            mx_w32(bytes, 84, in->limits.max_compute_work_group_size[1]);
            mx_w32(bytes, 88, in->limits.max_compute_work_group_size[2]);
            mx_w32(bytes, 92, in->limits.max_compute_work_group_invocations);
        }
    }
    return emit(out, cap, out_len, bytes,
                in->status == 0 ? size : MXGPU_NEGOTIATION_RESPONSE_COMPUTE_LIMIT_SIZE);
}

int mxgpu_negotiation_response_decode(const uint8_t *in, uint32_t len, struct mxgpu_negotiated *out)
{
    struct mxgpu_negotiated neg;
    uint32_t size;
    uint32_t status;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len < MXGPU_NEGOTIATION_RESPONSE_SIZE)
        return MX_ERR_LENGTH;
    if (mx_r32(in, 0) != MXGPU_PROTOCOL_MAGIC)
        return MX_ERR_MAGIC;
    status = mx_r32(in, 8);
    if (status > 6)
        return MX_ERR_STATE;
    size = mx_r32(in, 12);
    if (size != MXGPU_NEGOTIATION_RESPONSE_SIZE &&
        size != MXGPU_NEGOTIATION_RESPONSE_EXTENDED_SIZE &&
        size != MXGPU_NEGOTIATION_RESPONSE_TRANSFER_LIMIT_SIZE &&
        size != MXGPU_NEGOTIATION_RESPONSE_COMPUTE_LIMIT_SIZE)
        return MX_ERR_LENGTH;
    if (len < size)
        return MX_ERR_LENGTH;
    memset(&neg, 0, sizeof neg);
    neg.major = mx_r16(in, 4);
    neg.minor = mx_r16(in, 6);
    neg.status = status;
    neg.response_size = size;
    if (status != 0) {
        *out = neg;
        return MX_OK;
    }
    neg.features = mx_r64(in, 16);
    if (neg.features & ~MXGPU_FEAT_KNOWN)
        return MX_ERR_FEATURE;
    neg.limits.max_queues = mx_r16(in, 24);
    neg.limits.max_scanouts = mx_r16(in, 26);
    neg.limits.max_contexts = mx_r32(in, 28);
    neg.limits.max_resources = mx_r32(in, 32);
    neg.limits.max_descriptors_per_queue = mx_r32(in, 36);
    neg.limits.max_command_bytes = mx_r32(in, 40);
    neg.limits.max_inline_bytes = mx_r32(in, 44);
    neg.limits.max_resource_bytes = mx_r64(in, 48);
    neg.limits.max_resident_bytes = mx_r64(in, 56);
    if (size >= MXGPU_NEGOTIATION_RESPONSE_EXTENDED_SIZE) {
        neg.host_event_features = mx_r64(in, 64);
        if (neg.host_event_features & ~MXGPU_HOST_EVENT_KNOWN)
            return MX_ERR_FEATURE;
    }
    if (size >= MXGPU_NEGOTIATION_RESPONSE_TRANSFER_LIMIT_SIZE) {
        neg.limits.max_transfer_to_host_bytes = mx_r32(in, 72);
        if (neg.features & MXGPU_FEAT_LARGE_READBACK)
            neg.limits.max_transfer_from_host_bytes = mx_r32(in, 76);
        else
            neg.limits.max_transfer_from_host_bytes = neg.limits.max_inline_bytes;
    } else {
        neg.limits.max_transfer_to_host_bytes = neg.limits.max_inline_bytes;
        neg.limits.max_transfer_from_host_bytes = neg.limits.max_inline_bytes;
    }
    if (size == MXGPU_NEGOTIATION_RESPONSE_COMPUTE_LIMIT_SIZE) {
        neg.limits.max_compute_work_group_size[0] = mx_r32(in, 80);
        neg.limits.max_compute_work_group_size[1] = mx_r32(in, 84);
        neg.limits.max_compute_work_group_size[2] = mx_r32(in, 88);
        neg.limits.max_compute_work_group_invocations = mx_r32(in, 92);
    }
    *out = neg;
    return MX_OK;
}

static int resource_shape_ok(const struct mxgpu_resource_create *in, uint64_t max_resource_bytes)
{
    uint32_t edge;
    uint32_t max_mips;
    uint64_t minimum;
    uint32_t bpp;
    if (in->resource_id == 0)
        return MX_ERR_RESOURCE;
    if (in->usage == 0 || (in->usage & ~MXGPU_USAGE_KNOWN))
        return MX_ERR_USAGE;
    if (in->width == 0 || in->height == 0 || in->depth == 0 || in->array_layers == 0 ||
        in->mip_levels == 0 || in->sample_count == 0)
        return MX_ERR_SHAPE;
    if ((in->sample_count & (in->sample_count - 1)) != 0 || in->sample_count > 16 ||
        (in->sample_count > 1 && in->mip_levels != 1))
        return MX_ERR_SHAPE;
    if (in->byte_size == 0 || in->byte_size > max_resource_bytes)
        return MX_ERR_LIMIT;
    if (in->kind == MXGPU_KIND_BUFFER) {
        if (in->format != 0 || in->byte_size != (uint64_t)in->width || in->height != 1 ||
            in->depth != 1 || in->array_layers != 1 || in->mip_levels != 1 || in->sample_count != 1)
            return MX_ERR_SHAPE;
        return MX_OK;
    }
    if (!known_format(in->format))
        return MX_ERR_FORMAT;
    if (in->kind == MXGPU_KIND_TEXTURE_1D && (in->height != 1 || in->depth != 1))
        return MX_ERR_SHAPE;
    if (in->kind == MXGPU_KIND_TEXTURE_2D && in->depth != 1)
        return MX_ERR_SHAPE;
    if (in->kind == MXGPU_KIND_TEXTURE_3D && in->array_layers != 1)
        return MX_ERR_SHAPE;
    if (in->kind != MXGPU_KIND_TEXTURE_1D && in->kind != MXGPU_KIND_TEXTURE_2D &&
        in->kind != MXGPU_KIND_TEXTURE_3D)
        return MX_ERR_SHAPE;
    edge = in->width;
    if (in->kind != MXGPU_KIND_TEXTURE_1D && in->height > edge)
        edge = in->height;
    if (in->kind == MXGPU_KIND_TEXTURE_3D && in->depth > edge)
        edge = in->depth;
    max_mips = 32u - (uint32_t)__builtin_clz(edge);
    if (in->mip_levels > max_mips)
        return MX_ERR_SHAPE;
    bpp = mxgpu_format_bytes_per_pixel(in->format);
    minimum = 0;
    {
        uint32_t level;
        for (level = 0; level < in->mip_levels; level++) {
            uint64_t w = (in->width >> level) ? (in->width >> level) : 1;
            uint64_t h = 1;
            uint64_t d = 1;
            uint64_t level_bytes;
            if (in->kind != MXGPU_KIND_TEXTURE_1D)
                h = (in->height >> level) ? (in->height >> level) : 1;
            if (in->kind == MXGPU_KIND_TEXTURE_3D)
                d = (in->depth >> level) ? (in->depth >> level) : 1;
            level_bytes = w * h * d * in->array_layers * in->sample_count * bpp;
            if (w != 0 && level_bytes / w != h * d * in->array_layers * in->sample_count * bpp)
                return MX_ERR_SHAPE;
            if (minimum + level_bytes < minimum)
                return MX_ERR_SHAPE;
            minimum += level_bytes;
        }
    }
    if (in->byte_size < minimum)
        return MX_ERR_RANGE;
    if ((in->usage & (MXGPU_USAGE_SCANOUT | MXGPU_USAGE_CURSOR)) &&
        (in->kind != MXGPU_KIND_TEXTURE_2D || in->array_layers != 1 || in->mip_levels != 1 ||
         in->sample_count != 1))
        return MX_ERR_USAGE;
    return MX_OK;
}

int mxgpu_resource_create_encode(const struct mxgpu_resource_create *in,
                                 uint64_t max_resource_bytes, uint8_t *out, uint32_t cap,
                                 uint32_t *out_len)
{
    uint8_t bytes[MXGPU_RESOURCE_CREATE_SIZE];
    int status;
    if (!in)
        return fail(out_len, MX_ERR_STATE);
    status = resource_shape_ok(in, max_resource_bytes);
    if (status != MX_OK)
        return fail(out_len, status);
    memset(bytes, 0, sizeof bytes);
    mx_w32(bytes, 0, in->resource_id);
    mx_w16(bytes, 4, in->kind);
    mx_w16(bytes, 6, in->format);
    mx_w64(bytes, 8, in->usage);
    mx_w32(bytes, 16, in->width);
    mx_w32(bytes, 20, in->height);
    mx_w32(bytes, 24, in->depth);
    mx_w32(bytes, 28, in->array_layers);
    mx_w16(bytes, 32, in->mip_levels);
    mx_w16(bytes, 34, in->sample_count);
    mx_w64(bytes, 40, in->byte_size);
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

int mxgpu_resource_create_decode(const uint8_t *in, uint32_t len, uint64_t max_resource_bytes,
                                 struct mxgpu_resource_create *out)
{
    struct mxgpu_resource_create rec;
    int status;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len != MXGPU_RESOURCE_CREATE_SIZE)
        return MX_ERR_LENGTH;
    if (mx_r32(in, 36) != 0)
        return MX_ERR_RESERVED;
    memset(&rec, 0, sizeof rec);
    rec.resource_id = mx_r32(in, 0);
    rec.kind = mx_r16(in, 4);
    rec.format = mx_r16(in, 6);
    rec.usage = mx_r64(in, 8);
    rec.width = mx_r32(in, 16);
    rec.height = mx_r32(in, 20);
    rec.depth = mx_r32(in, 24);
    rec.array_layers = mx_r32(in, 28);
    rec.mip_levels = mx_r16(in, 32);
    rec.sample_count = mx_r16(in, 34);
    rec.byte_size = mx_r64(in, 40);
    status = resource_shape_ok(&rec, max_resource_bytes);
    if (status != MX_OK)
        return status;
    *out = rec;
    return MX_OK;
}

static int bind_ok(const struct mxgpu_resource_bind *in)
{
    uint64_t mask = MXGPU_PLACEMENT_PAGE_SIZE - 1ull;
    uint64_t resource_end;
    uint64_t binding_end;
    if (!in || in->resource_id == 0 || in->byte_size == 0)
        return MX_ERR_BINDING;
    if ((in->gpu_va & mask) || (in->resource_offset & mask) || (in->byte_size & mask))
        return MX_ERR_BINDING;
    if (in->resource_offset + in->byte_size < in->resource_offset)
        return MX_ERR_BINDING;
    if (in->gpu_va + in->byte_size < in->gpu_va)
        return MX_ERR_BINDING;
    resource_end = in->resource_offset + in->byte_size;
    binding_end = in->gpu_va + in->byte_size;
    if (resource_end <= in->resource_offset)
        return MX_ERR_BINDING;
    if (in->gpu_va < MXGPU_PLACEMENT_APERTURE_BASE)
        return MX_ERR_BINDING;
    if (binding_end > MXGPU_PLACEMENT_APERTURE_BASE + MXGPU_PLACEMENT_APERTURE_SIZE)
        return MX_ERR_BINDING;
    return MX_OK;
}

int mxgpu_resource_bind_encode(const struct mxgpu_resource_bind *in, uint8_t *out, uint32_t cap,
                               uint32_t *out_len)
{
    uint8_t bytes[MXGPU_RESOURCE_BIND_SIZE];
    int status = bind_ok(in);
    if (status != MX_OK)
        return fail(out_len, status);
    memset(bytes, 0, sizeof bytes);
    mx_w32(bytes, 0, in->resource_id);
    mx_w64(bytes, 8, in->gpu_va);
    mx_w64(bytes, 16, in->resource_offset);
    mx_w64(bytes, 24, in->byte_size);
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

int mxgpu_resource_bind_decode(const uint8_t *in, uint32_t len, struct mxgpu_resource_bind *out)
{
    struct mxgpu_resource_bind rec;
    int status;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len != MXGPU_RESOURCE_BIND_SIZE)
        return MX_ERR_LENGTH;
    if (mx_r16(in, 4) != 0 || mx_r16(in, 6) != 0)
        return MX_ERR_RESERVED;
    rec.resource_id = mx_r32(in, 0);
    rec.gpu_va = mx_r64(in, 8);
    rec.resource_offset = mx_r64(in, 16);
    rec.byte_size = mx_r64(in, 24);
    status = bind_ok(&rec);
    if (status != MX_OK)
        return status;
    *out = rec;
    return MX_OK;
}

int mxgpu_resource_id_encode(uint32_t resource_id, uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint8_t bytes[MXGPU_RESOURCE_ID_SIZE];
    if (resource_id == 0)
        return fail(out_len, MX_ERR_RESOURCE);
    memset(bytes, 0, sizeof bytes);
    mx_w32(bytes, 0, resource_id);
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

int mxgpu_resource_id_decode(const uint8_t *in, uint32_t len, uint32_t *resource_id)
{
    uint32_t id;
    if (!in || !resource_id)
        return MX_ERR_STATE;
    if (len != MXGPU_RESOURCE_ID_SIZE)
        return MX_ERR_LENGTH;
    if (mx_r32(in, 4) != 0)
        return MX_ERR_RESERVED;
    id = mx_r32(in, 0);
    if (id == 0)
        return MX_ERR_RESOURCE;
    *resource_id = id;
    return MX_OK;
}

static int header_ok(const struct mxgpu_command_header *header, uint32_t byte_len,
                     uint32_t max_command_bytes)
{
    uint16_t expect;
    int status;
    if (!header)
        return MX_ERR_STATE;
    if (byte_len < MXGPU_COMMAND_HEADER_SIZE || byte_len > max_command_bytes)
        return MX_ERR_LENGTH;
    if ((header->opcode == MXGPU_OP_QUERY_ADAPTER ||
         header->opcode == MXGPU_OP_QUERY_FORMAT_CAPABILITIES) &&
        byte_len != MXGPU_COMMAND_HEADER_SIZE)
        return MX_ERR_LENGTH;
    if (header->flags & ~MXGPU_CMD_FLAGS_KNOWN)
        return MX_ERR_FLAGS;
    status = mxgpu_opcode_queue(header->opcode, &expect);
    if (status != MX_OK)
        return status;
    if (header->queue != expect)
        return MX_ERR_QUEUE;
    if ((header->context_id == 0) != global_opcode(header->opcode))
        return MX_ERR_CONTEXT;
    if (header->sequence == 0)
        return MX_ERR_SEQUENCE;
    if (((header->flags & MXGPU_CMD_SIGNAL_FENCE) != 0) != (header->fence_value != 0))
        return MX_ERR_FENCE;
    return MX_OK;
}

int mxgpu_command_encode(const struct mxgpu_command_header *header, const uint8_t *payload,
                         uint32_t payload_len, uint32_t max_command_bytes, uint8_t *out,
                         uint32_t cap, uint32_t *out_len)
{
    uint32_t byte_len;
    int status;
    if (payload_len && !payload)
        return fail(out_len, MX_ERR_STATE);
    if (payload_len > UINT32_MAX - MXGPU_COMMAND_HEADER_SIZE)
        return fail(out_len, MX_ERR_LENGTH);
    byte_len = MXGPU_COMMAND_HEADER_SIZE + payload_len;
    status = header_ok(header, byte_len, max_command_bytes);
    if (status != MX_OK)
        return fail(out_len, status);
    if (out_len)
        *out_len = 0;
    if (!out || cap < byte_len)
        return MX_ERR_LENGTH;
    memset(out, 0, byte_len);
    mx_w16(out, 0, header->opcode);
    mx_w16(out, 2, header->flags);
    mx_w32(out, 4, byte_len);
    mx_w32(out, 8, header->context_id);
    mx_w16(out, 12, header->queue);
    mx_w64(out, 16, header->sequence);
    mx_w64(out, 24, header->fence_value);
    if (payload_len)
        memcpy(out + MXGPU_COMMAND_HEADER_SIZE, payload, payload_len);
    if (out_len)
        *out_len = byte_len;
    return MX_OK;
}

int mxgpu_command_decode(const uint8_t *in, uint32_t len, uint32_t max_command_bytes,
                         struct mxgpu_command_header *header, const uint8_t **payload,
                         uint32_t *payload_len)
{
    struct mxgpu_command_header rec;
    uint32_t byte_len;
    int status;
    if (!in || !header)
        return MX_ERR_STATE;
    if (len < MXGPU_COMMAND_HEADER_SIZE)
        return MX_ERR_LENGTH;
    byte_len = mx_r32(in, 4);
    if (byte_len < MXGPU_COMMAND_HEADER_SIZE || byte_len > max_command_bytes || byte_len != len)
        return MX_ERR_LENGTH;
    if (mx_r16(in, 14) != 0)
        return MX_ERR_RESERVED;
    rec.opcode = mx_r16(in, 0);
    rec.flags = mx_r16(in, 2);
    rec.context_id = mx_r32(in, 8);
    rec.queue = mx_r16(in, 12);
    rec.sequence = mx_r64(in, 16);
    rec.fence_value = mx_r64(in, 24);
    status = header_ok(&rec, byte_len, max_command_bytes);
    if (status != MX_OK)
        return status;
    *header = rec;
    if (payload)
        *payload = in + MXGPU_COMMAND_HEADER_SIZE;
    if (payload_len)
        *payload_len = byte_len - MXGPU_COMMAND_HEADER_SIZE;
    return MX_OK;
}

int mxgpu_completion_encode(const struct mxgpu_completion *in, uint8_t *out, uint32_t cap,
                            uint32_t *out_len)
{
    uint8_t bytes[MXGPU_COMPLETION_SIZE];
    if (!in)
        return fail(out_len, MX_ERR_STATE);
    if (in->status > MXGPU_COMPLETION_INVALID_PARAMETER && in->status > 8)
        return fail(out_len, MX_ERR_STATE);
    memset(bytes, 0, sizeof bytes);
    mx_w64(bytes, 0, in->sequence);
    mx_w32(bytes, 8, in->status);
    mx_w32(bytes, 12, in->response_bytes);
    mx_w64(bytes, 16, in->completed_fence);
    mx_w32(bytes, 24, in->device_generation);
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

int mxgpu_completion_decode(const uint8_t *in, uint32_t len, struct mxgpu_completion *out)
{
    if (!in || !out)
        return MX_ERR_STATE;
    if (len != MXGPU_COMPLETION_SIZE)
        return MX_ERR_LENGTH;
    if (mx_r32(in, 28) != 0)
        return MX_ERR_RESERVED;
    out->sequence = mx_r64(in, 0);
    out->status = mx_r32(in, 8);
    out->response_bytes = mx_r32(in, 12);
    out->completed_fence = mx_r64(in, 16);
    out->device_generation = mx_r32(in, 24);
    return MX_OK;
}

int mxgpu_descriptor_encode(const struct mxgpu_descriptor *in, uint8_t *out, uint32_t cap,
                            uint32_t *out_len)
{
    uint8_t bytes[MXGPU_QUEUE_DESCRIPTOR_SIZE];
    if (!in)
        return fail(out_len, MX_ERR_STATE);
    if (in->byte_len == 0)
        return fail(out_len, MX_ERR_RANGE);
    if (in->flags & ~(MXGPU_DESC_NEXT | MXGPU_DESC_DEVICE_WRITE))
        return fail(out_len, MX_ERR_FLAGS);
    if (in->address + in->byte_len < in->address)
        return fail(out_len, MX_ERR_RANGE);
    if ((in->flags & MXGPU_DESC_NEXT) == 0 && in->next != 0)
        return fail(out_len, MX_ERR_STATE);
    memset(bytes, 0, sizeof bytes);
    mx_w64(bytes, 0, in->address);
    mx_w32(bytes, 8, in->byte_len);
    mx_w16(bytes, 12, in->flags);
    mx_w16(bytes, 14, in->next);
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

int mxgpu_descriptor_decode(const uint8_t *in, uint32_t len, struct mxgpu_descriptor *out)
{
    struct mxgpu_descriptor rec;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len != MXGPU_QUEUE_DESCRIPTOR_SIZE)
        return MX_ERR_LENGTH;
    rec.address = mx_r64(in, 0);
    rec.byte_len = mx_r32(in, 8);
    rec.flags = mx_r16(in, 12);
    rec.next = mx_r16(in, 14);
    if (rec.byte_len == 0)
        return MX_ERR_RANGE;
    if (rec.flags & ~(MXGPU_DESC_NEXT | MXGPU_DESC_DEVICE_WRITE))
        return MX_ERR_FLAGS;
    if (rec.address + rec.byte_len < rec.address)
        return MX_ERR_RANGE;
    if ((rec.flags & MXGPU_DESC_NEXT) == 0 && rec.next != 0)
        return MX_ERR_STATE;
    *out = rec;
    return MX_OK;
}

int mxgpu_available_encode(const struct mxgpu_available_entry *in, uint8_t *out, uint32_t cap,
                           uint32_t *out_len)
{
    uint8_t bytes[MXGPU_AVAILABLE_ENTRY_SIZE];
    if (!in)
        return fail(out_len, MX_ERR_STATE);
    memset(bytes, 0, sizeof bytes);
    mx_w16(bytes, 0, in->descriptor_head);
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

int mxgpu_available_decode(const uint8_t *in, uint32_t len, struct mxgpu_available_entry *out)
{
    if (!in || !out)
        return MX_ERR_STATE;
    if (len != MXGPU_AVAILABLE_ENTRY_SIZE)
        return MX_ERR_LENGTH;
    if (mx_r16(in, 2) != 0)
        return MX_ERR_RESERVED;
    out->descriptor_head = mx_r16(in, 0);
    return MX_OK;
}

int mxgpu_used_encode(const struct mxgpu_used_entry *in, uint8_t *out, uint32_t cap,
                      uint32_t *out_len)
{
    uint8_t bytes[MXGPU_USED_ENTRY_SIZE];
    if (!in)
        return fail(out_len, MX_ERR_STATE);
    if (in->queue >= MXGPU_QUEUE_COUNT)
        return fail(out_len, MX_ERR_QUEUE);
    memset(bytes, 0, sizeof bytes);
    mx_w16(bytes, 0, in->descriptor_head);
    mx_w16(bytes, 2, in->queue);
    mx_w32(bytes, 4, in->written_bytes);
    mx_w32(bytes, 8, in->device_generation);
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

int mxgpu_used_decode(const uint8_t *in, uint32_t len, struct mxgpu_used_entry *out)
{
    if (!in || !out)
        return MX_ERR_STATE;
    if (len != MXGPU_USED_ENTRY_SIZE)
        return MX_ERR_LENGTH;
    if (mx_r16(in, 2) >= MXGPU_QUEUE_COUNT)
        return MX_ERR_QUEUE;
    if (mx_r32(in, 12) != 0)
        return MX_ERR_RESERVED;
    out->descriptor_head = mx_r16(in, 0);
    out->queue = mx_r16(in, 2);
    out->written_bytes = mx_r32(in, 4);
    out->device_generation = mx_r32(in, 8);
    return MX_OK;
}

int mxgpu_transfer_encode(const struct mxgpu_transfer *in, const uint8_t *data, uint8_t *out,
                          uint32_t cap, uint32_t *out_len)
{
    uint32_t total;
    if (!in || in->resource_id == 0 || in->data_bytes == 0 || !data)
        return fail(out_len, MX_ERR_RANGE);
    if (in->data_bytes > UINT32_MAX - MXGPU_TRANSFER_REQUEST_SIZE)
        return fail(out_len, MX_ERR_LENGTH);
    total = MXGPU_TRANSFER_REQUEST_SIZE + in->data_bytes;
    if (out_len)
        *out_len = 0;
    if (!out || cap < total)
        return MX_ERR_LENGTH;
    memset(out, 0, MXGPU_TRANSFER_REQUEST_SIZE);
    mx_w32(out, 0, in->resource_id);
    mx_w16(out, 4, in->mip_level);
    mx_w16(out, 6, in->array_layer);
    mx_w32(out, 8, in->x);
    mx_w32(out, 12, in->y);
    mx_w32(out, 16, in->z);
    mx_w32(out, 20, in->width);
    mx_w32(out, 24, in->height);
    mx_w32(out, 28, in->depth);
    mx_w64(out, 32, in->resource_offset);
    mx_w32(out, 40, in->row_bytes);
    mx_w32(out, 44, in->data_bytes);
    memcpy(out + MXGPU_TRANSFER_REQUEST_SIZE, data, in->data_bytes);
    if (out_len)
        *out_len = total;
    return MX_OK;
}

int mxgpu_transfer_decode(const uint8_t *in, uint32_t len, struct mxgpu_transfer *out,
                          const uint8_t **data)
{
    struct mxgpu_transfer rec;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len < MXGPU_TRANSFER_REQUEST_SIZE)
        return MX_ERR_LENGTH;
    rec.resource_id = mx_r32(in, 0);
    rec.mip_level = mx_r16(in, 4);
    rec.array_layer = mx_r16(in, 6);
    rec.x = mx_r32(in, 8);
    rec.y = mx_r32(in, 12);
    rec.z = mx_r32(in, 16);
    rec.width = mx_r32(in, 20);
    rec.height = mx_r32(in, 24);
    rec.depth = mx_r32(in, 28);
    rec.resource_offset = mx_r64(in, 32);
    rec.row_bytes = mx_r32(in, 40);
    rec.data_bytes = mx_r32(in, 44);
    if (rec.resource_id == 0 || rec.data_bytes == 0)
        return MX_ERR_RANGE;
    if (len != MXGPU_TRANSFER_REQUEST_SIZE + rec.data_bytes)
        return MX_ERR_LENGTH;
    *out = rec;
    if (data)
        *data = in + MXGPU_TRANSFER_REQUEST_SIZE;
    return MX_OK;
}

static int binding_ok(const struct mxgpu_execution_binding *in)
{
    if (!in || in->resource_id == 0)
        return MX_ERR_RESOURCE;
    if (in->access < MXGPU_BIND_ACCESS_READ || in->access > MXGPU_BIND_ACCESS_READ_WRITE)
        return MX_ERR_FLAGS;
    if (in->kind != MXGPU_BIND_KIND_BUFFER && in->kind != MXGPU_BIND_KIND_TEXTURE_2D &&
        in->kind != MXGPU_BIND_KIND_SAMPLER && in->kind != MXGPU_BIND_KIND_TEXTURE_CUBE)
        return MX_ERR_BINDING;
    if (in->space != 0)
        return MX_ERR_BINDING;
    if (in->kind == MXGPU_BIND_KIND_BUFFER) {
        if (in->size == 0 || in->offset + in->size < in->offset)
            return MX_ERR_RANGE;
        if (in->base_level || in->level_count || in->base_layer || in->layer_count)
            return MX_ERR_RANGE;
    } else if (in->offset || in->size || in->base_level || in->level_count || in->base_layer ||
               in->layer_count) {
        return MX_ERR_RANGE;
    }
    return MX_OK;
}

int mxgpu_binding_encode(const struct mxgpu_execution_binding *in, uint8_t *out, uint32_t cap,
                         uint32_t *out_len)
{
    uint8_t bytes[MXGPU_EXECUTION_BINDING_SIZE];
    int status = binding_ok(in);
    if (status != MX_OK)
        return fail(out_len, status);
    memset(bytes, 0, sizeof bytes);
    mx_w16(bytes, 0, in->slot);
    mx_w16(bytes, 2, in->access);
    mx_w16(bytes, 4, in->kind);
    mx_w16(bytes, 6, in->space);
    mx_w32(bytes, 8, in->resource_id);
    mx_w16(bytes, 12, in->base_level);
    mx_w16(bytes, 14, in->level_count);
    if (in->kind == MXGPU_BIND_KIND_BUFFER)
        mx_w64(bytes, 16, in->offset);
    else {
        mx_w32(bytes, 16, in->base_layer);
        mx_w32(bytes, 20, in->layer_count);
    }
    mx_w64(bytes, 24, in->size);
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

int mxgpu_binding_decode(const uint8_t *in, uint32_t len, struct mxgpu_execution_binding *out)
{
    struct mxgpu_execution_binding rec;
    int status;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len != MXGPU_EXECUTION_BINDING_SIZE)
        return MX_ERR_LENGTH;
    memset(&rec, 0, sizeof rec);
    rec.slot = mx_r16(in, 0);
    rec.access = mx_r16(in, 2);
    rec.kind = mx_r16(in, 4);
    rec.space = mx_r16(in, 6);
    rec.resource_id = mx_r32(in, 8);
    rec.base_level = mx_r16(in, 12);
    rec.level_count = mx_r16(in, 14);
    if (rec.kind == MXGPU_BIND_KIND_BUFFER)
        rec.offset = mx_r64(in, 16);
    else {
        rec.base_layer = mx_r32(in, 16);
        rec.layer_count = mx_r32(in, 20);
    }
    rec.size = mx_r64(in, 24);
    status = binding_ok(&rec);
    if (status != MX_OK)
        return status;
    *out = rec;
    return MX_OK;
}

int mxgpu_shader_create_encode(uint32_t shader_id, const uint8_t *bytecode, uint32_t bytecode_len,
                               uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint32_t total;
    if (shader_id == 0 || !bytecode || bytecode_len == 0 || (bytecode_len & 3u))
        return fail(out_len, MX_ERR_SHADER);
    total = MXGPU_SHADER_CREATE_HEADER_SIZE + bytecode_len;
    if (out_len)
        *out_len = 0;
    if (!out || cap < total)
        return MX_ERR_LENGTH;
    memset(out, 0, MXGPU_SHADER_CREATE_HEADER_SIZE);
    mx_w32(out, 0, shader_id);
    mx_w32(out, 4, bytecode_len);
    memcpy(out + MXGPU_SHADER_CREATE_HEADER_SIZE, bytecode, bytecode_len);
    if (out_len)
        *out_len = total;
    return MX_OK;
}

int mxgpu_pipeline_create_encode(uint32_t pipeline_id, uint16_t kind, uint16_t color_format,
                                 uint32_t shader_id, uint32_t first_entry, uint32_t second_entry,
                                 uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint8_t bytes[MXGPU_PIPELINE_CREATE_SIZE];
    if (pipeline_id == 0 || shader_id == 0 || first_entry == 0)
        return fail(out_len, MX_ERR_RESOURCE);
    if (kind == MXGPU_PIPELINE_COMPUTE) {
        if (color_format != 0 || second_entry != 0)
            return fail(out_len, MX_ERR_STATE);
    } else if (kind == MXGPU_PIPELINE_RENDER) {
        if (!known_format(color_format) || second_entry == 0 || second_entry == first_entry)
            return fail(out_len, MX_ERR_STATE);
    } else {
        return fail(out_len, MX_ERR_STATE);
    }
    memset(bytes, 0, sizeof bytes);
    mx_w32(bytes, 0, pipeline_id);
    mx_w16(bytes, 4, kind);
    mx_w16(bytes, 6, color_format);
    mx_w32(bytes, 8, shader_id);
    mx_w32(bytes, 12, first_entry);
    mx_w32(bytes, 16, second_entry);
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

int mxgpu_render_submit_encode(const struct mxgpu_render_submit *in,
                               const struct mxgpu_execution_binding *bindings, uint8_t *out,
                               uint32_t cap, uint32_t *out_len)
{
    uint32_t total;
    uint32_t i;
    if (!in || in->pipeline_id == 0 || in->color_target_id == 0)
        return fail(out_len, MX_ERR_RESOURCE);
    if (in->load_action < MXGPU_LOAD_LOAD || in->load_action > MXGPU_LOAD_DONT_CARE)
        return fail(out_len, MX_ERR_STATE);
    if (in->store_action < MXGPU_STORE_STORE || in->store_action > MXGPU_STORE_DONT_CARE)
        return fail(out_len, MX_ERR_STATE);
    if (in->draw_kind != MXGPU_DRAW_NON_INDEXED || in->primitive != MXGPU_PRIM_TRIANGLE)
        return fail(out_len, MX_ERR_STATE);
    if (in->index_type || in->draw_flags || in->base_vertex || in->index_resource_id ||
        in->index_buffer_offset)
        return fail(out_len, MX_ERR_STATE);
    if (in->element_count < 3 || (in->element_count % 3) != 0 || in->instance_count == 0)
        return fail(out_len, MX_ERR_RANGE);
    if (in->load_action != MXGPU_LOAD_CLEAR) {
        for (i = 0; i < 4; i++) {
            if (in->clear_rgba[i] != 0)
                return fail(out_len, MX_ERR_STATE);
        }
    } else {
        for (i = 0; i < 4; i++) {
            if (!mxgpu_f32_finite(in->clear_rgba[i]))
                return fail(out_len, MX_ERR_STATE);
        }
    }
    if (in->binding_count && !bindings)
        return fail(out_len, MX_ERR_STATE);
    for (i = 0; i < in->binding_count; i++) {
        if (binding_ok(&bindings[i]) != MX_OK)
            return fail(out_len, MX_ERR_BINDING);
    }
    total = MXGPU_RENDER_SUBMIT_HEADER_SIZE + in->binding_count * MXGPU_EXECUTION_BINDING_SIZE;
    if (out_len)
        *out_len = 0;
    if (!out || cap < total)
        return MX_ERR_LENGTH;
    memset(out, 0, total);
    mx_w32(out, 0, in->pipeline_id);
    mx_w32(out, 4, in->color_target_id);
    mx_w16(out, 8, in->binding_count);
    out[10] = in->load_action;
    out[11] = in->store_action;
    out[12] = in->draw_kind;
    out[13] = in->primitive;
    out[14] = in->index_type;
    out[15] = in->draw_flags;
    for (i = 0; i < 4; i++)
        mx_w32(out, 16 + i * 4, in->clear_rgba[i]);
    mx_w32(out, 32, in->element_start);
    mx_w32(out, 36, in->element_count);
    mx_w32(out, 40, in->instance_count);
    mx_w32(out, 44, (uint32_t)in->base_vertex);
    mx_w32(out, 48, in->base_instance);
    mx_w32(out, 52, in->index_resource_id);
    mx_w64(out, 56, in->index_buffer_offset);
    mx_w16(out, 64, in->color_target_mip_level);
    for (i = 0; i < in->binding_count; i++) {
        uint32_t wrote = 0;
        if (mxgpu_binding_encode(&bindings[i],
                                 out + MXGPU_RENDER_SUBMIT_HEADER_SIZE +
                                     i * MXGPU_EXECUTION_BINDING_SIZE,
                                 MXGPU_EXECUTION_BINDING_SIZE, &wrote) != MX_OK)
            return fail(out_len, MX_ERR_BINDING);
    }
    if (out_len)
        *out_len = total;
    return MX_OK;
}

int mxgpu_render_submit_decode(const uint8_t *in, uint32_t len, struct mxgpu_render_submit *out,
                               struct mxgpu_execution_binding *bindings, uint32_t binding_cap)
{
    struct mxgpu_render_submit rec;
    uint32_t i;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len < MXGPU_RENDER_SUBMIT_HEADER_SIZE)
        return MX_ERR_LENGTH;
    memset(&rec, 0, sizeof rec);
    rec.pipeline_id = mx_r32(in, 0);
    rec.color_target_id = mx_r32(in, 4);
    rec.binding_count = mx_r16(in, 8);
    rec.load_action = in[10];
    rec.store_action = in[11];
    rec.draw_kind = in[12];
    rec.primitive = in[13];
    rec.index_type = in[14];
    rec.draw_flags = in[15];
    for (i = 0; i < 4; i++)
        rec.clear_rgba[i] = mx_r32(in, 16 + i * 4);
    rec.element_start = mx_r32(in, 32);
    rec.element_count = mx_r32(in, 36);
    rec.instance_count = mx_r32(in, 40);
    rec.base_vertex = (int32_t)mx_r32(in, 44);
    rec.base_instance = mx_r32(in, 48);
    rec.index_resource_id = mx_r32(in, 52);
    rec.index_buffer_offset = mx_r64(in, 56);
    rec.color_target_mip_level = mx_r16(in, 64);
    if (mx_r16(in, 66) != 0 || mx_r32(in, 68) != 0)
        return MX_ERR_RESERVED;
    if (len != MXGPU_RENDER_SUBMIT_HEADER_SIZE + rec.binding_count * MXGPU_EXECUTION_BINDING_SIZE)
        return MX_ERR_LENGTH;
    if (rec.binding_count > binding_cap || (rec.binding_count && !bindings))
        return MX_ERR_LIMIT;
    for (i = 0; i < rec.binding_count; i++) {
        int status = mxgpu_binding_decode(in + MXGPU_RENDER_SUBMIT_HEADER_SIZE +
                                              i * MXGPU_EXECUTION_BINDING_SIZE,
                                          MXGPU_EXECUTION_BINDING_SIZE, &bindings[i]);
        if (status != MX_OK)
            return status;
    }
    if (rec.pipeline_id == 0 || rec.color_target_id == 0)
        return MX_ERR_RESOURCE;
    if (rec.element_count < 3 || rec.instance_count == 0)
        return MX_ERR_RANGE;
    *out = rec;
    return MX_OK;
}

int mxgpu_present_encode(const struct mxgpu_present *in, uint8_t *out, uint32_t cap,
                         uint32_t *out_len)
{
    uint32_t total;
    uint32_t i;
    if (!in || in->resource_id == 0)
        return fail(out_len, MX_ERR_RESOURCE);
    if (in->flags & ~((1u << 0) | (1u << 1) | (1u << 2)))
        return fail(out_len, MX_ERR_FLAGS);
    if (in->damage_count > MXGPU_MAX_PRESENT_DAMAGE_RECTS)
        return fail(out_len, MX_ERR_LIMIT);
    if (in->damage_count && !in->damage)
        return fail(out_len, MX_ERR_STATE);
    if (in->source.width == 0 || in->source.height == 0)
        return fail(out_len, MX_ERR_RANGE);
    total = MXGPU_PRESENT_HEADER_SIZE + in->damage_count * MXGPU_PRESENT_DAMAGE_RECT_SIZE;
    if (out_len)
        *out_len = 0;
    if (!out || cap < total)
        return MX_ERR_LENGTH;
    memset(out, 0, total);
    mx_w32(out, 0, in->resource_id);
    mx_w16(out, 4, in->scanout_id);
    mx_w16(out, 6, in->flags);
    mx_w32(out, 8, in->source.x);
    mx_w32(out, 12, in->source.y);
    mx_w32(out, 16, in->source.width);
    mx_w32(out, 20, in->source.height);
    mx_w32(out, 24, in->damage_count);
    for (i = 0; i < in->damage_count; i++) {
        uint32_t off = MXGPU_PRESENT_HEADER_SIZE + i * MXGPU_PRESENT_DAMAGE_RECT_SIZE;
        mx_w32(out, off, in->damage[i].x);
        mx_w32(out, off + 4, in->damage[i].y);
        mx_w32(out, off + 8, in->damage[i].width);
        mx_w32(out, off + 12, in->damage[i].height);
    }
    if (out_len)
        *out_len = total;
    return MX_OK;
}

int mxgpu_present_decode(const uint8_t *in, uint32_t len, struct mxgpu_present *out,
                         struct mxgpu_present_rect *damage, uint32_t damage_cap)
{
    struct mxgpu_present rec;
    uint32_t i;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len < MXGPU_PRESENT_HEADER_SIZE)
        return MX_ERR_LENGTH;
    memset(&rec, 0, sizeof rec);
    rec.resource_id = mx_r32(in, 0);
    rec.scanout_id = mx_r16(in, 4);
    rec.flags = mx_r16(in, 6);
    rec.source.x = mx_r32(in, 8);
    rec.source.y = mx_r32(in, 12);
    rec.source.width = mx_r32(in, 16);
    rec.source.height = mx_r32(in, 20);
    rec.damage_count = mx_r32(in, 24);
    if (mx_r32(in, 28) != 0)
        return MX_ERR_RESERVED;
    if (rec.flags & ~((1u << 0) | (1u << 1) | (1u << 2)))
        return MX_ERR_FLAGS;
    if (rec.resource_id == 0)
        return MX_ERR_RESOURCE;
    if (len != MXGPU_PRESENT_HEADER_SIZE + rec.damage_count * MXGPU_PRESENT_DAMAGE_RECT_SIZE)
        return MX_ERR_LENGTH;
    if (rec.damage_count > damage_cap)
        return MX_ERR_LIMIT;
    for (i = 0; i < rec.damage_count; i++) {
        uint32_t off = MXGPU_PRESENT_HEADER_SIZE + i * MXGPU_PRESENT_DAMAGE_RECT_SIZE;
        damage[i].x = mx_r32(in, off);
        damage[i].y = mx_r32(in, off + 4);
        damage[i].width = mx_r32(in, off + 8);
        damage[i].height = mx_r32(in, off + 12);
    }
    rec.damage = damage;
    *out = rec;
    return MX_OK;
}

static int blend_state_ok(const struct mxgpu_blend_state *in)
{
    uint32_t i;
    if (!in || !in->state_id || !in->target_count || in->target_count > 8)
        return MX_ERR_STATE;
    for (i = 0; i < in->target_count; i++) {
        const struct mxgpu_blend_target *t = &in->targets[i];
        if (t->enable > 1 || (t->write_mask & ~15u) || t->src_color < 1 || t->src_color > 15 ||
            t->dst_color < 1 || t->dst_color > 15 || t->src_alpha < 1 || t->src_alpha > 15 ||
            t->dst_alpha < 1 || t->dst_alpha > 15 || t->color_op < 1 || t->color_op > 5 ||
            t->alpha_op < 1 || t->alpha_op > 5)
            return MX_ERR_STATE;
    }
    return MX_OK;
}

int mxgpu_blend_state_encode(const struct mxgpu_blend_state *in, uint8_t *out, uint32_t cap,
                             uint32_t *out_len)
{
    uint8_t bytes[MXGPU_BLEND_STATE_HEADER_SIZE + 8 * MXGPU_BLEND_TARGET_SIZE];
    uint32_t i, n;
    int status = blend_state_ok(in);
    if (status != MX_OK)
        return fail(out_len, status);
    n = MXGPU_BLEND_STATE_HEADER_SIZE + in->target_count * MXGPU_BLEND_TARGET_SIZE;
    memset(bytes, 0, n);
    mx_w32(bytes, 0, in->state_id);
    bytes[4] = in->target_count;
    for (i = 0; i < in->target_count; i++) {
        const struct mxgpu_blend_target *t = &in->targets[i];
        uint8_t *p = bytes + 8 + 16 * i;
        p[0] = t->enable;
        p[1] = t->write_mask;
        p[4] = t->src_color;
        p[5] = t->dst_color;
        p[6] = t->color_op;
        p[7] = t->src_alpha;
        p[8] = t->dst_alpha;
        p[9] = t->alpha_op;
    }
    return emit(out, cap, out_len, bytes, n);
}

int mxgpu_blend_state_decode(const uint8_t *in, uint32_t len, struct mxgpu_blend_state *out)
{
    struct mxgpu_blend_state rec;
    uint32_t i;
    int status;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len < 8 || in[4] > 8 || len != 8u + in[4] * 16u)
        return MX_ERR_LENGTH;
    if (in[5] || in[6] || in[7])
        return MX_ERR_RESERVED;
    memset(&rec, 0, sizeof rec);
    rec.state_id = mx_r32(in, 0);
    rec.target_count = in[4];
    for (i = 0; i < rec.target_count; i++) {
        const uint8_t *p = in + 8 + 16 * i;
        struct mxgpu_blend_target *t = &rec.targets[i];
        if (mx_r16(p, 2) || mx_r16(p, 10) || mx_r32(p, 12))
            return MX_ERR_RESERVED;
        t->enable = p[0];
        t->write_mask = p[1];
        t->src_color = p[4];
        t->dst_color = p[5];
        t->color_op = p[6];
        t->src_alpha = p[7];
        t->dst_alpha = p[8];
        t->alpha_op = p[9];
    }
    status = blend_state_ok(&rec);
    if (status == MX_OK)
        *out = rec;
    return status;
}

static int rasterizer_state_ok(const struct mxgpu_rasterizer_state *in)
{
    if (!in || !in->state_id || in->fill_mode < 1 || in->fill_mode > 2 || in->cull_mode < 1 ||
        in->cull_mode > 3 || in->front_face < 1 || in->front_face > 2 ||
        in->depth_clip_enable > 1 || in->scissor_enable > 1 || in->multisample_enable > 1 ||
        in->antialiased_line_enable > 1 || !mxgpu_f32_finite(in->depth_bias_clamp) ||
        !mxgpu_f32_finite(in->slope_scaled_depth_bias))
        return MX_ERR_STATE;
    return MX_OK;
}

int mxgpu_rasterizer_state_encode(const struct mxgpu_rasterizer_state *in, uint8_t *out,
                                  uint32_t cap, uint32_t *out_len)
{
    uint8_t bytes[MXGPU_RASTERIZER_STATE_SIZE];
    int status = rasterizer_state_ok(in);
    if (status != MX_OK)
        return fail(out_len, status);
    memset(bytes, 0, sizeof bytes);
    mx_w32(bytes, 0, in->state_id);
    bytes[4] = in->fill_mode;
    bytes[5] = in->cull_mode;
    bytes[6] = in->front_face;
    bytes[7] = in->depth_clip_enable;
    bytes[8] = in->scissor_enable;
    bytes[9] = in->multisample_enable;
    bytes[10] = in->antialiased_line_enable;
    mx_w32(bytes, 12, (uint32_t)in->depth_bias);
    mx_w32(bytes, 16, in->depth_bias_clamp);
    mx_w32(bytes, 20, in->slope_scaled_depth_bias);
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

int mxgpu_rasterizer_state_decode(const uint8_t *in, uint32_t len,
                                  struct mxgpu_rasterizer_state *out)
{
    struct mxgpu_rasterizer_state rec;
    int status;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len != MXGPU_RASTERIZER_STATE_SIZE)
        return MX_ERR_LENGTH;
    if (in[11])
        return MX_ERR_RESERVED;
    memset(&rec, 0, sizeof rec);
    rec.state_id = mx_r32(in, 0);
    rec.fill_mode = in[4];
    rec.cull_mode = in[5];
    rec.front_face = in[6];
    rec.depth_clip_enable = in[7];
    rec.scissor_enable = in[8];
    rec.multisample_enable = in[9];
    rec.antialiased_line_enable = in[10];
    rec.depth_bias = (int32_t)mx_r32(in, 12);
    rec.depth_bias_clamp = mx_r32(in, 16);
    rec.slope_scaled_depth_bias = mx_r32(in, 20);
    status = rasterizer_state_ok(&rec);
    if (status == MX_OK)
        *out = rec;
    return status;
}

static int f32_zero(uint32_t bits)
{
    return (bits & 0x7fffffffu) == 0;
}
static int action_ok(uint8_t load, uint8_t store)
{
    return load >= MXGPU_LOAD_LOAD && load <= MXGPU_LOAD_DONT_CARE && store >= MXGPU_STORE_STORE &&
           store <= MXGPU_STORE_DONT_CARE;
}

static int render_color_ok(const struct mxgpu_color_attachment *t)
{
    uint32_t j;
    if (!t->resource_id || !known_format(t->format) || !action_ok(t->load_action, t->store_action))
        return MX_ERR_STATE;
    for (j = 0; j < 4; j++)
        if (!mxgpu_f32_finite(t->clear_rgba[j]) ||
            (t->load_action != MXGPU_LOAD_CLEAR && !f32_zero(t->clear_rgba[j])))
            return MX_ERR_STATE;
    return MX_OK;
}

static int render_viewport_ok(const struct mxgpu_viewport *v)
{
    if (!mxgpu_f32_finite(v->x) || !mxgpu_f32_finite(v->y) || !mxgpu_f32_finite(v->width) ||
        !mxgpu_f32_finite(v->height) || !mxgpu_f32_finite(v->min_depth) ||
        !mxgpu_f32_finite(v->max_depth) || (v->width & 0x80000000u) || f32_zero(v->width) ||
        f32_zero(v->height) || ((v->min_depth & 0x80000000u) && !f32_zero(v->min_depth)) ||
        ((v->max_depth & 0x80000000u) && !f32_zero(v->max_depth)) ||
        (v->max_depth & 0x7fffffffu) > 0x3f800000u ||
        (v->min_depth & 0x7fffffffu) > (v->max_depth & 0x7fffffffu))
        return MX_ERR_STATE;
    return MX_OK;
}

struct mxgpu_render_header {
    uint32_t pipeline_id, rasterizer_state_id, depth_stencil_state_id, blend_state_id;
    uint32_t vertex_layout_id, depth_stencil_target_id;
    uint16_t depth_stencil_target_mip_level;
    uint8_t depth_load_action, depth_store_action, stencil_load_action, stencil_store_action;
    uint32_t depth_clear_value, stencil_clear_value, stencil_reference, blend_factor[4];
    uint8_t color_target_count, viewport_count, scissor_count, vertex_buffer_count;
    uint16_t binding_count;
    uint8_t draw_kind, primitive, index_type, draw_flags;
    uint32_t element_start, element_count, instance_count;
    int32_t base_vertex;
    uint32_t base_instance, index_resource_id;
    uint64_t index_buffer_offset;
};

static int render_header_ok(const struct mxgpu_render_header *r)
{
    uint32_t i;
    if (!r->pipeline_id || !r->rasterizer_state_id ||
        (!r->color_target_count && !r->depth_stencil_target_id))
        return MX_ERR_RESOURCE;
    if (r->color_target_count > 8 || r->viewport_count > 16 || r->scissor_count > 16 ||
        r->vertex_buffer_count > 32)
        return MX_ERR_RANGE;
    if (!mxgpu_f32_finite(r->depth_clear_value) || r->stencil_clear_value > 255 ||
        r->stencil_reference > 255)
        return MX_ERR_STATE;
    if (!r->depth_stencil_target_id) {
        if (r->depth_stencil_target_mip_level || r->depth_load_action || r->depth_store_action ||
            r->stencil_load_action || r->stencil_store_action || !f32_zero(r->depth_clear_value) ||
            r->stencil_clear_value)
            return MX_ERR_STATE;
    } else if (!action_ok(r->depth_load_action, r->depth_store_action) ||
               !action_ok(r->stencil_load_action, r->stencil_store_action) ||
               (r->depth_load_action != MXGPU_LOAD_CLEAR && !f32_zero(r->depth_clear_value)) ||
               (r->stencil_load_action != MXGPU_LOAD_CLEAR && r->stencil_clear_value))
        return MX_ERR_STATE;
    for (i = 0; i < 4; i++)
        if (!mxgpu_f32_finite(r->blend_factor[i]) ||
            (!r->blend_state_id && !f32_zero(r->blend_factor[i])))
            return MX_ERR_STATE;
    return MX_OK;
}

static int render_draw_ok(const struct mxgpu_render_header *r)
{
    if (r->draw_kind != MXGPU_DRAW_NON_INDEXED || r->primitive != MXGPU_PRIM_TRIANGLE ||
        r->index_type || r->draw_flags || r->base_vertex || r->index_resource_id ||
        r->index_buffer_offset)
        return MX_ERR_STATE;
    if (r->element_count < 3 || (r->element_count % 3) || !r->instance_count ||
        r->base_instance > UINT32_MAX - r->instance_count ||
        r->element_start > UINT32_MAX - r->element_count)
        return MX_ERR_RANGE;
    return MX_OK;
}

static void render_header_from_record(const struct mxgpu_render_extended *r,
                                      struct mxgpu_render_header *h)
{
    uint32_t i;
    h->pipeline_id = r->pipeline_id;
    h->rasterizer_state_id = r->rasterizer_state_id;
    h->depth_stencil_state_id = r->depth_stencil_state_id;
    h->blend_state_id = r->blend_state_id;
    h->vertex_layout_id = r->vertex_layout_id;
    h->depth_stencil_target_id = r->depth_stencil_target_id;
    h->depth_stencil_target_mip_level = r->depth_stencil_target_mip_level;
    h->depth_load_action = r->depth_load_action;
    h->depth_store_action = r->depth_store_action;
    h->stencil_load_action = r->stencil_load_action;
    h->stencil_store_action = r->stencil_store_action;
    h->depth_clear_value = r->depth_clear_value;
    h->stencil_clear_value = r->stencil_clear_value;
    h->stencil_reference = r->stencil_reference;
    for (i = 0; i < 4; i++)
        h->blend_factor[i] = r->blend_factor[i];
    h->color_target_count = r->color_target_count;
    h->viewport_count = r->viewport_count;
    h->scissor_count = r->scissor_count;
    h->vertex_buffer_count = r->vertex_buffer_count;
    h->binding_count = r->binding_count;
    h->draw_kind = r->draw_kind;
    h->primitive = r->primitive;
    h->index_type = r->index_type;
    h->draw_flags = r->draw_flags;
    h->element_start = r->element_start;
    h->element_count = r->element_count;
    h->instance_count = r->instance_count;
    h->base_vertex = r->base_vertex;
    h->base_instance = r->base_instance;
    h->index_resource_id = r->index_resource_id;
    h->index_buffer_offset = r->index_buffer_offset;
}

static int render_extended_ok(const struct mxgpu_render_extended *r)
{
    struct mxgpu_render_header header;
    uint32_t i, j;
    int status;
    if (!r)
        return MX_ERR_RESOURCE;
    render_header_from_record(r, &header);
    status = render_header_ok(&header);
    if (status != MX_OK)
        return status;
    for (i = 0; i < r->color_target_count; i++) {
        const struct mxgpu_color_attachment *t = &r->color_targets[i];
        status = render_color_ok(t);
        if (status != MX_OK)
            return status;
        for (j = 0; j < i; j++)
            if (t->resource_id == r->color_targets[j].resource_id &&
                t->mip_level == r->color_targets[j].mip_level)
                return MX_ERR_STATE;
    }
    for (i = 0; i < r->viewport_count; i++) {
        status = render_viewport_ok(&r->viewports[i]);
        if (status != MX_OK)
            return status;
    }
    for (i = 0; i < r->scissor_count; i++)
        if (r->scissors[i].right <= r->scissors[i].left ||
            r->scissors[i].bottom <= r->scissors[i].top)
            return MX_ERR_RANGE;
    for (i = 0; i < r->vertex_buffer_count; i++)
        if (!r->vertex_buffers[i].resource_id)
            return MX_ERR_RESOURCE;
    return render_draw_ok(&header);
}

int mxgpu_render_extended_features(const struct mxgpu_render_extended *r, uint64_t features)
{
    uint64_t required = MXGPU_FEAT_RENDER | MXGPU_FEAT_RASTERIZER_STATE;
    uint32_t i;
    int status = render_extended_ok(r);
    if (status != MX_OK)
        return status;
    if (r->color_target_count > 1)
        required |= MXGPU_FEAT_MULTIPLE_RENDER_TARGETS;
    if (r->depth_stencil_target_id || r->depth_stencil_state_id)
        required |= MXGPU_FEAT_DEPTH_STENCIL_TARGET;
    if (r->blend_state_id)
        required |= MXGPU_FEAT_BLEND_STATE;
    if (r->viewport_count || r->scissor_count)
        required |= MXGPU_FEAT_VIEWPORT_SCISSOR;
    if (r->vertex_layout_id || r->vertex_buffer_count)
        required |= MXGPU_FEAT_VERTEX_INPUT_LAYOUT;
    for (i = 0; i < r->viewport_count; i++)
        if (r->viewports[i].height & 0x80000000u)
            required |= MXGPU_FEAT_VIEWPORT_Y_FLIP;
    return (features & required) == required ? MX_OK : MX_ERR_STATE;
}

static uint32_t render_extended_size(const struct mxgpu_render_extended *r)
{
    return 104u + r->color_target_count * 32u + r->viewport_count * 24u + r->scissor_count * 16u +
           r->vertex_buffer_count * 24u + r->binding_count * 32u;
}

int mxgpu_render_extended_encode(const struct mxgpu_render_extended *r,
                                 const struct mxgpu_execution_binding *bindings, uint8_t *out,
                                 uint32_t cap, uint32_t *out_len)
{
    uint32_t i, j, cursor, n;
    int status = render_extended_ok(r);
    if (status != MX_OK)
        return fail(out_len, status);
    if (r->binding_count && !bindings)
        return fail(out_len, MX_ERR_BINDING);
    for (i = 0; i < r->binding_count; i++) {
        if (binding_ok(&bindings[i]) != MX_OK)
            return fail(out_len, MX_ERR_BINDING);
        for (j = 0; j < i; j++)
            if (bindings[i].slot == bindings[j].slot && bindings[i].space == bindings[j].space)
                return fail(out_len, MX_ERR_BINDING);
    }
    n = render_extended_size(r);
    if (out_len)
        *out_len = 0;
    if (!out || cap < n)
        return MX_ERR_LENGTH;
    memset(out, 0, n);
    mx_w32(out, 0, r->pipeline_id);
    mx_w32(out, 4, r->rasterizer_state_id);
    mx_w32(out, 8, r->depth_stencil_state_id);
    mx_w32(out, 12, r->blend_state_id);
    mx_w32(out, 16, r->vertex_layout_id);
    mx_w32(out, 20, r->depth_stencil_target_id);
    mx_w16(out, 24, r->depth_stencil_target_mip_level);
    out[26] = r->depth_load_action;
    out[27] = r->depth_store_action;
    out[28] = r->stencil_load_action;
    out[29] = r->stencil_store_action;
    mx_w32(out, 32, r->depth_clear_value);
    mx_w32(out, 36, r->stencil_clear_value);
    mx_w32(out, 40, r->stencil_reference);
    for (i = 0; i < 4; i++)
        mx_w32(out, 44 + 4 * i, r->blend_factor[i]);
    out[60] = r->color_target_count;
    out[61] = r->viewport_count;
    out[62] = r->scissor_count;
    out[63] = r->vertex_buffer_count;
    mx_w16(out, 64, r->binding_count);
    out[66] = r->draw_kind;
    out[67] = r->primitive;
    out[68] = r->index_type;
    out[69] = r->draw_flags;
    mx_w32(out, 72, r->element_start);
    mx_w32(out, 76, r->element_count);
    mx_w32(out, 80, r->instance_count);
    mx_w32(out, 84, (uint32_t)r->base_vertex);
    mx_w32(out, 88, r->base_instance);
    mx_w32(out, 92, r->index_resource_id);
    mx_w64(out, 96, r->index_buffer_offset);
    cursor = 104;
    for (i = 0; i < r->color_target_count; i++, cursor += 32) {
        const struct mxgpu_color_attachment *t = &r->color_targets[i];
        mx_w32(out, cursor, t->resource_id);
        mx_w16(out, cursor + 4, t->mip_level);
        mx_w16(out, cursor + 6, t->format);
        out[cursor + 8] = t->load_action;
        out[cursor + 9] = t->store_action;
        for (j = 0; j < 4; j++)
            mx_w32(out, cursor + 12 + 4 * j, t->clear_rgba[j]);
    }
    for (i = 0; i < r->viewport_count; i++, cursor += 24) {
        const struct mxgpu_viewport *v = &r->viewports[i];
        mx_w32(out, cursor, v->x);
        mx_w32(out, cursor + 4, v->y);
        mx_w32(out, cursor + 8, v->width);
        mx_w32(out, cursor + 12, v->height);
        mx_w32(out, cursor + 16, v->min_depth);
        mx_w32(out, cursor + 20, v->max_depth);
    }
    for (i = 0; i < r->scissor_count; i++, cursor += 16) {
        const struct mxgpu_scissor *s = &r->scissors[i];
        mx_w32(out, cursor, s->left);
        mx_w32(out, cursor + 4, s->top);
        mx_w32(out, cursor + 8, s->right);
        mx_w32(out, cursor + 12, s->bottom);
    }
    for (i = 0; i < r->vertex_buffer_count; i++, cursor += 24) {
        mx_w32(out, cursor, r->vertex_buffers[i].resource_id);
        mx_w32(out, cursor + 8, r->vertex_buffers[i].stride);
        mx_w64(out, cursor + 16, r->vertex_buffers[i].offset);
    }
    for (i = 0; i < r->binding_count; i++, cursor += 32)
        if (mxgpu_binding_encode(&bindings[i], out + cursor, 32, NULL) != MX_OK)
            return fail(out_len, MX_ERR_BINDING);
    if (out_len)
        *out_len = n;
    return MX_OK;
}

static void render_header_read(const uint8_t *in, struct mxgpu_render_header *r)
{
    uint32_t i;
    memset(r, 0, sizeof *r);
    r->pipeline_id = mx_r32(in, 0);
    r->rasterizer_state_id = mx_r32(in, 4);
    r->depth_stencil_state_id = mx_r32(in, 8);
    r->blend_state_id = mx_r32(in, 12);
    r->vertex_layout_id = mx_r32(in, 16);
    r->depth_stencil_target_id = mx_r32(in, 20);
    r->depth_stencil_target_mip_level = mx_r16(in, 24);
    r->depth_load_action = in[26];
    r->depth_store_action = in[27];
    r->stencil_load_action = in[28];
    r->stencil_store_action = in[29];
    r->depth_clear_value = mx_r32(in, 32);
    r->stencil_clear_value = mx_r32(in, 36);
    r->stencil_reference = mx_r32(in, 40);
    for (i = 0; i < 4; i++)
        r->blend_factor[i] = mx_r32(in, 44 + 4 * i);
    r->color_target_count = in[60];
    r->viewport_count = in[61];
    r->scissor_count = in[62];
    r->vertex_buffer_count = in[63];
    r->binding_count = mx_r16(in, 64);
    r->draw_kind = in[66];
    r->primitive = in[67];
    r->index_type = in[68];
    r->draw_flags = in[69];
    r->element_start = mx_r32(in, 72);
    r->element_count = mx_r32(in, 76);
    r->instance_count = mx_r32(in, 80);
    r->base_vertex = signed32_from_bits(mx_r32(in, 84));
    r->base_instance = mx_r32(in, 88);
    r->index_resource_id = mx_r32(in, 92);
    r->index_buffer_offset = mx_r64(in, 96);
}

static void render_header_store(const struct mxgpu_render_header *h,
                                struct mxgpu_render_extended *out)
{
    uint32_t i;
    out->pipeline_id = h->pipeline_id;
    out->rasterizer_state_id = h->rasterizer_state_id;
    out->depth_stencil_state_id = h->depth_stencil_state_id;
    out->blend_state_id = h->blend_state_id;
    out->vertex_layout_id = h->vertex_layout_id;
    out->depth_stencil_target_id = h->depth_stencil_target_id;
    out->depth_stencil_target_mip_level = h->depth_stencil_target_mip_level;
    out->depth_load_action = h->depth_load_action;
    out->depth_store_action = h->depth_store_action;
    out->stencil_load_action = h->stencil_load_action;
    out->stencil_store_action = h->stencil_store_action;
    out->depth_clear_value = h->depth_clear_value;
    out->stencil_clear_value = h->stencil_clear_value;
    out->stencil_reference = h->stencil_reference;
    for (i = 0; i < 4; i++)
        out->blend_factor[i] = h->blend_factor[i];
    out->color_target_count = h->color_target_count;
    out->viewport_count = h->viewport_count;
    out->scissor_count = h->scissor_count;
    out->vertex_buffer_count = h->vertex_buffer_count;
    out->binding_count = h->binding_count;
    out->draw_kind = h->draw_kind;
    out->primitive = h->primitive;
    out->index_type = h->index_type;
    out->draw_flags = h->draw_flags;
    out->element_start = h->element_start;
    out->element_count = h->element_count;
    out->instance_count = h->instance_count;
    out->base_vertex = h->base_vertex;
    out->base_instance = h->base_instance;
    out->index_resource_id = h->index_resource_id;
    out->index_buffer_offset = h->index_buffer_offset;
}

static void render_color_read(const uint8_t *in, struct mxgpu_color_attachment *t)
{
    uint32_t j;
    t->resource_id = mx_r32(in, 0);
    t->mip_level = mx_r16(in, 4);
    t->format = mx_r16(in, 6);
    t->load_action = in[8];
    t->store_action = in[9];
    for (j = 0; j < 4; j++)
        t->clear_rgba[j] = mx_r32(in, 12 + 4 * j);
}

static void render_viewport_read(const uint8_t *in, struct mxgpu_viewport *v)
{
    v->x = mx_r32(in, 0);
    v->y = mx_r32(in, 4);
    v->width = mx_r32(in, 8);
    v->height = mx_r32(in, 12);
    v->min_depth = mx_r32(in, 16);
    v->max_depth = mx_r32(in, 20);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((__noinline__))
#endif
static void render_decode_alias_tail(const uint8_t *in, const struct mxgpu_render_header *h,
                                     const struct mxgpu_color_attachment *colors,
                                     const struct mxgpu_viewport *viewports,
                                     struct mxgpu_render_extended *out,
                                     struct mxgpu_execution_binding *bindings)
{
    struct mxgpu_scissor scissors[16] = {{0}};
    struct mxgpu_vertex_buffer_binding vertices[32] = {{0}};
    uint32_t i, cursor = 104u + h->color_target_count * 32u + h->viewport_count * 24u;
    for (i = 0; i < h->scissor_count; i++, cursor += 16) {
        scissors[i].left = mx_r32(in, cursor);
        scissors[i].top = mx_r32(in, cursor + 4);
        scissors[i].right = mx_r32(in, cursor + 8);
        scissors[i].bottom = mx_r32(in, cursor + 12);
    }
    for (i = 0; i < h->vertex_buffer_count; i++, cursor += 24) {
        vertices[i].resource_id = mx_r32(in, cursor);
        vertices[i].stride = mx_r32(in, cursor + 8);
        vertices[i].offset = mx_r64(in, cursor + 16);
    }
    for (i = 0; i < h->binding_count; i++, cursor += 32)
        (void)mxgpu_binding_decode(in + cursor, 32, &bindings[i]);
    memset(out, 0, sizeof *out);
    render_header_store(h, out);
    memcpy(out->color_targets, colors, sizeof out->color_targets);
    memcpy(out->viewports, viewports, sizeof out->viewports);
    memcpy(out->scissors, scissors, sizeof scissors);
    memcpy(out->vertex_buffers, vertices, sizeof vertices);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((__noinline__))
#endif
static void render_decode_alias(const uint8_t *in, const struct mxgpu_render_header *h,
                                struct mxgpu_render_extended *out,
                                struct mxgpu_execution_binding *bindings)
{
    struct mxgpu_color_attachment colors[8] = {{0}};
    struct mxgpu_viewport viewports[16] = {{0}};
    uint32_t i, cursor = 104;
    for (i = 0; i < h->color_target_count; i++, cursor += 32)
        render_color_read(in + cursor, &colors[i]);
    for (i = 0; i < h->viewport_count; i++, cursor += 24)
        render_viewport_read(in + cursor, &viewports[i]);
    render_decode_alias_tail(in, h, colors, viewports, out, bindings);
}

int mxgpu_render_extended_decode(const uint8_t *in, uint32_t len, struct mxgpu_render_extended *out,
                                 struct mxgpu_execution_binding *bindings, uint32_t binding_cap)
{
    struct mxgpu_render_header h;
    struct mxgpu_color_attachment color;
    struct mxgpu_viewport viewport;
    struct mxgpu_execution_binding binding;
    uint32_t i, j, cursor, vertex_offset, binding_offset, expected;
    int status;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len < 104)
        return MX_ERR_LENGTH;
    if (mx_r16(in, 30) || mx_r16(in, 70))
        return MX_ERR_RESERVED;
    render_header_read(in, &h);
    if (h.color_target_count > 8 || h.viewport_count > 16 || h.scissor_count > 16 ||
        h.vertex_buffer_count > 32)
        return MX_ERR_RANGE;
    expected = 104u + h.color_target_count * 32u + h.viewport_count * 24u + h.scissor_count * 16u +
               h.vertex_buffer_count * 24u + h.binding_count * 32u;
    if (len != expected || h.binding_count > binding_cap)
        return MX_ERR_LENGTH;
    if (h.binding_count && !bindings)
        return MX_ERR_BINDING;
    vertex_offset =
        104u + h.color_target_count * 32u + h.viewport_count * 24u + h.scissor_count * 16u;
    binding_offset = vertex_offset + h.vertex_buffer_count * 24u;
    for (i = 0; i < h.color_target_count; i++) {
        cursor = 104u + i * 32u;
        if (mx_r16(in, cursor + 10) || mx_r32(in, cursor + 28))
            return MX_ERR_RESERVED;
    }
    for (i = 0; i < h.vertex_buffer_count; i++) {
        cursor = vertex_offset + i * 24u;
        if (mx_r32(in, cursor + 4) || mx_r32(in, cursor + 12))
            return MX_ERR_RESERVED;
    }
    status = render_header_ok(&h);
    if (status != MX_OK)
        return status;
    cursor = 104;
    for (i = 0; i < h.color_target_count; i++, cursor += 32) {
        render_color_read(in + cursor, &color);
        status = render_color_ok(&color);
        if (status != MX_OK)
            return status;
        for (j = 0; j < i; j++)
            if (color.resource_id == mx_r32(in, 104u + j * 32u) &&
                color.mip_level == mx_r16(in, 108u + j * 32u))
                return MX_ERR_STATE;
    }
    for (i = 0; i < h.viewport_count; i++, cursor += 24) {
        render_viewport_read(in + cursor, &viewport);
        status = render_viewport_ok(&viewport);
        if (status != MX_OK)
            return status;
    }
    for (i = 0; i < h.scissor_count; i++, cursor += 16)
        if (mx_r32(in, cursor + 8) <= mx_r32(in, cursor) ||
            mx_r32(in, cursor + 12) <= mx_r32(in, cursor + 4))
            return MX_ERR_RANGE;
    for (i = 0; i < h.vertex_buffer_count; i++, cursor += 24)
        if (!mx_r32(in, cursor))
            return MX_ERR_RESOURCE;
    status = render_draw_ok(&h);
    if (status != MX_OK)
        return status;
    for (i = 0; i < h.binding_count; i++, cursor += 32) {
        status = mxgpu_binding_decode(in + cursor, 32, &binding);
        if (status != MX_OK)
            return status;
        for (j = 0; j < i; j++) {
            const uint8_t *previous = in + binding_offset + j * 32u;
            if (binding.slot == mx_r16(previous, 0) && binding.space == mx_r16(previous, 6))
                return MX_ERR_BINDING;
        }
    }
    {
        uintptr_t input = (uintptr_t)in, output = (uintptr_t)out;
        if ((output <= input && input - output < sizeof *out) ||
            (input < output && output - input < len)) {
            render_decode_alias(in, &h, out, bindings);
            return MX_OK;
        }
    }
    memset(out, 0, sizeof *out);
    render_header_store(&h, out);
    cursor = 104;
    for (i = 0; i < h.color_target_count; i++, cursor += 32)
        render_color_read(in + cursor, &out->color_targets[i]);
    for (i = 0; i < h.viewport_count; i++, cursor += 24)
        render_viewport_read(in + cursor, &out->viewports[i]);
    for (i = 0; i < h.scissor_count; i++, cursor += 16) {
        out->scissors[i].left = mx_r32(in, cursor);
        out->scissors[i].top = mx_r32(in, cursor + 4);
        out->scissors[i].right = mx_r32(in, cursor + 8);
        out->scissors[i].bottom = mx_r32(in, cursor + 12);
    }
    for (i = 0; i < h.vertex_buffer_count; i++, cursor += 24) {
        out->vertex_buffers[i].resource_id = mx_r32(in, cursor);
        out->vertex_buffers[i].stride = mx_r32(in, cursor + 8);
        out->vertex_buffers[i].offset = mx_r64(in, cursor + 16);
    }
    for (i = 0; i < h.binding_count; i++, cursor += 32)
        (void)mxgpu_binding_decode(in + cursor, 32, &bindings[i]);
    return MX_OK;
}

static uint32_t sampler_float_order(uint32_t bits)
{
    return bits & 0x80000000u ? ~bits : bits ^ 0x80000000u;
}

static int sampler_state_ok(const struct mxgpu_sampler_state *s)
{
    uint32_t i;
    if (!s || !s->sampler_id || s->min_filter < 1 || s->min_filter > 2 || s->mag_filter < 1 ||
        s->mag_filter > 2 || s->mip_filter < 1 || s->mip_filter > 2 || s->address_u < 1 ||
        s->address_u > 4 || s->address_v < 1 || s->address_v > 4 || s->address_w < 1 ||
        s->address_w > 4 || s->compare > 8 || !s->max_anisotropy || s->max_anisotropy > 16 ||
        !mxgpu_f32_finite(s->mip_lod_bias) || !mxgpu_f32_finite(s->min_lod) ||
        !mxgpu_f32_finite(s->max_lod))
        return MX_ERR_STATE;
    if ((s->min_lod & 0x7fffffffu) || (s->max_lod & 0x7fffffffu))
        if (sampler_float_order(s->min_lod) > sampler_float_order(s->max_lod))
            return MX_ERR_RANGE;
    for (i = 0; i < 4; i++)
        if (!mxgpu_f32_finite(s->border_color[i]))
            return MX_ERR_STATE;
    return MX_OK;
}

int mxgpu_sampler_state_encode(const struct mxgpu_sampler_state *s, uint8_t *out, uint32_t cap,
                               uint32_t *out_len)
{
    uint32_t i;
    int status = sampler_state_ok(s);
    if (status != MX_OK)
        return fail(out_len, status);
    if (!out || cap < MXGPU_SAMPLER_STATE_SIZE)
        return fail(out_len, MX_ERR_LENGTH);
    memset(out, 0, MXGPU_SAMPLER_STATE_SIZE);
    mx_w32(out, 0, s->sampler_id);
    out[4] = s->min_filter;
    out[5] = s->mag_filter;
    out[6] = s->mip_filter;
    out[7] = s->address_u;
    out[8] = s->address_v;
    out[9] = s->address_w;
    out[10] = s->compare != 0;
    out[11] = s->compare;
    mx_w16(out, 14, s->max_anisotropy);
    mx_w32(out, 16, s->mip_lod_bias);
    mx_w32(out, 20, s->min_lod);
    mx_w32(out, 24, s->max_lod);
    for (i = 0; i < 4; i++)
        mx_w32(out, 28 + 4 * i, s->border_color[i]);
    if (out_len)
        *out_len = MXGPU_SAMPLER_STATE_SIZE;
    return MX_OK;
}

int mxgpu_sampler_state_decode(const uint8_t *in, uint32_t len, struct mxgpu_sampler_state *out)
{
    struct mxgpu_sampler_state s;
    uint32_t i;
    int status;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len != MXGPU_SAMPLER_STATE_SIZE)
        return MX_ERR_LENGTH;
    if (mx_r16(in, 12) || mx_r32(in, 44))
        return MX_ERR_RESERVED;
    if (in[10] > 1 || (!in[10] && in[11]) || (in[10] && !in[11]))
        return MX_ERR_STATE;
    memset(&s, 0, sizeof s);
    s.sampler_id = mx_r32(in, 0);
    s.min_filter = in[4];
    s.mag_filter = in[5];
    s.mip_filter = in[6];
    s.address_u = in[7];
    s.address_v = in[8];
    s.address_w = in[9];
    s.compare = in[11];
    s.max_anisotropy = mx_r16(in, 14);
    s.mip_lod_bias = mx_r32(in, 16);
    s.min_lod = mx_r32(in, 20);
    s.max_lod = mx_r32(in, 24);
    for (i = 0; i < 4; i++)
        s.border_color[i] = mx_r32(in, 28 + 4 * i);
    status = sampler_state_ok(&s);
    if (status == MX_OK)
        *out = s;
    return status;
}

static int depth_stencil_state_ok(const struct mxgpu_depth_stencil_state *s)
{
    if (!s || !s->state_id || s->depth_test_enable > 1 || s->depth_write_enable > 1 ||
        s->stencil_enable > 1 || s->depth_compare < MXGPU_COMPARE_NEVER ||
        s->depth_compare > MXGPU_COMPARE_ALWAYS)
        return MX_ERR_STATE;
    const struct mxgpu_stencil_face *faces[2] = {&s->front, &s->back};
    for (unsigned i = 0; i < 2; i++) {
        const struct mxgpu_stencil_face *f = faces[i];
        if (f->fail < MXGPU_STENCIL_KEEP || f->fail > MXGPU_STENCIL_DECREMENT_WRAP ||
            f->depth_fail < MXGPU_STENCIL_KEEP || f->depth_fail > MXGPU_STENCIL_DECREMENT_WRAP ||
            f->pass < MXGPU_STENCIL_KEEP || f->pass > MXGPU_STENCIL_DECREMENT_WRAP ||
            f->compare < MXGPU_COMPARE_NEVER || f->compare > MXGPU_COMPARE_ALWAYS)
            return MX_ERR_STATE;
    }
    return MX_OK;
}

int mxgpu_depth_stencil_state_encode(const struct mxgpu_depth_stencil_state *in, uint8_t *out,
                                     uint32_t cap, uint32_t *out_len)
{
    uint8_t bytes[MXGPU_DEPTH_STENCIL_STATE_SIZE];
    int status = depth_stencil_state_ok(in);
    if (status != MX_OK)
        return fail(out_len, status);
    memset(bytes, 0, sizeof bytes);
    mx_w32(bytes, 0, in->state_id);
    bytes[4] = in->depth_test_enable;
    bytes[5] = in->depth_write_enable;
    bytes[6] = in->depth_compare;
    bytes[7] = in->stencil_enable;
    bytes[8] = in->stencil_read_mask;
    bytes[9] = in->stencil_write_mask;
    bytes[12] = in->front.fail;
    bytes[13] = in->front.depth_fail;
    bytes[14] = in->front.pass;
    bytes[15] = in->front.compare;
    bytes[16] = in->back.fail;
    bytes[17] = in->back.depth_fail;
    bytes[18] = in->back.pass;
    bytes[19] = in->back.compare;
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

int mxgpu_depth_stencil_state_decode(const uint8_t *in, uint32_t len,
                                     struct mxgpu_depth_stencil_state *out)
{
    struct mxgpu_depth_stencil_state s;
    int status;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len != MXGPU_DEPTH_STENCIL_STATE_SIZE)
        return MX_ERR_LENGTH;
    if (mx_r16(in, 10) || mx_r32(in, 20))
        return MX_ERR_RESERVED;
    memset(&s, 0, sizeof s);
    s.state_id = mx_r32(in, 0);
    s.depth_test_enable = in[4];
    s.depth_write_enable = in[5];
    s.depth_compare = in[6];
    s.stencil_enable = in[7];
    s.stencil_read_mask = in[8];
    s.stencil_write_mask = in[9];
    s.front.fail = in[12];
    s.front.depth_fail = in[13];
    s.front.pass = in[14];
    s.front.compare = in[15];
    s.back.fail = in[16];
    s.back.depth_fail = in[17];
    s.back.pass = in[18];
    s.back.compare = in[19];
    status = depth_stencil_state_ok(&s);
    if (status == MX_OK)
        *out = s;
    return status;
}

int mxgpu_depth_stencil_state_features(uint64_t features)
{
    return (features & MXGPU_FEAT_DEPTH_STENCIL_TARGET) ? MX_OK : MX_ERR_STATE;
}

static int adapter_info_ok(const struct mxgpu_adapter_info *in)
{
    uint32_t mask = 0;
    uint16_t format;
    if (!in)
        return MX_ERR_STATE;
    for (format = 1; format <= 32; format++)
        if (known_format(format))
            mask |= (uint32_t)1u << (format - 1u);
    if (!in->max_buffer_bytes || !in->max_texture_dimension_2d || !in->max_inline_transfer_bytes ||
        !in->pixel_format_mask || (in->pixel_format_mask & ~mask))
        return MX_ERR_RANGE;
    return MX_OK;
}

int mxgpu_adapter_info_encode(const struct mxgpu_adapter_info *in, uint8_t *out, uint32_t cap,
                              uint32_t *out_len)
{
    uint8_t bytes[MXGPU_ADAPTER_INFO_SIZE];
    int status = adapter_info_ok(in);
    if (status != MX_OK)
        return fail(out_len, status);
    mx_w64(bytes, 0, in->max_buffer_bytes);
    mx_w32(bytes, 8, in->max_texture_dimension_2d);
    mx_w32(bytes, 12, in->pixel_format_mask);
    mx_w32(bytes, 16, in->max_inline_transfer_bytes);
    mx_w32(bytes, 20, in->max_texture_array_layers);
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

int mxgpu_adapter_info_decode(const uint8_t *in, uint32_t len, struct mxgpu_adapter_info *out)
{
    struct mxgpu_adapter_info info;
    int status;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len != MXGPU_ADAPTER_INFO_SIZE)
        return MX_ERR_LENGTH;
    info.max_buffer_bytes = mx_r64(in, 0);
    info.max_texture_dimension_2d = mx_r32(in, 8);
    info.pixel_format_mask = mx_r32(in, 12);
    info.max_inline_transfer_bytes = mx_r32(in, 16);
    info.max_texture_array_layers = mx_r32(in, 20);
    status = adapter_info_ok(&info);
    if (status != MX_OK)
        return status;
    *out = info;
    return MX_OK;
}

static int format_capabilities_ok(const struct mxgpu_format_capabilities *c, uint32_t mask)
{
    const uint32_t depth = (1u << (MXGPU_FMT_DEPTH32_FLOAT - 1)) |
                           (1u << (MXGPU_FMT_DEPTH32_FLOAT_STENCIL8 - 1)) |
                           (1u << (MXGPU_FMT_DEPTH24_UNORM_STENCIL8 - 1));
    if (!c ||
        ((c->sampled | c->color_target | c->blendable | c->depth_stencil_target | c->storage |
          c->transfer_source | c->transfer_destination | c->scanout) &
         ~mask) ||
        ((c->color_target | c->blendable | c->storage | c->scanout) & depth))
        return MX_ERR_STATE;
    return MX_OK;
}

int mxgpu_format_capabilities_encode(const struct mxgpu_format_capabilities *in,
                                     uint32_t pixel_format_mask, uint8_t *out, uint32_t cap,
                                     uint32_t *out_len)
{
    uint8_t bytes[MXGPU_FORMAT_CAPABILITIES_SIZE];
    int status = format_capabilities_ok(in, pixel_format_mask);
    if (status != MX_OK)
        return fail(out_len, status);
    mx_w32(bytes, 0, in->sampled);
    mx_w32(bytes, 4, in->color_target);
    mx_w32(bytes, 8, in->blendable);
    mx_w32(bytes, 12, in->depth_stencil_target);
    mx_w32(bytes, 16, in->storage);
    mx_w32(bytes, 20, in->transfer_source);
    mx_w32(bytes, 24, in->transfer_destination);
    mx_w32(bytes, 28, in->scanout);
    return emit(out, cap, out_len, bytes, sizeof bytes);
}

int mxgpu_format_capabilities_decode(const uint8_t *in, uint32_t len, uint32_t pixel_format_mask,
                                     struct mxgpu_format_capabilities *out)
{
    struct mxgpu_format_capabilities c;
    int status;
    if (!in || !out)
        return MX_ERR_STATE;
    if (len != MXGPU_FORMAT_CAPABILITIES_SIZE)
        return MX_ERR_LENGTH;
    c.sampled = mx_r32(in, 0);
    c.color_target = mx_r32(in, 4);
    c.blendable = mx_r32(in, 8);
    c.depth_stencil_target = mx_r32(in, 12);
    c.storage = mx_r32(in, 16);
    c.transfer_source = mx_r32(in, 20);
    c.transfer_destination = mx_r32(in, 24);
    c.scanout = mx_r32(in, 28);
    status = format_capabilities_ok(&c, pixel_format_mask);
    if (status == MX_OK)
        *out = c;
    return status;
}

int mxgpu_format_capabilities_features(uint64_t features)
{
    return (features & MXGPU_FEAT_EXTENDED_PIXEL_FORMATS) ? MX_OK : MX_ERR_STATE;
}
