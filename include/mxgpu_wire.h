/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#ifndef MXGPU_WIRE_H
#define MXGPU_WIRE_H

#include <stddef.h>
#include <stdint.h>

#include "mx_versions.h"
#include "mxgpu_transport_abi.h"

#define MXGPU_NEGOTIATION_REQUEST_SIZE 32u
#define MXGPU_NEGOTIATION_RESPONSE_SIZE 64u
#define MXGPU_NEGOTIATION_RESPONSE_EXTENDED_SIZE 72u
#define MXGPU_NEGOTIATION_RESPONSE_TRANSFER_LIMIT_SIZE 80u
#define MXGPU_NEGOTIATION_RESPONSE_COMPUTE_LIMIT_SIZE 96u
#define MXGPU_COMMAND_HEADER_SIZE 32u
#define MXGPU_COMPLETION_SIZE 32u
#define MXGPU_RESOURCE_CREATE_SIZE 48u
#define MXGPU_RESOURCE_ID_SIZE 8u
#define MXGPU_RESOURCE_BIND_SIZE 32u
#define MXGPU_RESOURCE_PLACEMENT_INFO_SIZE 32u
#define MXGPU_TRANSFER_REQUEST_SIZE 48u
#define MXGPU_TIMELINE_WAIT_SIZE 16u
#define MXGPU_SHADER_CREATE_HEADER_SIZE 16u
#define MXGPU_PIPELINE_CREATE_SIZE 32u
#define MXGPU_EXECUTION_BINDING_SIZE 32u
#define MXGPU_RENDER_SUBMIT_HEADER_SIZE 72u
#define MXGPU_PRESENT_HEADER_SIZE 32u
#define MXGPU_PRESENT_DAMAGE_RECT_SIZE 16u
#define MXGPU_CURSOR_UPDATE_SIZE 32u
#define MXGPU_MAX_PRESENT_DAMAGE_RECTS 256u

#define MXGPU_FEAT_SCANOUT (1ull << 0)
#define MXGPU_FEAT_CURSOR (1ull << 1)
#define MXGPU_FEAT_RENDER (1ull << 2)
#define MXGPU_FEAT_COMPUTE (1ull << 3)
#define MXGPU_FEAT_TIMELINE_FENCES (1ull << 4)
#define MXGPU_FEAT_EXTERNAL_MEMORY (1ull << 5)
#define MXGPU_FEAT_MULTI_QUEUE (1ull << 6)
#define MXGPU_FEAT_RESOURCE_COHERENCY (1ull << 7)
#define MXGPU_FEAT_DEVICE_RESET (1ull << 8)
#define MXGPU_FEAT_FENCE_SIGNAL (1ull << 9)
#define MXGPU_FEAT_RENDER_TARGET_MIP (1ull << 10)
#define MXGPU_FEAT_RESOURCE_RESIDENCY (1ull << 11)
#define MXGPU_FEAT_RESOURCE_PLACEMENT (1ull << 12)
#define MXGPU_FEAT_PAGING_QUEUE (1ull << 13)
#define MXGPU_FEAT_COLOR_CLEAR (1ull << 14)
#define MXGPU_FEAT_RESOURCE_COPY (1ull << 15)
#define MXGPU_FEAT_DEPTH_STENCIL_TARGET (1ull << 16)
#define MXGPU_FEAT_MULTIPLE_RENDER_TARGETS (1ull << 17)
#define MXGPU_FEAT_VIEWPORT_SCISSOR (1ull << 18)
#define MXGPU_FEAT_BLEND_STATE (1ull << 19)
#define MXGPU_FEAT_RASTERIZER_STATE (1ull << 20)
#define MXGPU_FEAT_SAMPLER_OBJECTS (1ull << 21)
#define MXGPU_FEAT_QUERY_OBJECTS (1ull << 22)
#define MXGPU_FEAT_VERTEX_INPUT_LAYOUT (1ull << 23)
#define MXGPU_FEAT_GEOMETRY_SHADER (1ull << 24)
#define MXGPU_FEAT_TESSELLATION (1ull << 25)
#define MXGPU_FEAT_STREAM_OUTPUT (1ull << 26)
#define MXGPU_FEAT_TEXTURE_VIEW_LEVELS (1ull << 27)
#define MXGPU_FEAT_VIEWPORT_Y_FLIP (1ull << 28)
#define MXGPU_FEAT_EXTENDED_PIXEL_FORMATS (1ull << 29)
#define MXGPU_FEAT_TEXTURE_COMPARE (1ull << 30)
#define MXGPU_FEAT_FLOAT_BUFFER_FORMATS (1ull << 31)
#define MXGPU_FEAT_DEPTH24_STENCIL8 (1ull << 32)
#define MXGPU_FEAT_PRIMITIVE_RESTART (1ull << 33)
#define MXGPU_FEAT_TEXTURE_ARRAY (1ull << 34)
#define MXGPU_FEAT_GPU_MMU (1ull << 35)
#define MXGPU_FEAT_VBLANK (1ull << 36)
#define MXGPU_FEAT_QUEUE_RESET (1ull << 37)
#define MXGPU_FEAT_SCANOUT_APERTURE (1ull << 38)
#define MXGPU_FEAT_INTEGER_PIXEL_FORMATS (1ull << 39)
#define MXGPU_FEAT_RASTER_ORDER_GROUPS (1ull << 40)
#define MXGPU_FEAT_BUFFER_FILL (1ull << 41)
#define MXGPU_FEAT_LARGE_READBACK (1ull << 42)
#define MXGPU_FEAT_RESOURCE_ARRAYS (1ull << 43)
#define MXGPU_FEAT_KNOWN ((1ull << 44) - 1ull)
#define MXGPU_FEAT_RESOURCE_PAGING (MXGPU_FEAT_RESOURCE_RESIDENCY | MXGPU_FEAT_PAGING_QUEUE)

