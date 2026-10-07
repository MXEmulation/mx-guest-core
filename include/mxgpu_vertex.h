/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#ifndef MXGPU_VERTEX_H
#define MXGPU_VERTEX_H
#include "mxgpu_wire.h"

#define MXGPU_VERTEX_LAYOUT_CREATE_HEADER_SIZE 8u
#define MXGPU_VERTEX_INPUT_ELEMENT_SIZE 16u
#define MXGPU_MAX_VERTEX_ELEMENTS 255u
#define MXGPU_VERTEX_FORMAT_FLOAT32 1u
#define MXGPU_VERTEX_FORMAT_FLOAT32X2 2u
#define MXGPU_VERTEX_FORMAT_FLOAT32X3 3u
#define MXGPU_VERTEX_FORMAT_FLOAT32X4 4u
#define MXGPU_VERTEX_FORMAT_UINT32 5u
#define MXGPU_VERTEX_FORMAT_UINT32X2 6u
#define MXGPU_VERTEX_FORMAT_UINT32X3 7u
#define MXGPU_VERTEX_FORMAT_UINT32X4 8u
#define MXGPU_VERTEX_FORMAT_SINT32 9u
#define MXGPU_VERTEX_FORMAT_SINT32X2 10u
#define MXGPU_VERTEX_FORMAT_SINT32X3 11u
#define MXGPU_VERTEX_FORMAT_SINT32X4 12u
#define MXGPU_VERTEX_FORMAT_UNORM8X4 13u
#define MXGPU_VERTEX_FORMAT_SNORM8X4 14u
#define MXGPU_VERTEX_FORMAT_FLOAT16X2 15u
#define MXGPU_VERTEX_FORMAT_FLOAT16X4 16u
#define MXGPU_VERTEX_STEP_PER_VERTEX 1u
#define MXGPU_VERTEX_STEP_PER_INSTANCE 2u

struct mxgpu_vertex_input_element {
    uint32_t semantic_index;
    uint16_t format;
    uint16_t input_slot;
    uint32_t byte_offset;
    uint8_t step_mode;
};
struct mxgpu_vertex_layout {
    uint32_t layout_id;
    uint32_t element_count;
};

/* Backend element, buffer, stride and format limits are checked separately
 * against the backend's capabilities. This codec validates the wire contract.
 * All writable spans are distinct from inputs and each other. Malformed input
 * leaves layout, elements and payload unchanged. Span refusal preserves written;
 * other encode refusals set written to zero. */
int mxgpu_vertex_layout_features(uint64_t features);
int mxgpu_vertex_layout_encode(const struct mxgpu_vertex_layout *layout,
    const struct mxgpu_vertex_input_element *elements, uint8_t *out,
    uint32_t capacity, uint32_t *written);
int mxgpu_vertex_layout_decode(const uint8_t *in, uint32_t length,
    struct mxgpu_vertex_layout *layout, struct mxgpu_vertex_input_element *elements,
    uint32_t element_capacity);
int mxgpu_vertex_layout_destroy_encode(uint32_t layout_id, uint8_t *out,
    uint32_t capacity, uint32_t *written);
int mxgpu_vertex_layout_destroy_decode(const uint8_t *in, uint32_t length, uint32_t *layout_id);
#endif
