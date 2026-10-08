/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxsb.h"
#include "mx_le.h"
#include <assert.h>
#include <string.h>

static uint32_t words[128];
static uint8_t bytes[sizeof(words)];

static int module(uint16_t opcode, uint32_t type, uint32_t location,
    uint32_t interpolation, uint32_t stage, uint16_t minor, int duplicate, uint32_t root)
{
    struct mxsb_writer writer; struct mxsb_limits limits; uint32_t i;
    uint32_t input[5]={0,1,type,location,interpolation};
    uint32_t second[5]={0,2,type,location,interpolation};
    uint32_t output[3]={(3u<<16)|MXSB_OP_STAGE_OUTPUT,location,1};
    uint32_t return_value[2]={(2u<<16)|MXSB_OP_RETURN_VALUE,1};
    uint32_t constant[4]={(4u<<16)|MXSB_OP_CONSTANT,3,MXSB_TYPE_F32,0};
    uint32_t vector[8]={(8u<<16)|MXSB_OP_CONSTRUCT,4,MXSB_TYPE_F32X4,4,3,3,3,3};
    const uint32_t *records[6]; uint32_t sizes[6],count=0;
    uint32_t input_words=opcode == MXSB_OP_STAGE_INPUT ? 5u : 4u;
    input[0]=(input_words<<16)|opcode; second[0]=input[0];
    assert(mxsb_writer_init(&writer,words,128,minor) == MXSB_OK);
    assert(mxsb_writer_entry(&writer,1,stage,root) == MXSB_OK);
    records[count]=input; sizes[count++]=input_words;
    if (duplicate) { records[count]=second; sizes[count++]=input_words; }
    if (type != MXSB_TYPE_F32X4) {
        records[count]=constant; sizes[count++]=4;
        records[count]=vector; sizes[count++]=8; return_value[1]=4;
    }
    if (stage == MXSB_STAGE_VERTEX) { records[count]=output; sizes[count++]=3; }
    records[count]=return_value; sizes[count++]=2;
    assert(mxsb_writer_block(&writer,1,1,records,sizes,count) == MXSB_OK);
    words[2]=writer.count; words[3]=writer.binding_count;
    words[4]=writer.entry_count; words[5]=writer.block_count;
    for (i=0;i<writer.count;++i) mx_w32(bytes,i*4u,words[i]);
    mxsb_limits_default(&limits);
    return mxsb_verify(bytes,writer.count*4u,&limits);
}