#define MXGPU_HOST_EVENT_SCANOUT_MODE_REQUEST (1ull << 0)
#define MXGPU_HOST_EVENT_KNOWN MXGPU_HOST_EVENT_SCANOUT_MODE_REQUEST

#define MXGPU_PLACEMENT_APERTURE_BASE 0x0000000100000000ull
#define MXGPU_PLACEMENT_APERTURE_SIZE 0x0000010000000000ull
#define MXGPU_PLACEMENT_PAGE_SIZE 0x1000ull
#define MXGPU_PLACEMENT_FLAG_BUFFER_SUBRANGE (1u << 0)
#define MXGPU_PLACEMENT_FLAG_NONBUFFER_FULL (1u << 1)
#define MXGPU_PLACEMENT_FLAG_PERSISTS_EVICTION (1u << 2)
#define MXGPU_PLACEMENT_FLAG_SINGLE_BINDING (1u << 3)
#define MXGPU_PLACEMENT_FLAGS_KNOWN                                                                \
    (MXGPU_PLACEMENT_FLAG_BUFFER_SUBRANGE | MXGPU_PLACEMENT_FLAG_NONBUFFER_FULL |                  \
     MXGPU_PLACEMENT_FLAG_PERSISTS_EVICTION | MXGPU_PLACEMENT_FLAG_SINGLE_BINDING)

#define MXGPU_OP_QUERY_ADAPTER 0x0001u
#define MXGPU_OP_QUERY_PLACEMENT 0x0002u
#define MXGPU_OP_CONTEXT_CREATE 0x0010u
#define MXGPU_OP_CONTEXT_DESTROY 0x0011u
#define MXGPU_OP_RESOURCE_CREATE 0x0020u
#define MXGPU_OP_RESOURCE_DESTROY 0x0021u
#define MXGPU_OP_RESOURCE_BIND 0x0024u
#define MXGPU_OP_RESOURCE_UNBIND 0x0025u
#define MXGPU_OP_SHADER_CREATE 0x0030u
#define MXGPU_OP_SHADER_DESTROY 0x0031u
#define MXGPU_OP_PIPELINE_CREATE 0x0040u
#define MXGPU_OP_PIPELINE_DESTROY 0x0041u
#define MXGPU_OP_RENDER_SUBMIT 0x0100u
#define MXGPU_OP_PRESENT 0x0101u
#define MXGPU_OP_COLOR_CLEAR 0x0102u
#define MXGPU_OP_TRANSFER_TO_HOST 0x0300u
#define MXGPU_OP_TRANSFER_FROM_HOST 0x0301u
#define MXGPU_OP_CURSOR_UPDATE 0x0400u
#define MXGPU_OP_TIMELINE_WAIT 0x0500u

