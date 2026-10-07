/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxgpu_vertex.h"
#include "mx_le.h"
#include <string.h>

static int span_ok(const void *address, size_t bytes)
{
    return address && (uintptr_t)address <= UINTPTR_MAX-bytes;
}
static int overlap(const void *a, size_t an, const void *b, size_t bn)
{
    uintptr_t av=(uintptr_t)a,bv=(uintptr_t)b;
    if (!an || !bn) return 0;
    return av <= bv ? bv-av < an : av-bv < bn;
}
static int element_valid(const struct mxgpu_vertex_input_element *element)
{
    if (element->format < MXGPU_VERTEX_FORMAT_FLOAT32 || element->format > MXGPU_VERTEX_FORMAT_FLOAT16X4)
        return MX_ERR_RANGE;
    if (element->step_mode != MXGPU_VERTEX_STEP_PER_VERTEX && element->step_mode != MXGPU_VERTEX_STEP_PER_INSTANCE)
        return MX_ERR_RANGE;
    return MX_OK;
}
static void read_element(const uint8_t *bytes, struct mxgpu_vertex_input_element *element)
{
    memset(element,0,sizeof(*element));
    element->semantic_index=mx_r32(bytes,0);
    element->format=mx_r16(bytes,4); element->input_slot=mx_r16(bytes,6);
    element->byte_offset=mx_r32(bytes,8); element->step_mode=bytes[12];
}
int mxgpu_vertex_layout_features(uint64_t features)
{
    return (features & MXGPU_FEAT_VERTEX_INPUT_LAYOUT) ? MX_OK : MX_ERR_FEATURE;
}
int mxgpu_vertex_layout_encode(const struct mxgpu_vertex_layout *layout,
    const struct mxgpu_vertex_input_element *elements, uint8_t *out,
    uint32_t capacity, uint32_t *written)
{
    uint32_t i,j,bytes,count;
    if (!span_ok(layout,sizeof(*layout)) || !out || !written ||
        !span_ok(out,capacity) || !span_ok(written,sizeof(*written)) ||
        overlap(out,capacity,layout,sizeof(*layout)) || overlap(written,sizeof(*written),layout,sizeof(*layout)) ||
        overlap(out,capacity,written,sizeof(*written))) return MX_ERR_STATE;
    count=layout->element_count;
    if (!count || count > MXGPU_MAX_VERTEX_ELEMENTS || !layout->layout_id) {
        *written=0; return MX_ERR_RANGE;
    }
    if (!span_ok(elements,(size_t)count*sizeof(*elements)) ||
        overlap(out,capacity,elements,(size_t)count*sizeof(*elements)) ||
        overlap(written,sizeof(*written),elements,(size_t)count*sizeof(*elements))) return MX_ERR_STATE;
    *written=0;
    for (i=0;i<count;++i) {
        int result=element_valid(elements+i);
        if (result != MX_OK) return result;
        for (j=0;j<i;++j) if (elements[i].semantic_index == elements[j].semantic_index) return MX_ERR_BINDING;
    }
    bytes=MXGPU_VERTEX_LAYOUT_CREATE_HEADER_SIZE+count*MXGPU_VERTEX_INPUT_ELEMENT_SIZE;
    if (capacity < bytes) return MX_ERR_LENGTH;
    memset(out,0,bytes); mx_w32(out,0,layout->layout_id); out[4]=(uint8_t)count;
    for (i=0;i<count;++i) {
        uint8_t *record=out+MXGPU_VERTEX_LAYOUT_CREATE_HEADER_SIZE+i*MXGPU_VERTEX_INPUT_ELEMENT_SIZE;
        mx_w32(record,0,elements[i].semantic_index); mx_w16(record,4,elements[i].format);
        mx_w16(record,6,elements[i].input_slot); mx_w32(record,8,elements[i].byte_offset);
        record[12]=elements[i].step_mode;
    }
    *written=bytes; return MX_OK;
}
int mxgpu_vertex_layout_decode(const uint8_t *in, uint32_t length,
    struct mxgpu_vertex_layout *layout, struct mxgpu_vertex_input_element *elements,
    uint32_t element_capacity)
{
    struct mxgpu_vertex_layout candidate;
    uint32_t i,j,count;
    if (!span_ok(in,length) || !span_ok(layout,sizeof(*layout))) return MX_ERR_STATE;
    if (length < MXGPU_VERTEX_LAYOUT_CREATE_HEADER_SIZE) return MX_ERR_LENGTH;
    candidate.layout_id=mx_r32(in,0); count=in[4]; candidate.element_count=count;
    if (!candidate.layout_id || !count) return MX_ERR_RANGE;
    if (in[5] || in[6] || in[7]) return MX_ERR_RESERVED;
    if (length != MXGPU_VERTEX_LAYOUT_CREATE_HEADER_SIZE+count*MXGPU_VERTEX_INPUT_ELEMENT_SIZE ||
        element_capacity < count) return MX_ERR_LENGTH;
    if (!span_ok(elements,(size_t)count*sizeof(*elements)) ||
        overlap(layout,sizeof(*layout),in,length) || overlap(elements,(size_t)count*sizeof(*elements),in,length) ||
        overlap(layout,sizeof(*layout),elements,(size_t)count*sizeof(*elements))) return MX_ERR_STATE;
    for (i=0;i<count;++i) {
        struct mxgpu_vertex_input_element element;
        const uint8_t *record=in+MXGPU_VERTEX_LAYOUT_CREATE_HEADER_SIZE+i*MXGPU_VERTEX_INPUT_ELEMENT_SIZE;
        int result;
        if (record[13] || record[14] || record[15]) return MX_ERR_RESERVED;
        read_element(record,&element); result=element_valid(&element);
        if (result != MX_OK) return result;
        for (j=0;j<i;++j)
            if (element.semantic_index == mx_r32(in,MXGPU_VERTEX_LAYOUT_CREATE_HEADER_SIZE+j*MXGPU_VERTEX_INPUT_ELEMENT_SIZE))
                return MX_ERR_BINDING;
    }
    for (i=0;i<count;++i) read_element(in+MXGPU_VERTEX_LAYOUT_CREATE_HEADER_SIZE+i*MXGPU_VERTEX_INPUT_ELEMENT_SIZE,elements+i);
    *layout=candidate; return MX_OK;
}
int mxgpu_vertex_layout_destroy_encode(uint32_t layout_id, uint8_t *out,
    uint32_t capacity, uint32_t *written)
{
    if (!span_ok(out,capacity) || !span_ok(written,sizeof(*written)) ||
        overlap(out,capacity,written,sizeof(*written))) return MX_ERR_STATE;
    return mxgpu_resource_id_encode(layout_id,out,capacity,written);
}
int mxgpu_vertex_layout_destroy_decode(const uint8_t *in, uint32_t length, uint32_t *layout_id)
{
    if (!span_ok(in,length) || !span_ok(layout_id,sizeof(*layout_id)) ||
        overlap(in,length,layout_id,sizeof(*layout_id))) return MX_ERR_STATE;
    return mxgpu_resource_id_decode(in,length,layout_id);
}