int main(void)
{
    uint16_t minor=123; struct mxsb_limits limits; uint32_t length,type;
    assert(mxsb_opcode_minimum_minor(MXSB_OP_VERTEX_ATTRIBUTE,&minor) == MXSB_OK && minor == 5);
    assert(mxsb_opcode_minimum_minor(MXSB_OP_STAGE_INPUT,&minor) == MXSB_OK && minor == 1);
    assert(mxsb_opcode_minimum_minor(MXSB_OP_STAGE_OUTPUT,&minor) == MXSB_OK && minor == 1);
    minor=123; assert(mxsb_opcode_minimum_minor(0xffffu,&minor) == MXSB_ERR_OPCODE && minor == 123);
    assert(mxsb_opcode_minimum_minor(MXSB_OP_BUFFER_STORE,&minor) == MXSB_OK && minor == 0);
    assert(module(MXSB_OP_VERTEX_ATTRIBUTE,MXSB_TYPE_F32X4,0,0,MXSB_STAGE_VERTEX,5,0,1) == MXSB_OK);
    assert(module(MXSB_OP_VERTEX_ATTRIBUTE,MXSB_TYPE_F32X4,0,0,MXSB_STAGE_VERTEX,4,0,1) == MXSB_ERR_OPCODE);
    assert(module(MXSB_OP_VERTEX_ATTRIBUTE,MXSB_TYPE_BOOL,0,0,MXSB_STAGE_VERTEX,5,0,1) == MXSB_ERR_TYPE);
    assert(module(MXSB_OP_VERTEX_ATTRIBUTE,MXSB_TYPE_F32X4,0,0,MXSB_STAGE_FRAGMENT,5,0,1) == MXSB_ERR_STAGE);
    assert(module(MXSB_OP_VERTEX_ATTRIBUTE,MXSB_TYPE_F32X4,0,0,MXSB_STAGE_VERTEX,5,1,1) == MXSB_ERR_VALUE);
    assert(module(MXSB_OP_VERTEX_ATTRIBUTE,MXSB_TYPE_F32X4,0,0,MXSB_STAGE_VERTEX,5,0,2) == MXSB_ERR_STRUCTURE);
    assert(module(MXSB_OP_STAGE_INPUT,MXSB_TYPE_F32X4,0,MXSB_INTERP_PERSPECTIVE,MXSB_STAGE_FRAGMENT,1,0,1) == MXSB_OK);
    assert(module(MXSB_OP_STAGE_INPUT,MXSB_TYPE_F32X4,127,MXSB_INTERP_PERSPECTIVE,MXSB_STAGE_FRAGMENT,1,0,1) == MXSB_ERR_VALUE);
    assert(module(MXSB_OP_STAGE_INPUT,MXSB_TYPE_F32X4,0,0,MXSB_STAGE_FRAGMENT,1,0,1) == MXSB_ERR_TYPE);
    assert(module(MXSB_OP_STAGE_INPUT,MXSB_TYPE_U32X4,0,MXSB_INTERP_PERSPECTIVE,MXSB_STAGE_FRAGMENT,1,0,1) == MXSB_ERR_TYPE);
    for (type=MXSB_TYPE_I32;type<=MXSB_TYPE_F32X4;++type) {
        assert(module(MXSB_OP_VERTEX_ATTRIBUTE,type,0,0,MXSB_STAGE_VERTEX,5,0,1) == MXSB_OK);
        assert(module(MXSB_OP_STAGE_INPUT,type,0,MXSB_INTERP_FLAT,MXSB_STAGE_FRAGMENT,1,0,1) == MXSB_OK);
    }
    assert(module(MXSB_OP_STAGE_INPUT,MXSB_TYPE_F32X4,0,MXSB_INTERP_FLAT,MXSB_STAGE_FRAGMENT,1,1,1) == MXSB_ERR_VALUE);
    assert(module(MXSB_OP_STAGE_INPUT,MXSB_TYPE_F32X4,0,MXSB_INTERP_PERSPECTIVE,MXSB_STAGE_FRAGMENT,1,0,2) == MXSB_ERR_STRUCTURE);
    assert(module(MXSB_OP_VERTEX_ATTRIBUTE,MXSB_TYPE_F32X4,126,0,MXSB_STAGE_VERTEX,5,0,1) == MXSB_OK);
    length=words[2]*4u; mxsb_limits_default(&limits);
    mx_w32(bytes,24u*4u,127);
    assert(mxsb_verify(bytes,length,&limits) == MXSB_ERR_VALUE);
    mx_w32(bytes,24u*4u,0); mx_w32(bytes,25u*4u,99);
    assert(mxsb_verify(bytes,length,&limits) == MXSB_ERR_VALUE);
    mx_w32(bytes,22u*4u,UINT32_MAX); mx_w32(bytes,25u*4u,1);
    assert(mxsb_verify(bytes,length,&limits) == MXSB_OK);
    assert(module(MXSB_OP_STAGE_INPUT,MXSB_TYPE_F32X4,0,MXSB_INTERP_PERSPECTIVE,MXSB_STAGE_FRAGMENT,5,0,1) == MXSB_OK);
    {
        struct mxsb_writer writer;
        uint32_t input[5]={(5u<<16)|MXSB_OP_STAGE_INPUT,1,MXSB_TYPE_F32X4,0,MXSB_INTERP_PERSPECTIVE};
        uint32_t output[3]={(3u<<16)|MXSB_OP_STAGE_OUTPUT,1,1};
        uint32_t returned[2]={(2u<<16)|MXSB_OP_RETURN_VALUE,1};
        const uint32_t *records[]={input,output,returned}; uint32_t sizes[]={5,3,2};
        assert(mxsb_writer_init(&writer,words,128,MXSB_FRAGMENT_STAGE_OUTPUT_MINIMUM_MINOR) == MXSB_OK);
        assert(mxsb_writer_entry(&writer,1,MXSB_STAGE_FRAGMENT,1) == MXSB_OK);
        assert(mxsb_writer_block(&writer,1,1,records,sizes,3) == MXSB_OK);
        assert(mxsb_writer_finish(&writer,bytes,sizeof(bytes),&length) == MXSB_OK);
        mx_w32(bytes,25u*4u,0); assert(mxsb_verify(bytes,length,&limits) == MXSB_ERR_VALUE);
        mx_w32(bytes,25u*4u,8); assert(mxsb_verify(bytes,length,&limits) == MXSB_ERR_VALUE);
        mx_w32(bytes,25u*4u,1); mx_w32(bytes,4,((uint32_t)MXSB_VERSION_MAJOR<<16)|4u);
        assert(mxsb_verify(bytes,length,&limits) == MXSB_ERR_STAGE);
    }
    return 0;
}