#define MXGPU_CMD_SIGNAL_FENCE (1u << 0)
#define MXGPU_CMD_RESPONSE_REQUIRED (1u << 1)
#define MXGPU_CMD_FLAGS_KNOWN (MXGPU_CMD_SIGNAL_FENCE | MXGPU_CMD_RESPONSE_REQUIRED)

#define MXGPU_KIND_BUFFER 1u
#define MXGPU_KIND_TEXTURE_1D 2u
#define MXGPU_KIND_TEXTURE_2D 3u
#define MXGPU_KIND_TEXTURE_3D 4u

#define MXGPU_FMT_R8_UNORM 1u
#define MXGPU_FMT_RGBA8_UNORM 2u
#define MXGPU_FMT_BGRA8_UNORM 3u
#define MXGPU_FMT_RGBA16_FLOAT 4u
#define MXGPU_FMT_DEPTH32_FLOAT 13u
#define MXGPU_FMT_DEPTH32_FLOAT_STENCIL8 14u
#define MXGPU_FMT_DEPTH24_UNORM_STENCIL8 19u

#define MXGPU_USAGE_TRANSFER_SOURCE (1ull << 0)
#define MXGPU_USAGE_TRANSFER_DESTINATION (1ull << 1)
#define MXGPU_USAGE_VERTEX (1ull << 2)
#define MXGPU_USAGE_INDEX (1ull << 3)
#define MXGPU_USAGE_UNIFORM (1ull << 4)
#define MXGPU_USAGE_STORAGE (1ull << 5)
#define MXGPU_USAGE_SAMPLED (1ull << 6)
#define MXGPU_USAGE_COLOR_TARGET (1ull << 7)
#define MXGPU_USAGE_DEPTH_STENCIL (1ull << 8)
#define MXGPU_USAGE_SCANOUT (1ull << 9)
#define MXGPU_USAGE_CURSOR (1ull << 10)
#define MXGPU_USAGE_KNOWN ((1ull << 11) - 1ull)

#define MXGPU_PIPELINE_COMPUTE 1u
#define MXGPU_PIPELINE_RENDER 2u
#define MXGPU_BIND_ACCESS_READ 1u
#define MXGPU_BIND_ACCESS_WRITE 2u
#define MXGPU_BIND_ACCESS_READ_WRITE 3u
#define MXGPU_BIND_KIND_BUFFER 1u
#define MXGPU_BIND_KIND_TEXTURE_2D 2u
#define MXGPU_BIND_KIND_SAMPLER 3u
#define MXGPU_BIND_KIND_TEXTURE_CUBE 7u

#define MXGPU_LOAD_LOAD 1u
#define MXGPU_LOAD_CLEAR 2u
#define MXGPU_LOAD_DONT_CARE 3u
#define MXGPU_STORE_STORE 1u
#define MXGPU_STORE_DONT_CARE 2u
#define MXGPU_DRAW_NON_INDEXED 1u
#define MXGPU_DRAW_INDEXED 2u
#define MXGPU_PRIM_TRIANGLE 4u
#define MXGPU_DRAW_FLAG_PRIMITIVE_RESTART (1u << 0)

#define MXGPU_COMPLETION_SUCCESS 0u
#define MXGPU_COMPLETION_INVALID_COMMAND 1u
#define MXGPU_COMPLETION_INVALID_PARAMETER 4u
#define MXGPU_COMPLETION_OUT_OF_MEMORY 5u

