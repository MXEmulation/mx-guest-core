/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxgpu_vertex.h"
#include "mx_le.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    struct mxgpu_vertex_layout layout={7,MXGPU_MAX_VERTEX_ELEMENTS},decoded,saved;
    struct mxgpu_vertex_input_element elements[MXGPU_MAX_VERTEX_ELEMENTS],out[MXGPU_MAX_VERTEX_ELEMENTS],saved_elements[MXGPU_MAX_VERTEX_ELEMENTS];
    uint8_t bytes[MXGPU_VERTEX_LAYOUT_CREATE_HEADER_SIZE+MXGPU_MAX_VERTEX_ELEMENTS*MXGPU_VERTEX_INPUT_ELEMENT_SIZE],snapshot[sizeof(bytes)];
    uint8_t command[sizeof(bytes)+MXGPU_COMMAND_HEADER_SIZE];
    struct mxgpu_command_header header={0},decoded_header;
    const uint8_t *payload; uint32_t command_bytes,payload_bytes;
    uint32_t i,length,id; uint16_t queue;
    memset(elements,0,sizeof(elements));
    for (i=0;i<MXGPU_MAX_VERTEX_ELEMENTS;++i) {
        elements[i].semantic_index=i; elements[i].format=(uint16_t)(i%16u+1u);
        elements[i].input_slot=(uint16_t)(UINT16_MAX-i); elements[i].byte_offset=UINT32_MAX-i;
        elements[i].step_mode=(uint8_t)(i%2u+1u);
    }
    assert(mxgpu_vertex_layout_features(0) == MX_ERR_FEATURE);
    assert(mxgpu_vertex_layout_features(MXGPU_FEAT_VERTEX_INPUT_LAYOUT) == MX_OK);
    assert(mxgpu_opcode_queue(MXGPU_OP_VERTEX_LAYOUT_CREATE,&queue) == MX_OK && queue == MXGPU_QUEUE_CONTROL);
    assert(mxgpu_opcode_queue(MXGPU_OP_VERTEX_LAYOUT_DESTROY,&queue) == MX_OK && queue == MXGPU_QUEUE_CONTROL);
    assert(mxgpu_vertex_layout_encode(&layout,elements,bytes,sizeof(bytes),&length) == MX_OK && length == sizeof(bytes));
    assert(mx_r32(bytes,0) == 7 && bytes[4] == 255 && !bytes[5] && !bytes[6] && !bytes[7]);
    assert(mx_r32(bytes,8) == 0 && mx_r16(bytes,12) == 1 && mx_r16(bytes,14) == UINT16_MAX &&
        mx_r32(bytes,16) == UINT32_MAX && bytes[20] == 1 && !bytes[21] && !bytes[22] && !bytes[23]);
    assert(mxgpu_vertex_layout_decode(bytes,length,&decoded,out,MXGPU_MAX_VERTEX_ELEMENTS) == MX_OK);
    assert(!memcmp(&layout,&decoded,sizeof(layout)) && !memcmp(elements,out,sizeof(elements)));
    header.opcode=MXGPU_OP_VERTEX_LAYOUT_CREATE; header.queue=MXGPU_QUEUE_CONTROL;
    header.context_id=1; header.sequence=1;
    assert(mxgpu_command_encode(&header,bytes,length,sizeof(command),command,sizeof(command),&command_bytes) == MX_OK);
    assert(mxgpu_command_decode(command,command_bytes,sizeof(command),&decoded_header,&payload,&payload_bytes) == MX_OK);
    assert(decoded_header.opcode == MXGPU_OP_VERTEX_LAYOUT_CREATE && payload_bytes == length && !memcmp(payload,bytes,length));
    saved=decoded; memcpy(saved_elements,out,sizeof(out)); memcpy(snapshot,bytes,sizeof(bytes));
    assert(mxgpu_vertex_layout_decode(bytes,length,&decoded,out,254) == MX_ERR_LENGTH);
    for (i=0;i<length;++i) assert(mxgpu_vertex_layout_decode(bytes,i,&decoded,out,255) != MX_OK);
    bytes[23]=1; assert(mxgpu_vertex_layout_decode(bytes,length,&decoded,out,255) == MX_ERR_RESERVED); bytes[23]=0;
    bytes[5]=1; assert(mxgpu_vertex_layout_decode(bytes,length,&decoded,out,255) == MX_ERR_RESERVED); bytes[5]=0;
    mx_w32(bytes,24,0); assert(mxgpu_vertex_layout_decode(bytes,length,&decoded,out,255) == MX_ERR_BINDING);
    assert(!memcmp(&decoded,&saved,sizeof(saved)) && !memcmp(out,saved_elements,sizeof(out)));
    memcpy(bytes,snapshot,sizeof(bytes)); elements[1].semantic_index=0;
    assert(mxgpu_vertex_layout_encode(&layout,elements,bytes,sizeof(bytes),&length) == MX_ERR_BINDING && !length);
    assert(!memcmp(bytes,snapshot,sizeof(bytes))); elements[1].semantic_index=1;
    elements[0].format=17; assert(mxgpu_vertex_layout_encode(&layout,elements,bytes,sizeof(bytes),&length) == MX_ERR_RANGE);
    elements[0].format=1; elements[0].step_mode=0;
    assert(mxgpu_vertex_layout_encode(&layout,elements,bytes,sizeof(bytes),&length) == MX_ERR_RANGE);
    elements[0].step_mode=1;
    assert(mxgpu_vertex_layout_encode(&layout,elements,bytes,sizeof(bytes)-1,&length) == MX_ERR_LENGTH);
    assert(!memcmp(bytes,snapshot,sizeof(bytes)));
    length=123;
    assert(mxgpu_vertex_layout_encode(&layout,elements,bytes,sizeof(bytes),(uint32_t *)bytes) == MX_ERR_STATE);
    assert(!memcmp(bytes,snapshot,sizeof(bytes)));
    assert(mxgpu_vertex_layout_decode(bytes,sizeof(bytes),(struct mxgpu_vertex_layout *)bytes,out,255) == MX_ERR_STATE);
    assert(mxgpu_vertex_layout_destroy_encode(7,bytes,sizeof(bytes),&length) == MX_OK && length == 8);
    assert(mx_r32(bytes,0) == 7 && !mx_r32(bytes,4));
    assert(mxgpu_vertex_layout_destroy_decode(bytes,length,&id) == MX_OK && id == 7);
    bytes[4]=1; id=123;
    assert(mxgpu_vertex_layout_destroy_decode(bytes,length,&id) == MX_ERR_RESERVED && id == 123);
    assert(mxgpu_vertex_layout_destroy_encode(0,bytes,sizeof(bytes),&length) == MX_ERR_RESOURCE && !length);
    return 0;
}