enum mx_status {
    MX_OK = 0,
    MX_ERR_MAGIC = 1,
    MX_ERR_LENGTH = 2,
    MX_ERR_RESERVED = 3,
    MX_ERR_VERSION = 4,
    MX_ERR_FEATURE = 5,
    MX_ERR_OPCODE = 6,
    MX_ERR_QUEUE = 7,
    MX_ERR_CONTEXT = 8,
    MX_ERR_SEQUENCE = 9,
    MX_ERR_FENCE = 10,
    MX_ERR_FLAGS = 11,
    MX_ERR_RESOURCE = 12,
    MX_ERR_BINDING = 13,
    MX_ERR_SHAPE = 14,
    MX_ERR_RANGE = 15,
    MX_ERR_SHADER = 16,
    MX_ERR_LIMIT = 17,
    MX_ERR_STATE = 18,
    MX_ERR_FORMAT = 19,
    MX_ERR_USAGE = 20
};

struct mxgpu_negotiation_request {
    uint16_t minimum_major;
    uint16_t minimum_minor;
    uint16_t maximum_major;
    uint16_t maximum_minor;
    uint64_t requested_features;
    uint64_t required_features;
};

struct mxgpu_limits {
    uint16_t max_queues;
    uint16_t max_scanouts;
    uint32_t max_contexts;
    uint32_t max_resources;
    uint32_t max_descriptors_per_queue;
    uint32_t max_command_bytes;
    uint32_t max_inline_bytes;
    uint32_t max_transfer_to_host_bytes;
    uint32_t max_transfer_from_host_bytes;
    uint64_t max_resource_bytes;
    uint64_t max_resident_bytes;
    uint32_t max_compute_work_group_size[3];
    uint32_t max_compute_work_group_invocations;
};

struct mxgpu_negotiated {
    uint16_t major;
    uint16_t minor;
    uint32_t status;
    uint32_t response_size;
    uint64_t features;
    uint64_t host_event_features;
    struct mxgpu_limits limits;
};

struct mxgpu_resource_create {
    uint32_t resource_id;
    uint16_t kind;
    uint16_t format;
    uint64_t usage;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t array_layers;
    uint16_t mip_levels;
    uint16_t sample_count;
    uint64_t byte_size;
};

struct mxgpu_resource_bind {
    uint32_t resource_id;
    uint64_t gpu_va;
    uint64_t resource_offset;
    uint64_t byte_size;
};

struct mxgpu_command_header {
    uint16_t opcode;
    uint16_t flags;
    uint32_t context_id;
    uint16_t queue;
    uint64_t sequence;
    uint64_t fence_value;
};

struct mxgpu_completion {
    uint64_t sequence;
    uint32_t status;
    uint32_t response_bytes;
    uint64_t completed_fence;
    uint32_t device_generation;
};

struct mxgpu_descriptor {
    uint64_t address;
    uint32_t byte_len;
    uint16_t flags;
    uint16_t next;
};

struct mxgpu_available_entry {
    uint16_t descriptor_head;
};

struct mxgpu_used_entry {
    uint16_t descriptor_head;
    uint16_t queue;
    uint32_t written_bytes;
    uint32_t device_generation;
};

struct mxgpu_transfer {
    uint32_t resource_id;
    uint16_t mip_level;
    uint16_t array_layer;
    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint64_t resource_offset;
    uint32_t row_bytes;
    uint32_t data_bytes;
};

struct mxgpu_execution_binding {
    uint16_t slot;
    uint16_t space;
    uint16_t access;
    uint16_t kind;
    uint32_t resource_id;
    uint16_t base_level;
    uint16_t level_count;
    uint32_t base_layer;
    uint32_t layer_count;
    uint64_t offset;
    uint64_t size;
};

struct mxgpu_render_submit {
    uint32_t pipeline_id;
    uint32_t color_target_id;
    uint16_t binding_count;
    uint8_t load_action;
    uint8_t store_action;
    uint8_t draw_kind;
    uint8_t primitive;
    uint8_t index_type;
    uint8_t draw_flags;
    uint32_t clear_rgba[4];
    uint32_t element_start;
    uint32_t element_count;
    uint32_t instance_count;
    int32_t base_vertex;
    uint32_t base_instance;
    uint32_t index_resource_id;
    uint64_t index_buffer_offset;
    uint16_t color_target_mip_level;
};

#define MXGPU_OP_BLEND_STATE_CREATE 0x0060u
#define MXGPU_OP_BLEND_STATE_DESTROY 0x0061u
#define MXGPU_OP_RASTERIZER_STATE_CREATE 0x0070u
#define MXGPU_OP_RASTERIZER_STATE_DESTROY 0x0071u
#define MXGPU_OP_RENDER_SUBMIT_EXTENDED 0x0103u
#define MXGPU_BLEND_ZERO 1u
#define MXGPU_BLEND_ONE 2u
#define MXGPU_BLEND_SRC_COLOR 3u
#define MXGPU_BLEND_INV_SRC_COLOR 4u
#define MXGPU_BLEND_SRC_ALPHA 5u
#define MXGPU_BLEND_INV_SRC_ALPHA 6u
#define MXGPU_BLEND_DST_ALPHA 7u
#define MXGPU_BLEND_INV_DST_ALPHA 8u
#define MXGPU_BLEND_DST_COLOR 9u
#define MXGPU_BLEND_INV_DST_COLOR 10u
#define MXGPU_BLEND_SRC_ALPHA_SATURATE 11u
#define MXGPU_BLEND_CONSTANT_COLOR 12u
#define MXGPU_BLEND_INV_CONSTANT_COLOR 13u
#define MXGPU_BLEND_CONSTANT_ALPHA 14u
#define MXGPU_BLEND_INV_CONSTANT_ALPHA 15u
#define MXGPU_BLEND_ADD 1u
#define MXGPU_BLEND_SUBTRACT 2u
#define MXGPU_BLEND_REVERSE_SUBTRACT 3u
#define MXGPU_BLEND_MIN 4u
#define MXGPU_BLEND_MAX 5u
#define MXGPU_FILL_SOLID 1u
#define MXGPU_FILL_WIREFRAME 2u
#define MXGPU_CULL_NONE 1u
#define MXGPU_CULL_FRONT 2u
#define MXGPU_CULL_BACK 3u
#define MXGPU_FRONT_CLOCKWISE 1u
#define MXGPU_FRONT_COUNTERCLOCKWISE 2u
#define MXGPU_BLEND_STATE_HEADER_SIZE 8u
#define MXGPU_BLEND_TARGET_SIZE 16u
#define MXGPU_RASTERIZER_STATE_SIZE 24u
#define MXGPU_RENDER_EXTENDED_HEADER_SIZE 104u
#define MXGPU_COLOR_ATTACHMENT_SIZE 32u
#define MXGPU_VIEWPORT_SIZE 24u
#define MXGPU_SCISSOR_SIZE 16u
#define MXGPU_VERTEX_BUFFER_BINDING_SIZE 24u

#define MXGPU_ADAPTER_INFO_SIZE 24u
struct mxgpu_adapter_info {
    uint64_t max_buffer_bytes;
    uint32_t max_texture_dimension_2d, pixel_format_mask;
    uint32_t max_inline_transfer_bytes, max_texture_array_layers;
};
int mxgpu_adapter_info_encode(const struct mxgpu_adapter_info *in, uint8_t *out, uint32_t cap,
                              uint32_t *out_len);
int mxgpu_adapter_info_decode(const uint8_t *in, uint32_t len, struct mxgpu_adapter_info *out);

#define MXGPU_OP_QUERY_FORMAT_CAPABILITIES 0x0003u
#define MXGPU_FORMAT_CAPABILITIES_SIZE 32u
struct mxgpu_format_capabilities {
    uint32_t sampled, color_target, blendable, depth_stencil_target;
    uint32_t storage, transfer_source, transfer_destination, scanout;
};
int mxgpu_format_capabilities_encode(const struct mxgpu_format_capabilities *in,
                                     uint32_t pixel_format_mask, uint8_t *out, uint32_t cap,
                                     uint32_t *out_len);
int mxgpu_format_capabilities_decode(const uint8_t *in, uint32_t len, uint32_t pixel_format_mask,
                                     struct mxgpu_format_capabilities *out);
int mxgpu_format_capabilities_features(uint64_t features);

#define MXGPU_OP_DEPTH_STENCIL_STATE_CREATE 0x0050u
#define MXGPU_OP_DEPTH_STENCIL_STATE_DESTROY 0x0051u
#define MXGPU_DEPTH_STENCIL_STATE_SIZE 24u
#define MXGPU_COMPARE_NEVER 1u
#define MXGPU_COMPARE_LESS 2u
#define MXGPU_COMPARE_EQUAL 3u
#define MXGPU_COMPARE_LESS_EQUAL 4u
#define MXGPU_COMPARE_GREATER 5u
#define MXGPU_COMPARE_NOT_EQUAL 6u
#define MXGPU_COMPARE_GREATER_EQUAL 7u
#define MXGPU_COMPARE_ALWAYS 8u
#define MXGPU_STENCIL_KEEP 1u
#define MXGPU_STENCIL_ZERO 2u
#define MXGPU_STENCIL_REPLACE 3u
#define MXGPU_STENCIL_INCREMENT_CLAMP 4u
#define MXGPU_STENCIL_DECREMENT_CLAMP 5u
#define MXGPU_STENCIL_INVERT 6u
#define MXGPU_STENCIL_INCREMENT_WRAP 7u
#define MXGPU_STENCIL_DECREMENT_WRAP 8u
struct mxgpu_stencil_face {
    uint8_t fail, depth_fail, pass, compare;
};
struct mxgpu_depth_stencil_state {
    uint32_t state_id;
    uint8_t depth_test_enable, depth_write_enable, depth_compare, stencil_enable;
    uint8_t stencil_read_mask, stencil_write_mask;
    struct mxgpu_stencil_face front, back;
};
int mxgpu_depth_stencil_state_encode(const struct mxgpu_depth_stencil_state *in, uint8_t *out,
                                     uint32_t cap, uint32_t *out_len);
int mxgpu_depth_stencil_state_decode(const uint8_t *in, uint32_t len,
                                     struct mxgpu_depth_stencil_state *out);
int mxgpu_depth_stencil_state_features(uint64_t features);

#define MXGPU_OP_SAMPLER_CREATE 0x0080u
#define MXGPU_OP_SAMPLER_DESTROY 0x0081u
#define MXGPU_SAMPLER_STATE_SIZE 48u
#define MXGPU_FILTER_NEAREST 1u
#define MXGPU_FILTER_LINEAR 2u
#define MXGPU_ADDRESS_REPEAT 1u
#define MXGPU_ADDRESS_MIRRORED_REPEAT 2u
#define MXGPU_ADDRESS_CLAMP_TO_EDGE 3u
#define MXGPU_ADDRESS_CLAMP_TO_BORDER 4u
struct mxgpu_sampler_state {
    uint32_t sampler_id;
    uint8_t min_filter, mag_filter, mip_filter;
    uint8_t address_u, address_v, address_w, compare;
    uint16_t max_anisotropy;
    uint32_t mip_lod_bias, min_lod, max_lod, border_color[4];
};
int mxgpu_sampler_state_encode(const struct mxgpu_sampler_state *in, uint8_t *out, uint32_t cap,
                               uint32_t *out_len);
int mxgpu_sampler_state_decode(const uint8_t *in, uint32_t len, struct mxgpu_sampler_state *out);

struct mxgpu_blend_target {
    uint8_t enable, write_mask, src_color, dst_color, color_op;
    uint8_t src_alpha, dst_alpha, alpha_op;
};
struct mxgpu_blend_state {
    uint32_t state_id;
    uint8_t target_count;
    struct mxgpu_blend_target targets[8];
};
struct mxgpu_rasterizer_state {
    uint32_t state_id;
    uint8_t fill_mode, cull_mode, front_face, depth_clip_enable;
    uint8_t scissor_enable, multisample_enable, antialiased_line_enable;
    int32_t depth_bias;
    uint32_t depth_bias_clamp, slope_scaled_depth_bias;
};
struct mxgpu_color_attachment {
    uint32_t resource_id;
    uint16_t mip_level, format;
    uint8_t load_action, store_action;
    uint32_t clear_rgba[4];
};
struct mxgpu_viewport {
    uint32_t x, y, width, height, min_depth, max_depth;
};
struct mxgpu_scissor {
    uint32_t left, top, right, bottom;
};
struct mxgpu_vertex_buffer_binding {
    uint32_t resource_id, stride;
    uint64_t offset;
};
struct mxgpu_render_extended {
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
    struct mxgpu_color_attachment color_targets[8];
    struct mxgpu_viewport viewports[16];
    struct mxgpu_scissor scissors[16];
    struct mxgpu_vertex_buffer_binding vertex_buffers[32];
};
int mxgpu_blend_state_encode(const struct mxgpu_blend_state *in, uint8_t *out, uint32_t cap,
                             uint32_t *out_len);
int mxgpu_blend_state_decode(const uint8_t *in, uint32_t len, struct mxgpu_blend_state *out);
int mxgpu_rasterizer_state_encode(const struct mxgpu_rasterizer_state *in, uint8_t *out,
                                  uint32_t cap, uint32_t *out_len);
int mxgpu_rasterizer_state_decode(const uint8_t *in, uint32_t len,
                                  struct mxgpu_rasterizer_state *out);
int mxgpu_render_extended_encode(const struct mxgpu_render_extended *in,
                                 const struct mxgpu_execution_binding *bindings, uint8_t *out,
                                 uint32_t cap, uint32_t *out_len);
int mxgpu_render_extended_decode(const uint8_t *in, uint32_t len, struct mxgpu_render_extended *out,
                                 struct mxgpu_execution_binding *bindings, uint32_t binding_cap);
int mxgpu_render_extended_features(const struct mxgpu_render_extended *in, uint64_t features);

struct mxgpu_present_rect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

struct mxgpu_present {
    uint32_t resource_id;
    uint16_t scanout_id;
    uint16_t flags;
    struct mxgpu_present_rect source;
    uint32_t damage_count;
    const struct mxgpu_present_rect *damage;
};

#define MXGPU_CURSOR_HIDE (1u << 0)
#define MXGPU_CURSOR_MOVE_ONLY (1u << 1)
#define MXGPU_CURSOR_FLAGS_KNOWN (MXGPU_CURSOR_HIDE | MXGPU_CURSOR_MOVE_ONLY)

struct mxgpu_cursor_update {
    uint32_t resource_id;
    uint16_t scanout_id;
    uint16_t flags;
    int32_t x;
    int32_t y;
    uint32_t hot_x;
    uint32_t hot_y;
};

int mxgpu_negotiation_request_encode(const struct mxgpu_negotiation_request *in, uint8_t *out,
                                     uint32_t cap, uint32_t *out_len);
int mxgpu_negotiation_request_decode(const uint8_t *in, uint32_t len,
                                     struct mxgpu_negotiation_request *out);
int mxgpu_negotiation_response_encode(const struct mxgpu_negotiated *in, uint8_t *out, uint32_t cap,
                                      uint32_t *out_len);
int mxgpu_negotiation_response_decode(const uint8_t *in, uint32_t len,
                                      struct mxgpu_negotiated *out);
int mxgpu_resource_create_encode(const struct mxgpu_resource_create *in,
                                 uint64_t max_resource_bytes, uint8_t *out, uint32_t cap,
                                 uint32_t *out_len);
int mxgpu_resource_create_decode(const uint8_t *in, uint32_t len, uint64_t max_resource_bytes,
                                 struct mxgpu_resource_create *out);
int mxgpu_resource_bind_encode(const struct mxgpu_resource_bind *in, uint8_t *out, uint32_t cap,
                               uint32_t *out_len);
int mxgpu_resource_bind_decode(const uint8_t *in, uint32_t len, struct mxgpu_resource_bind *out);
int mxgpu_resource_id_encode(uint32_t resource_id, uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxgpu_resource_id_decode(const uint8_t *in, uint32_t len, uint32_t *resource_id);
int mxgpu_command_encode(const struct mxgpu_command_header *header, const uint8_t *payload,
                         uint32_t payload_len, uint32_t max_command_bytes, uint8_t *out,
                         uint32_t cap, uint32_t *out_len);
int mxgpu_command_decode(const uint8_t *in, uint32_t len, uint32_t max_command_bytes,
                         struct mxgpu_command_header *header, const uint8_t **payload,
                         uint32_t *payload_len);
int mxgpu_completion_encode(const struct mxgpu_completion *in, uint8_t *out, uint32_t cap,
                            uint32_t *out_len);
int mxgpu_completion_decode(const uint8_t *in, uint32_t len, struct mxgpu_completion *out);
int mxgpu_descriptor_encode(const struct mxgpu_descriptor *in, uint8_t *out, uint32_t cap,
                            uint32_t *out_len);
int mxgpu_descriptor_decode(const uint8_t *in, uint32_t len, struct mxgpu_descriptor *out);
int mxgpu_available_encode(const struct mxgpu_available_entry *in, uint8_t *out, uint32_t cap,
                           uint32_t *out_len);
int mxgpu_available_decode(const uint8_t *in, uint32_t len, struct mxgpu_available_entry *out);
int mxgpu_used_encode(const struct mxgpu_used_entry *in, uint8_t *out, uint32_t cap,
                      uint32_t *out_len);
int mxgpu_used_decode(const uint8_t *in, uint32_t len, struct mxgpu_used_entry *out);
int mxgpu_transfer_encode(const struct mxgpu_transfer *in, const uint8_t *data, uint8_t *out,
                          uint32_t cap, uint32_t *out_len);
int mxgpu_transfer_decode(const uint8_t *in, uint32_t len, struct mxgpu_transfer *out,
                          const uint8_t **data);
int mxgpu_binding_encode(const struct mxgpu_execution_binding *in, uint8_t *out, uint32_t cap,
                         uint32_t *out_len);
int mxgpu_binding_decode(const uint8_t *in, uint32_t len, struct mxgpu_execution_binding *out);
int mxgpu_shader_create_encode(uint32_t shader_id, const uint8_t *bytecode, uint32_t bytecode_len,
                               uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxgpu_pipeline_create_encode(uint32_t pipeline_id, uint16_t kind, uint16_t color_format,
                                 uint32_t shader_id, uint32_t first_entry, uint32_t second_entry,
                                 uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxgpu_render_submit_encode(const struct mxgpu_render_submit *in,
                               const struct mxgpu_execution_binding *bindings, uint8_t *out,
                               uint32_t cap, uint32_t *out_len);
int mxgpu_render_submit_decode(const uint8_t *in, uint32_t len, struct mxgpu_render_submit *out,
                               struct mxgpu_execution_binding *bindings, uint32_t binding_cap);
int mxgpu_present_encode(const struct mxgpu_present *in, uint8_t *out, uint32_t cap,
                         uint32_t *out_len);
int mxgpu_present_decode(const uint8_t *in, uint32_t len, struct mxgpu_present *out,
                         struct mxgpu_present_rect *damage, uint32_t damage_cap);
int mxgpu_cursor_update_encode(const struct mxgpu_cursor_update *in, uint8_t *out, uint32_t cap,
                               uint32_t *out_len);
int mxgpu_cursor_update_decode(const uint8_t *in, uint32_t len, struct mxgpu_cursor_update *out);
int mxgpu_opcode_queue(uint16_t opcode, uint16_t *queue);
uint32_t mxgpu_format_bytes_per_pixel(uint16_t format);
int mxgpu_f32_finite(uint32_t bits);

#endif
