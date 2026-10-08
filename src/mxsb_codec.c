/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxsb.h"

#include "mx_le.h"

#include <string.h>

int mxsb_limits_default(struct mxsb_limits *out)
{
    if (!out)
        return MXSB_ERR_LIMIT;
    out->max_words = 1048576u;
    out->max_bindings = 128u;
    out->max_entry_points = 64u;
    out->max_blocks = 4096u;
    out->max_instructions = 65536u;
    return MXSB_OK;
}

static int limits_ok(const struct mxsb_limits *limits)
{
    if (!limits || limits->max_words < MXSB_HEADER_WORDS || limits->max_bindings == 0 ||
        limits->max_entry_points == 0 || limits->max_blocks == 0 || limits->max_instructions == 0)
        return MXSB_ERR_LIMIT;
    return MXSB_OK;
}

static uint32_t opcode_minor(uint16_t opcode)
{
    if (opcode >= 1 && opcode <= 20)
        return 0;
    if (opcode == 21 || opcode == 22)
        return 1;
    if (opcode == 23)
        return 2;
    if (opcode == MXSB_OP_VERTEX_ATTRIBUTE || opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND)
        return 5;
    if (opcode >= 36 && opcode <= 48)
        return 6;
    if (opcode >= 50 && opcode <= 62)
        return 9;
    if (opcode >= MXSB_OP_WORKGROUP_MEMORY && opcode <= MXSB_OP_WORKGROUP_ATOMIC_ADD)
        return 16;
    if (opcode >= MXSB_OP_BUFFER_ATOMIC_LOAD && opcode <= MXSB_OP_DEVICE_MEMORY_BARRIER)
        return 20;
    if (opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND_OPERANDS)
        return 67;
    if (opcode >= MXSB_OP_TEXTURE_SAMPLE_LOD && opcode <= MXSB_OP_TEXTURE_SAMPLE_BOUND_GRAD)
        return 10;
    if (opcode >= MXSB_OP_DDX && opcode <= MXSB_OP_FWIDTH)
        return 21;
    if (opcode >= 0x100 && opcode <= 0x105)
        return 0;
    return 0xffffu;
}

static int record_words_ok(uint16_t opcode, uint32_t words);

int mxsb_opcode_minimum_minor(uint16_t opcode, uint16_t *minor)
{
    uint32_t value,words;
    if (!minor) return MXSB_ERR_STRUCTURE;
    value=opcode_minor(opcode);
    if (value == 0xffffu) return MXSB_ERR_OPCODE;
    for (words=1;words<=14u;++words) if (record_words_ok(opcode,words)) break;
    if (words > 14u) return MXSB_ERR_OPCODE;
    *minor=(uint16_t)value;
    return MXSB_OK;
}

static int record_words_ok(uint16_t opcode, uint32_t words)
{
    switch (opcode) {
    case MXSB_OP_CONSTANT:
    case MXSB_OP_BUILTIN:
    case MXSB_OP_VERTEX_ATTRIBUTE:
    case MXSB_OP_DDX:
    case MXSB_OP_DDY:
    case MXSB_OP_FWIDTH:
    case MXSB_OP_NEG:
    case MXSB_OP_FABS:
    case MXSB_OP_FLOOR:
    case MXSB_OP_CEIL:
    case MXSB_OP_FRACT:
    case MXSB_OP_SQRT:
    case MXSB_OP_RSQ:
    case MXSB_OP_RCP:
    case MXSB_OP_EXP2:
    case MXSB_OP_LOG2:
    case MXSB_OP_SIN:
    case MXSB_OP_COS:
    case MXSB_OP_NOT:
    case MXSB_OP_BIT_NOT:
    case MXSB_OP_I2F:
    case MXSB_OP_U2F:
    case MXSB_OP_F2I:
    case MXSB_OP_F2U:
    case MXSB_OP_BITCAST:
        return words == 4;
    case MXSB_OP_ADD:
    case MXSB_OP_SUB:
    case MXSB_OP_MUL:
    case MXSB_OP_DIV:
    case MXSB_OP_EQ:
    case MXSB_OP_LT:
    case MXSB_OP_LE:
    case MXSB_OP_BIT_AND:
    case MXSB_OP_BIT_OR:
    case MXSB_OP_BIT_XOR:
    case MXSB_OP_SHL:
    case MXSB_OP_SHR:
    case MXSB_OP_IDIV:
    case MXSB_OP_IMOD:
        return words == 5;
    case MXSB_OP_SELECT:
        return words == 6;
    case MXSB_OP_EXTRACT:
    case MXSB_OP_BUFFER_LOAD:
    case MXSB_OP_STAGE_INPUT:
    case MXSB_OP_WORKGROUP_LOAD:
    case MXSB_OP_WORKGROUP_STORE:
    case MXSB_OP_WORKGROUP_ATOMIC_LOAD:
    case MXSB_OP_WORKGROUP_ATOMIC_STORE:
    case MXSB_OP_BUFFER_ATOMIC_LOAD:
    case MXSB_OP_BUFFER_ATOMIC_STORE:
        return words == 5;
    case MXSB_OP_BUFFER_STORE:
    case MXSB_OP_TEXTURE_STORE:
        return words == 4;
    case MXSB_OP_WORKGROUP_ATOMIC_EXCHANGE:
    case MXSB_OP_WORKGROUP_ATOMIC_ADD:
    case MXSB_OP_BUFFER_ATOMIC_EXCHANGE:
    case MXSB_OP_BUFFER_ATOMIC_ADD:
    case MXSB_OP_BUFFER_ATOMIC_SUBTRACT:
    case MXSB_OP_BUFFER_ATOMIC_MINIMUM:
    case MXSB_OP_BUFFER_ATOMIC_MAXIMUM:
    case MXSB_OP_BUFFER_ATOMIC_AND:
    case MXSB_OP_BUFFER_ATOMIC_OR:
    case MXSB_OP_BUFFER_ATOMIC_XOR:
        return words == 6;
    case MXSB_OP_WORKGROUP_ATOMIC_COMPARE_EXCHANGE:
    case MXSB_OP_BUFFER_ATOMIC_COMPARE_EXCHANGE:
        return words == 7;
    case MXSB_OP_WORKGROUP_MEMORY:
        return words == 2;
    case MXSB_OP_CONTROL_BARRIER:
    case MXSB_OP_MEMORY_BARRIER:
    case MXSB_OP_DEVICE_MEMORY_BARRIER:
        return words == 1;
    case MXSB_OP_CONSTRUCT:
        return words >= 4;
    case MXSB_OP_STAGE_OUTPUT:
        return words == 3;
    case MXSB_OP_TEXTURE_SAMPLE:
    case MXSB_OP_TEXTURE_SAMPLE_BOUND:
        return words == 6;
    case MXSB_OP_TEXTURE_SAMPLE_LOD:
    case MXSB_OP_TEXTURE_SAMPLE_BOUND_LOD:
        return words == 7;
    case MXSB_OP_TEXTURE_SAMPLE_GRAD:
    case MXSB_OP_TEXTURE_SAMPLE_BOUND_GRAD:
        return words == 8;
    case MXSB_OP_TEXTURE_SAMPLE_BOUND_OPERANDS:
        return words == 14;
    case MXSB_OP_RETURN_VOID:
    case MXSB_OP_DISCARD:
        return words == 1;
    case MXSB_OP_RETURN_VALUE:
        return words == 2;
    default:
        return 0;
    }
}

static int opcode_has_result(uint16_t opcode)
{
    switch (opcode) {
    case MXSB_OP_STAGE_OUTPUT:
    case MXSB_OP_BUFFER_STORE:
    case MXSB_OP_TEXTURE_STORE:
    case MXSB_OP_WORKGROUP_MEMORY:
    case MXSB_OP_WORKGROUP_STORE:
    case MXSB_OP_CONTROL_BARRIER:
    case MXSB_OP_MEMORY_BARRIER:
    case MXSB_OP_WORKGROUP_ATOMIC_STORE:
    case MXSB_OP_BUFFER_ATOMIC_STORE:
    case MXSB_OP_DEVICE_MEMORY_BARRIER:
        return 0;
    default:
        return opcode < 0x100u;
    }
}

static int interface_type(uint32_t type)
{
    return type >= MXSB_TYPE_I32 && type <= MXSB_TYPE_F32X4;
}

static int interface_record(const uint8_t *bytes, uint32_t base, uint32_t cursor,
    uint16_t opcode, uint32_t stage, uint32_t root, uint16_t minor)
{
    uint32_t prior=base+4u;
    uint32_t location=mx_r32(bytes,(cursor+(opcode == MXSB_OP_STAGE_OUTPUT ? 1u : 3u))*4u);
    if (mx_r32(bytes,base*4u) != root) return MXSB_ERR_STRUCTURE;
    if (opcode == MXSB_OP_STAGE_INPUT) {
        uint32_t type=mx_r32(bytes,(cursor+2u)*4u), interpolation=mx_r32(bytes,(cursor+4u)*4u);
        if (stage != MXSB_STAGE_FRAGMENT) return MXSB_ERR_STAGE;
        if (!interface_type(type) || interpolation < MXSB_INTERP_PERSPECTIVE ||
            interpolation > MXSB_INTERP_FLAT ||
            (interpolation != MXSB_INTERP_FLAT && type != MXSB_TYPE_F32 &&
             (type < MXSB_TYPE_F32X2 || type > MXSB_TYPE_F32X4))) return MXSB_ERR_TYPE;
    } else if (opcode == MXSB_OP_VERTEX_ATTRIBUTE) {
        if (stage != MXSB_STAGE_VERTEX) return MXSB_ERR_STAGE;
        if (!interface_type(mx_r32(bytes,(cursor+2u)*4u))) return MXSB_ERR_TYPE;
    } else if (stage == MXSB_STAGE_FRAGMENT) {
        if (minor < MXSB_FRAGMENT_STAGE_OUTPUT_MINIMUM_MINOR) return MXSB_ERR_STAGE;
        if (!location || location >= MXSB_MAX_COLOR_ATTACHMENTS) return MXSB_ERR_VALUE;
    } else if (stage != MXSB_STAGE_VERTEX) return MXSB_ERR_STAGE;
    if (opcode != MXSB_OP_VERTEX_ATTRIBUTE && location >= MXSB_MAX_STAGE_INTERFACE_LOCATIONS)
        return MXSB_ERR_VALUE;
    while (prior < cursor) {
        uint32_t head=mx_r32(bytes,prior*4u);
        if ((uint16_t)head == opcode &&
            mx_r32(bytes,(prior+(opcode == MXSB_OP_STAGE_OUTPUT ? 1u : 3u))*4u) == location)
            return MXSB_ERR_VALUE;
        prior+=head>>16;
    }
    if (opcode == MXSB_OP_STAGE_OUTPUT) {
        uint32_t value=mx_r32(bytes,(cursor+2u)*4u), found=0;
        prior=base+4u;
        while (prior < cursor) {
            uint32_t head=mx_r32(bytes,prior*4u);
            if (opcode_has_result((uint16_t)head) && mx_r32(bytes,(prior+1u)*4u) == value) {
                uint32_t type=mx_r32(bytes,(prior+2u)*4u);
                if (!interface_type(type) || (stage == MXSB_STAGE_FRAGMENT && type != MXSB_TYPE_F32X4))
                    return MXSB_ERR_TYPE;
                found=1;
            }
            prior+=head>>16;
        }
        if (!found) return MXSB_ERR_VALUE;
    }
    return MXSB_OK;
}

static uint32_t binding_word(const uint8_t *bytes, uint32_t index, uint32_t field)
{
    return mx_r32(bytes, (MXSB_HEADER_WORDS + index * MXSB_BINDING_WORDS + field) * 4u);
}

static int result_precedes(const uint8_t *bytes, uint32_t first_block, uint32_t block_base,
                           uint32_t cursor, uint32_t entry, uint32_t result)
{
    uint32_t block = first_block;
    while (block <= block_base) {
        uint32_t end = block == block_base ? cursor : block + mx_r32(bytes, (block + 2u) * 4u);
        if (mx_r32(bytes, (block + 1u) * 4u) == entry) {
            uint32_t prior = block + 4u;
            while (prior < end) {
                uint32_t head = mx_r32(bytes, prior * 4u);
                if (opcode_has_result((uint16_t)head) && mx_r32(bytes, (prior + 1u) * 4u) == result)
                    return 1;
                prior += head >> 16;
            }
        }
        if (block == block_base)
            break;
        block += mx_r32(bytes, (block + 2u) * 4u);
    }
    return 0;
}

static int return_precedes(const uint8_t *bytes, uint32_t first_block, uint32_t block_base,
                           uint32_t entry, uint16_t opcode)
{
    uint32_t block = first_block;
    while (block < block_base) {
        uint32_t end = block + mx_r32(bytes, (block + 2u) * 4u);
        if (mx_r32(bytes, (block + 1u) * 4u) == entry) {
            uint32_t prior = block + 4u;
            while (prior < end) {
                uint32_t head = mx_r32(bytes, prior * 4u);
                if ((uint16_t)head == opcode)
                    return 1;
                prior += head >> 16;
            }
        }
        block = end;
    }
    return 0;
}

struct vctx {
    const uint8_t *bytes;
    uint32_t words;
    uint32_t bindings;
    uint32_t blocks;
    uint32_t blocks_base;
    uint32_t block_base;
    uint32_t cursor;
    uint32_t entry;
    uint32_t root;
    uint32_t stage;
    uint16_t minor;
};

struct builtin_info {
    uint32_t id;
    uint32_t stage;
    uint32_t type;
    uint32_t minor;
};

/* A zero stage admits every stage that takes part in a draw. */
static const struct builtin_info builtins[] = {
    {MXSB_BUILTIN_GLOBAL_INVOCATION_ID, MXSB_STAGE_COMPUTE, MXSB_TYPE_U32X3, 0},
    {MXSB_BUILTIN_LOCAL_INVOCATION_ID, MXSB_STAGE_COMPUTE, MXSB_TYPE_U32X3, 0},
    {MXSB_BUILTIN_WORKGROUP_ID, MXSB_STAGE_COMPUTE, MXSB_TYPE_U32X3, 0},
    {MXSB_BUILTIN_VERTEX_ID, MXSB_STAGE_VERTEX, MXSB_TYPE_U32, 0},
    {MXSB_BUILTIN_INSTANCE_ID, MXSB_STAGE_VERTEX, MXSB_TYPE_U32, 0},
    {MXSB_BUILTIN_BASE_VERTEX, MXSB_STAGE_VERTEX, MXSB_TYPE_U32, 7},
    {MXSB_BUILTIN_BASE_INSTANCE, MXSB_STAGE_VERTEX, MXSB_TYPE_U32, 7},
    {MXSB_BUILTIN_VIEW_INDEX, 0, MXSB_TYPE_U32, 38},
    {MXSB_BUILTIN_LOCAL_INVOCATION_INDEX, MXSB_STAGE_COMPUTE, MXSB_TYPE_U32, 51},
    {MXSB_BUILTIN_WORKGROUP_SIZE, MXSB_STAGE_COMPUTE, MXSB_TYPE_U32X3, 52},
    {MXSB_BUILTIN_DISPATCH_WORKGROUP_SIZE, MXSB_STAGE_COMPUTE, MXSB_TYPE_U32X3, 52},
    {MXSB_BUILTIN_NUM_WORKGROUPS, MXSB_STAGE_COMPUTE, MXSB_TYPE_U32X3, 52},
    {MXSB_BUILTIN_GLOBAL_SIZE, MXSB_STAGE_COMPUTE, MXSB_TYPE_U32X3, 52},
};

static uint32_t ctx_word(const struct vctx *c, uint32_t index)
{
    return mx_r32(c->bytes, (c->cursor + index) * 4u);
}

static int find_binding(const struct vctx *c, uint32_t id, uint32_t *index)
{
    uint32_t w;
    for (w = 0; w < c->bindings; w++) {
        if (binding_word(c->bytes, w, 0) == id) {
            *index = w;
            return 1;
        }
    }
    return 0;
}

static uint32_t binding_kind(const struct vctx *c, uint32_t index)
{
    return (binding_word(c->bytes, index, 1) >> 16) & 15u;
}

static uint32_t binding_access(const struct vctx *c, uint32_t index)
{
    return (binding_word(c->bytes, index, 1) >> 20) & 15u;
}

static int operand_check(const struct vctx *c, uint32_t value, uint32_t expected)
{
    uint32_t prior = c->block_base + 4u;
    int found = 0;
    while (prior < c->cursor) {
        uint32_t head = mx_r32(c->bytes, prior * 4u);
        if (opcode_has_result((uint16_t)head) && mx_r32(c->bytes, (prior + 1u) * 4u) == value) {
            if (mx_r32(c->bytes, (prior + 2u) * 4u) != expected)
                return MXSB_ERR_TYPE;
            found = 1;
        }
        prior += head >> 16;
    }
    return found ? MXSB_OK : MXSB_ERR_VALUE;
}

static uint32_t type_bytes(uint32_t type)
{
    return 4u * (type <= MXSB_TYPE_F32 ? 1u : (type - MXSB_TYPE_I32X2) % 3u + 2u);
}

/* Byte size of the entry's workgroup memory declaration, or zero when it declares none. */
static uint32_t entry_workgroup_bytes(const struct vctx *c)
{
    uint32_t block = c->blocks_base;
    uint32_t i;
    for (i = 0; i < c->blocks; i++) {
        uint32_t size;
        uint32_t head;
        if (c->words - block < 4u)
            return 0;
        size = mx_r32(c->bytes, (block + 2u) * 4u);
        if (size < 4u || size > c->words - block)
            return 0;
        head = size >= 6u ? mx_r32(c->bytes, (block + 4u) * 4u) : 0;
        if (mx_r32(c->bytes, (block + 1u) * 4u) == c->entry &&
            mx_r32(c->bytes, block * 4u) == c->root && (uint16_t)head == MXSB_OP_WORKGROUP_MEMORY &&
            (head >> 16) == 2u)
            return mx_r32(c->bytes, (block + 5u) * 4u);
        block += size;
    }
    return 0;
}

static int builtin_record(const struct vctx *c)
{
    uint32_t type = ctx_word(c, 2);
    uint32_t id = ctx_word(c, 3);
    size_t i;
    for (i = 0; i < sizeof builtins / sizeof builtins[0]; i++) {
        if (builtins[i].id != id)
            continue;
        if (builtins[i].stage ? builtins[i].stage != c->stage : c->stage == MXSB_STAGE_COMPUTE)
            return MXSB_ERR_STAGE;
        if (c->minor < builtins[i].minor)
            return MXSB_ERR_VERSION;
        return type == builtins[i].type ? MXSB_OK : MXSB_ERR_TYPE;
    }
    return c->stage == MXSB_STAGE_FRAGMENT ? MXSB_ERR_STAGE : MXSB_ERR_VALUE;
}

static int buffer_store_record(const struct vctx *c)
{
    uint32_t w;
    int status;
    if (!find_binding(c, ctx_word(c, 1), &w) || binding_kind(c, w) != MXSB_BINDING_STORAGE ||
        !(binding_access(c, w) & MXSB_ACCESS_WRITE))
        return MXSB_ERR_BINDING;
    status = operand_check(c, ctx_word(c, 2), MXSB_TYPE_U32);
    if (status != MXSB_OK)
        return status;
    return operand_check(c, ctx_word(c, 3), binding_word(c->bytes, w, 2));
}

static int texture_store_record(const struct vctx *c)
{
    uint32_t w;
    uint32_t kind;
    int status;
    if (!find_binding(c, ctx_word(c, 1), &w))
        return MXSB_ERR_BINDING;
    kind = binding_kind(c, w);
    if (kind != MXSB_BINDING_TEXTURE_2D && kind != MXSB_BINDING_TEXTURE_CUBE)
        return MXSB_ERR_BINDING;
    if (kind == MXSB_BINDING_TEXTURE_CUBE && c->minor < MXSB_CUBE_TEXEL_ACCESS_MINIMUM_MINOR)
        return MXSB_ERR_VERSION;
    if (!(binding_access(c, w) & MXSB_ACCESS_WRITE))
        return MXSB_ERR_BINDING;
    status = operand_check(c, ctx_word(c, 2),
                           kind == MXSB_BINDING_TEXTURE_CUBE ? MXSB_TYPE_U32X3 : MXSB_TYPE_U32X2);
    if (status != MXSB_OK)
        return status;
    return operand_check(c, ctx_word(c, 3), binding_word(c->bytes, w, 2));
}

static int buffer_atomic_record(const struct vctx *c, uint16_t opcode)
{
    int load = opcode == MXSB_OP_BUFFER_ATOMIC_LOAD;
    int store = opcode == MXSB_OP_BUFFER_ATOMIC_STORE;
    uint32_t first = store ? 1u : 2u;
    uint32_t type = ctx_word(c, first);
    uint32_t w;
    int status;
    if (!find_binding(c, ctx_word(c, first + 1u), &w) ||
        binding_kind(c, w) != MXSB_BINDING_STORAGE || !(binding_access(c, w) & MXSB_ACCESS_READ) ||
        (!load && !(binding_access(c, w) & MXSB_ACCESS_WRITE)))
        return MXSB_ERR_BINDING;
    if (type != binding_word(c->bytes, w, 2) || (type != MXSB_TYPE_I32 && type != MXSB_TYPE_U32))
        return MXSB_ERR_TYPE;
    status = operand_check(c, ctx_word(c, first + 2u), MXSB_TYPE_U32);
    if (status != MXSB_OK)
        return status;
    if (opcode == MXSB_OP_BUFFER_ATOMIC_COMPARE_EXCHANGE) {
        status = operand_check(c, ctx_word(c, 5), type);
        if (status != MXSB_OK)
            return status;
        return operand_check(c, ctx_word(c, 6), type);
    }
    if (load)
        return MXSB_OK;
    return operand_check(c, ctx_word(c, store ? 4u : 5u), type);
}

static int workgroup_record(const struct vctx *c, uint16_t opcode)
{
    int stored = opcode == MXSB_OP_WORKGROUP_STORE || opcode == MXSB_OP_WORKGROUP_ATOMIC_STORE;
    int atomic = opcode >= MXSB_OP_WORKGROUP_ATOMIC_LOAD;
    uint32_t type = ctx_word(c, stored ? 1u : 2u);
    uint32_t offset = ctx_word(c, stored ? 2u : 3u);
    uint32_t declared;
    uint32_t size;
    int status;
    if (c->stage != MXSB_STAGE_COMPUTE)
        return MXSB_ERR_STAGE;
    declared = entry_workgroup_bytes(c);
    if (declared == 0)
        return MXSB_ERR_STRUCTURE;
    if (atomic ? (type != MXSB_TYPE_I32 && type != MXSB_TYPE_U32) : !interface_type(type))
        return MXSB_ERR_TYPE;
    size = type_bytes(type);
    if (offset % size != 0 || offset > declared || size > declared - offset)
        return MXSB_ERR_VALUE;
    status = operand_check(c, ctx_word(c, stored ? 3u : 4u), MXSB_TYPE_U32);
    if (status != MXSB_OK)
        return status;
    switch (opcode) {
    case MXSB_OP_WORKGROUP_STORE:
    case MXSB_OP_WORKGROUP_ATOMIC_STORE:
        return operand_check(c, ctx_word(c, 4), type);
    case MXSB_OP_WORKGROUP_ATOMIC_EXCHANGE:
    case MXSB_OP_WORKGROUP_ATOMIC_ADD:
        return operand_check(c, ctx_word(c, 5), type);
    case MXSB_OP_WORKGROUP_ATOMIC_COMPARE_EXCHANGE:
        status = operand_check(c, ctx_word(c, 5), type);
        if (status != MXSB_OK)
            return status;
        return operand_check(c, ctx_word(c, 6), type);
    default:
        return MXSB_OK;
    }
}

/* Checks for the builtin, storage, barrier, workgroup memory and atomic records. */
static int extended_record(const struct vctx *c, uint16_t opcode)
{
    switch (opcode) {
    case MXSB_OP_BUILTIN:
        return builtin_record(c);
    case MXSB_OP_BUFFER_STORE:
        return buffer_store_record(c);
    case MXSB_OP_TEXTURE_STORE:
        return texture_store_record(c);
    case MXSB_OP_WORKGROUP_MEMORY:
        if (c->stage != MXSB_STAGE_COMPUTE)
            return MXSB_ERR_STAGE;
        if (mx_r32(c->bytes, c->block_base * 4u) != c->root || c->cursor != c->block_base + 4u)
            return MXSB_ERR_STRUCTURE;
        return ctx_word(c, 1) == 0 || (ctx_word(c, 1) & 3u) ? MXSB_ERR_VALUE : MXSB_OK;
    case MXSB_OP_CONTROL_BARRIER:
        if (c->stage != MXSB_STAGE_COMPUTE)
            return MXSB_ERR_STAGE;
        return mx_r32(c->bytes, c->block_base * 4u) == c->root ? MXSB_OK : MXSB_ERR_STRUCTURE;
    case MXSB_OP_MEMORY_BARRIER:
        return c->stage == MXSB_STAGE_COMPUTE ? MXSB_OK : MXSB_ERR_STAGE;
    case MXSB_OP_WORKGROUP_LOAD:
    case MXSB_OP_WORKGROUP_STORE:
    case MXSB_OP_WORKGROUP_ATOMIC_LOAD:
    case MXSB_OP_WORKGROUP_ATOMIC_STORE:
    case MXSB_OP_WORKGROUP_ATOMIC_EXCHANGE:
    case MXSB_OP_WORKGROUP_ATOMIC_COMPARE_EXCHANGE:
    case MXSB_OP_WORKGROUP_ATOMIC_ADD:
        return workgroup_record(c, opcode);
    case MXSB_OP_BUFFER_ATOMIC_LOAD:
    case MXSB_OP_BUFFER_ATOMIC_STORE:
    case MXSB_OP_BUFFER_ATOMIC_EXCHANGE:
    case MXSB_OP_BUFFER_ATOMIC_COMPARE_EXCHANGE:
    case MXSB_OP_BUFFER_ATOMIC_ADD:
    case MXSB_OP_BUFFER_ATOMIC_SUBTRACT:
    case MXSB_OP_BUFFER_ATOMIC_MINIMUM:
    case MXSB_OP_BUFFER_ATOMIC_MAXIMUM:
    case MXSB_OP_BUFFER_ATOMIC_AND:
    case MXSB_OP_BUFFER_ATOMIC_OR:
    case MXSB_OP_BUFFER_ATOMIC_XOR:
        return buffer_atomic_record(c, opcode);
    default:
        return MXSB_OK;
    }
}

int mxsb_verify(const uint8_t *bytes, uint32_t len, const struct mxsb_limits *limits)
{
    uint32_t words;
    uint32_t i;
    uint16_t major;
    uint16_t minor;
    uint32_t declared;
    uint32_t bindings;
    uint32_t entries;
    uint32_t blocks;
    uint32_t cursor;
    uint32_t entry_base;
    uint32_t blocks_base;
    uint32_t instruction_total = 0;
    struct vctx vc;
    memset(&vc, 0, sizeof vc);
    if (limits_ok(limits) != MXSB_OK)
        return MXSB_ERR_LIMIT;
    if (!bytes || (len & 3u))
        return MXSB_ERR_LENGTH;
    words = len / 4u;
    if (words < MXSB_HEADER_WORDS || words > limits->max_words)
        return MXSB_ERR_LENGTH;
    if (mx_r32(bytes, 0) != MXSB_MAGIC)
        return MXSB_ERR_MAGIC;
    major = (uint16_t)(mx_r32(bytes, 4) >> 16);
    minor = (uint16_t)mx_r32(bytes, 4);
    if (major != MXSB_VERSION_MAJOR || minor > MXSB_VERSION_MINOR)
        return MXSB_ERR_VERSION;
    declared = mx_r32(bytes, 8);
    if (declared != words)
        return MXSB_ERR_LENGTH;
    bindings = mx_r32(bytes, 12);
    entries = mx_r32(bytes, 16);
    blocks = mx_r32(bytes, 20);
    if (minor < 4 && mx_r32(bytes, 24) != 0)
        return MXSB_ERR_RESERVED;
    if (minor < 63 && mx_r32(bytes, 28) != 0)
        return MXSB_ERR_RESERVED;
    if (bindings > limits->max_bindings || entries == 0 || entries > limits->max_entry_points ||
        blocks == 0 || blocks > limits->max_blocks)
        return MXSB_ERR_LIMIT;
    cursor = MXSB_HEADER_WORDS;
    if (bindings > (words - cursor) / MXSB_BINDING_WORDS)
        return MXSB_ERR_LENGTH;
    entry_base = cursor + bindings * MXSB_BINDING_WORDS;
    if (entries > (words - entry_base) / MXSB_ENTRY_WORDS)
        return MXSB_ERR_LENGTH;
    blocks_base = entry_base + entries * MXSB_ENTRY_WORDS;
    for (i = 0; i < bindings; i++) {
        uint32_t base = (cursor + i * MXSB_BINDING_WORDS) * 4u;
        uint32_t packed = mx_r32(bytes, base + 4);
        uint32_t kind = (packed >> 16) & 0xfu;
        uint32_t access = (packed >> 20) & 0xfu;
        uint32_t value_type = mx_r32(bytes, base + 8);
        uint32_t elements = mx_r32(bytes, base + 12);
        uint32_t attr = mx_r32(bytes, base + 16);
        uint32_t prior;
        if (mx_r32(bytes, base) == 0)
            return MXSB_ERR_BINDING;
        for (prior = 0; prior < i; prior++)
            if (binding_word(bytes, prior, 0) == mx_r32(bytes, base))
                return MXSB_ERR_BINDING;
        if (attr != 0 || (packed >> 24) != 0)
            return MXSB_ERR_RESERVED;
        if (kind != MXSB_BINDING_UNIFORM && kind != MXSB_BINDING_STORAGE &&
            kind != MXSB_BINDING_TEXTURE_2D && kind != MXSB_BINDING_TEXTURE_CUBE &&
            kind != MXSB_BINDING_SAMPLER)
            return MXSB_ERR_BINDING;
        if (access < MXSB_ACCESS_READ || access > MXSB_ACCESS_READ_WRITE)
            return MXSB_ERR_BINDING;
        if ((kind == MXSB_BINDING_UNIFORM || kind == MXSB_BINDING_SAMPLER) &&
            access != MXSB_ACCESS_READ)
            return MXSB_ERR_BINDING;
        if (kind == MXSB_BINDING_TEXTURE_CUBE && access != MXSB_ACCESS_READ &&
            minor < MXSB_CUBE_TEXEL_ACCESS_MINIMUM_MINOR)
            return MXSB_ERR_BINDING;
        if (kind == MXSB_BINDING_STORAGE && (elements == 0 || !interface_type(value_type)))
            return MXSB_ERR_BINDING;
        if (kind == MXSB_BINDING_SAMPLER &&
            (minor < 5 || elements < 1 || elements > 9 || (elements > 1 && minor < 26) ||
             value_type != MXSB_TYPE_U32))
            return MXSB_ERR_BINDING;
        if (kind == MXSB_BINDING_UNIFORM && (elements == 0 || value_type != MXSB_TYPE_F32X4))
            return MXSB_ERR_BINDING;
        if ((kind == MXSB_BINDING_TEXTURE_2D || kind == MXSB_BINDING_TEXTURE_CUBE) &&
            (elements != 0 || value_type != MXSB_TYPE_F32X4 ||
             (kind == MXSB_BINDING_TEXTURE_CUBE && minor < 22)))
            return MXSB_ERR_BINDING;
    }
    cursor += bindings * MXSB_BINDING_WORDS;
    for (i = 0; i < entries; i++) {
        uint32_t base = (cursor + i * MXSB_ENTRY_WORDS) * 4u;
        uint32_t stage = mx_r32(bytes, base + 4);
        uint32_t wg0 = mx_r32(bytes, base + 12);
        uint32_t wg1 = mx_r32(bytes, base + 16);
        uint32_t wg2 = mx_r32(bytes, base + 20);
        uint32_t tail = mx_r32(bytes, base + 24);
        if (tail != 0)
            return MXSB_ERR_RESERVED;
        if (stage != MXSB_STAGE_COMPUTE && stage != MXSB_STAGE_VERTEX &&
            stage != MXSB_STAGE_FRAGMENT)
            return MXSB_ERR_STAGE;
        if (stage == MXSB_STAGE_COMPUTE) {
            uint64_t plane = (uint64_t)wg0 * wg1;
            if (wg0 == 0 || wg1 == 0 || wg2 == 0 || plane > UINT32_MAX ||
                plane * wg2 > UINT32_MAX)
                return MXSB_ERR_STRUCTURE;
        } else if (wg0 || wg1 || wg2) {
            return MXSB_ERR_STRUCTURE;
        }
        if (mx_r32(bytes, base) == 0)
            return MXSB_ERR_STRUCTURE;
        {
            uint32_t prior;
            for (prior = 0; prior < i; prior++)
                if (mx_r32(bytes, (entry_base + prior * MXSB_ENTRY_WORDS) * 4u) ==
                    mx_r32(bytes, base))
                    return MXSB_ERR_STRUCTURE;
        }
    }
    cursor += entries * MXSB_ENTRY_WORDS;
    for (i = 0; i < blocks; i++) {
        uint32_t block_base;
        uint32_t block_words;
        uint32_t record_count;
        uint32_t entry;
        uint32_t local;
        uint32_t stage = 0;
        uint32_t root = 0;
        uint32_t found = 0;
        uint32_t r;
        if (words - cursor < 4)
            return MXSB_ERR_LENGTH;
        block_base = cursor;
        block_words = mx_r32(bytes, (block_base + 2) * 4u);
        record_count = mx_r32(bytes, (block_base + 3) * 4u);
        entry = mx_r32(bytes, (block_base + 1) * 4u);
        if (block_words < 4 || block_words > words - block_base)
            return MXSB_ERR_LENGTH;
        for (local = 0; local < entries; local++) {
            if (mx_r32(bytes, (entry_base + local * MXSB_ENTRY_WORDS) * 4u) == entry) {
                stage = mx_r32(bytes, (entry_base + local * MXSB_ENTRY_WORDS + 1u) * 4u);
                root = mx_r32(bytes, (entry_base + local * MXSB_ENTRY_WORDS + 2u) * 4u);
                found = 1;
            }
        }
        if (!found)
            return MXSB_ERR_STRUCTURE;
        cursor = block_base + 4;
        vc.bytes = bytes;
        vc.words = words;
        vc.bindings = bindings;
        vc.blocks = blocks;
        vc.blocks_base = blocks_base;
        vc.block_base = block_base;
        vc.entry = entry;
        vc.root = root;
        vc.stage = stage;
        vc.minor = minor;
        if (record_count == 0)
            return MXSB_ERR_STRUCTURE;
        if (record_count > limits->max_instructions - instruction_total)
            return MXSB_ERR_LIMIT;
        instruction_total += record_count;
        if (record_count > block_words - 4u)
            return MXSB_ERR_LENGTH;
        for (r = 0; r < record_count; r++) {
            uint32_t head;
            uint32_t count;
            uint16_t opcode;
            uint32_t w;
            uint32_t result;
            if (cursor >= block_base + block_words)
                return MXSB_ERR_LENGTH;
            head = mx_r32(bytes, cursor * 4u);
            count = head >> 16;
            opcode = (uint16_t)head;
            if (count == 0 || count > block_base + block_words - cursor)
                return MXSB_ERR_LENGTH;
            if (opcode_minor(opcode) == 0xffffu || minor < opcode_minor(opcode))
                return MXSB_ERR_OPCODE;
            if (opcode == MXSB_OP_CONSTRUCT) {
                uint32_t lanes;
                if (count < 4)
                    return MXSB_ERR_STRUCTURE;
                lanes = mx_r32(bytes, (cursor + 3) * 4u);
                if (lanes != count - 4u)
                    return MXSB_ERR_STRUCTURE;
            } else if (!record_words_ok(opcode, count)) {
                return MXSB_ERR_STRUCTURE;
            }
            if ((opcode == MXSB_OP_TEXTURE_SAMPLE || opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND) &&
                stage != MXSB_STAGE_FRAGMENT)
                return MXSB_ERR_STAGE;
            if (opcode == MXSB_OP_STAGE_INPUT && stage != MXSB_STAGE_FRAGMENT)
                return MXSB_ERR_STAGE;
            vc.cursor = cursor;
            {
                int extended_status = extended_record(&vc, opcode);
                if (extended_status != MXSB_OK)
                    return extended_status;
            }
            if (opcode == MXSB_OP_STAGE_INPUT || opcode == MXSB_OP_STAGE_OUTPUT ||
                opcode == MXSB_OP_VERTEX_ATTRIBUTE) {
                int interface_status=interface_record(bytes,block_base,cursor,opcode,stage,root,minor);
                if (interface_status != MXSB_OK) return interface_status;
            }
            if (opcode >= MXSB_OP_DDX && opcode <= MXSB_OP_FWIDTH) {
                uint32_t type = mx_r32(bytes, (cursor + 2) * 4u);
                uint32_t value = mx_r32(bytes, (cursor + 3) * 4u);
                uint32_t prior = block_base + 4;
                uint32_t matched = 0;
                if (stage != MXSB_STAGE_FRAGMENT)
                    return MXSB_ERR_STAGE;
                if (type != MXSB_TYPE_F32 && type != MXSB_TYPE_F32X2 && type != MXSB_TYPE_F32X3 &&
                    type != MXSB_TYPE_F32X4)
                    return MXSB_ERR_TYPE;
                while (prior < cursor) {
                    uint32_t prior_head = mx_r32(bytes, prior * 4u);
                    uint32_t prior_count = prior_head >> 16;
                    if (opcode_has_result((uint16_t)prior_head) &&
                        mx_r32(bytes, (prior + 1) * 4u) == value) {
                        if (mx_r32(bytes, (prior + 2) * 4u) != type)
                            return MXSB_ERR_TYPE;
                        matched = 1;
                    }
                    prior += prior_count;
                }
                if (!matched)
                    return MXSB_ERR_VALUE;
            }
            if (opcode_has_result(opcode)) {
                result = mx_r32(bytes, (cursor + 1) * 4u);
                if (result == 0 ||
                    result_precedes(bytes, blocks_base, block_base, cursor, entry, result))
                    return MXSB_ERR_VALUE;
            }
            if (opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND_OPERANDS || opcode == MXSB_OP_BUFFER_LOAD ||
                (opcode >= MXSB_OP_TEXTURE_SAMPLE_LOD &&
                 opcode <= MXSB_OP_TEXTURE_SAMPLE_BOUND_GRAD) ||
                (opcode == MXSB_OP_TEXTURE_SAMPLE || opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND)) {
                uint32_t binding = mx_r32(bytes, (cursor + 3) * 4u);
                uint32_t matched = 0;
                for (w = 0; w < bindings; w++) {
                    if (binding_word(bytes, w, 0) == binding) {
                        matched = 1;
                        if (opcode == MXSB_OP_BUFFER_LOAD &&
                            ((binding_word(bytes, w, 1) >> 16) & 15u) != MXSB_BINDING_UNIFORM &&
                            ((binding_word(bytes, w, 1) >> 16) & 15u) != MXSB_BINDING_STORAGE)
                            return MXSB_ERR_BINDING;
                        if ((opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND_OPERANDS ||
                             opcode == MXSB_OP_TEXTURE_SAMPLE ||
                             opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND ||
                             (opcode >= MXSB_OP_TEXTURE_SAMPLE_LOD &&
                              opcode <= MXSB_OP_TEXTURE_SAMPLE_BOUND_GRAD)) &&
                            ((binding_word(bytes, w, 1) >> 16) & 15u) != MXSB_BINDING_TEXTURE_2D &&
                            ((binding_word(bytes, w, 1) >> 16) & 15u) != MXSB_BINDING_TEXTURE_CUBE)
                            return MXSB_ERR_BINDING;
                        if (opcode == MXSB_OP_BUFFER_LOAD
                                ? !(binding_access(&vc, w) & MXSB_ACCESS_READ)
                                : binding_access(&vc, w) != MXSB_ACCESS_READ)
                            return MXSB_ERR_BINDING;
                        if (mx_r32(bytes, (cursor + 2) * 4u) != binding_word(bytes, w, 2))
                            return MXSB_ERR_TYPE;
                        if (opcode == MXSB_OP_BUFFER_LOAD &&
                            binding_kind(&vc, w) == MXSB_BINDING_STORAGE) {
                            int index_status =
                                operand_check(&vc, mx_r32(bytes, (cursor + 4) * 4u), MXSB_TYPE_U32);
                            if (index_status != MXSB_OK)
                                return index_status;
                        }
                    }
                }
                if (!matched)
                    return MXSB_ERR_BINDING;
            }
            if (opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND_OPERANDS ||
                opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND ||
                opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND_LOD ||
                opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND_GRAD) {
                uint32_t sampler = mx_r32(bytes, (cursor + 4) * 4u);
                uint32_t matched = 0;
                if (opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND && stage != MXSB_STAGE_FRAGMENT)
                    return MXSB_ERR_STAGE;
                for (w = 0; w < bindings; w++)
                    if (binding_word(bytes, w, 0) == sampler &&
                        ((binding_word(bytes, w, 1) >> 16) & 15u) == MXSB_BINDING_SAMPLER)
                        matched = 1;
                if (!matched)
                    return MXSB_ERR_BINDING;
            }
            if (opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND_OPERANDS ||
                opcode == MXSB_OP_TEXTURE_SAMPLE || opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND ||
                (opcode >= MXSB_OP_TEXTURE_SAMPLE_LOD &&
                 opcode <= MXSB_OP_TEXTURE_SAMPLE_BOUND_GRAD)) {
                uint32_t extended = opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND_OPERANDS;
                uint32_t bound = extended || opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND ||
                                 opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND_LOD ||
                                 opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND_GRAD;
                uint32_t level_kind =
                    extended ? mx_r32(bytes, (cursor + 6) * 4u)
                    : (opcode == MXSB_OP_TEXTURE_SAMPLE || opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND)
                        ? 0u
                    : (opcode == MXSB_OP_TEXTURE_SAMPLE_GRAD ||
                       opcode == MXSB_OP_TEXTURE_SAMPLE_BOUND_GRAD)
                        ? 3u
                        : 2u;
                uint32_t gradient = level_kind == 3;
                uint32_t base_operands = level_kind == 0 ? 1u : gradient ? 3u : 2u;
                uint32_t floor = extended ? mx_r32(bytes, (cursor + 9) * 4u) : 0;
                uint32_t operands = base_operands + (floor != 0);
                if (extended) {
                    uint32_t first = mx_r32(bytes, (cursor + 7) * 4u),
                             second = mx_r32(bytes, (cursor + 8) * 4u);
                    if (level_kind > 3 || (level_kind == 0 && (first || second)) ||
                        ((level_kind == 1 || level_kind == 2) && second) ||
                        (level_kind == 2 && floor) || mx_r32(bytes, (cursor + 13) * 4u))
                        return MXSB_ERR_STRUCTURE;
                    if (level_kind <= 1 && stage != MXSB_STAGE_FRAGMENT)
                        return MXSB_ERR_STAGE;
                }
                uint32_t texture = mx_r32(bytes, (cursor + 3) * 4u), kind = 0, compare = 0;
                uint32_t operand;
                for (w = 0; w < bindings; w++)
                    if (binding_word(bytes, w, 0) == texture)
                        kind = ((binding_word(bytes, w, 1) >> 16) & 15u);
                if (extended) {
                    for (w = 0; w < 3; w++) {
                        uint32_t displacement = mx_r32(bytes, (cursor + 10 + w) * 4u);
                        if (displacement && (kind == MXSB_BINDING_TEXTURE_CUBE || w >= 2 ||
                                             (displacement > 7 && displacement < UINT32_MAX - 7)))
                            return MXSB_ERR_STRUCTURE;
                    }
                }
                if (bound) {
                    uint32_t sampler = mx_r32(bytes, (cursor + 4) * 4u);
                    for (w = 0; w < bindings; w++)
                        if (binding_word(bytes, w, 0) == sampler &&
                            ((binding_word(bytes, w, 1) >> 16) & 15u) == MXSB_BINDING_SAMPLER)
                            compare = binding_word(bytes, w, 3) - 1;
                } else {
                    uint32_t sampler = mx_r32(bytes, (cursor + 5) * 4u);
                    uint32_t filter = sampler & 15u, u = (sampler >> 4) & 15u,
                             v = (sampler >> 8) & 15u;
                    uint32_t mip = (sampler >> 12) & 15u, clamp = (sampler >> 16) & 15u;
                    compare = (sampler >> 20) & 15u;
                    if ((sampler & 0xff000000u) || filter < 1 || filter > 2 || u < 1 || u > 3 ||
                        v < 1 || v > 3 || mip > 2 || clamp > 1 || compare > 8 ||
                        (clamp && minor < 8) || (compare && minor < 13))
                        return MXSB_ERR_BINDING;
                }
                for (operand = 0; operand < operands; operand++) {
                    uint32_t source_word = operand >= base_operands ? 9u
                                           : operand == 0           ? (bound ? 5u : 4u)
                                                          : (extended ? 6u : 5u) + operand;
                    uint32_t value = mx_r32(bytes, (cursor + source_word) * 4u);
                    uint32_t dimensions = kind == MXSB_BINDING_TEXTURE_CUBE ? 3u : 2u;
                    uint32_t lanes = operand == 0 ? dimensions + (compare != 0)
                                     : operand < base_operands && gradient
                                         ? (extended ? dimensions : 2u)
                                         : 1u;
                    uint32_t expected = lanes == 1   ? MXSB_TYPE_F32
                                        : lanes == 2 ? MXSB_TYPE_F32X2
                                        : lanes == 3 ? MXSB_TYPE_F32X3
                                                     : MXSB_TYPE_F32X4;
                    uint32_t prior = block_base + 4, matched = 0;
                    while (prior < cursor) {
                        uint32_t prior_head = mx_r32(bytes, prior * 4u),
                                 prior_count = prior_head >> 16;
                        if (opcode_has_result((uint16_t)prior_head) &&
                            mx_r32(bytes, (prior + 1) * 4u) == value) {
                            if (mx_r32(bytes, (prior + 2) * 4u) != expected)
                                return MXSB_ERR_TYPE;
                            matched = 1;
                        }
                        prior += prior_count;
                    }
                    if (!matched)
                        return MXSB_ERR_VALUE;
                }
            }
            if (opcode == MXSB_OP_RETURN_VALUE) {
                uint32_t value = mx_r32(bytes, (cursor + 1) * 4u);
                uint32_t prior = block_base + 4u;
                uint32_t matched = 0;
                if (r + 1u != record_count)
                    return MXSB_ERR_STRUCTURE;
                if (stage == MXSB_STAGE_COMPUTE)
                    return MXSB_ERR_TYPE;
                if (return_precedes(bytes, blocks_base, block_base, entry, MXSB_OP_RETURN_VOID))
                    return MXSB_ERR_TYPE;
                while (prior < cursor) {
                    uint32_t prior_head = mx_r32(bytes, prior * 4u);
                    if (opcode_has_result((uint16_t)prior_head) &&
                        mx_r32(bytes, (prior + 1u) * 4u) == value) {
                        if (mx_r32(bytes, (prior + 2u) * 4u) != MXSB_TYPE_F32X4)
                            return MXSB_ERR_TYPE;
                        matched = 1;
                    }
                    prior += prior_head >> 16;
                }
                if (!matched)
                    return MXSB_ERR_VALUE;
            } else if (opcode == MXSB_OP_RETURN_VOID || opcode == MXSB_OP_DISCARD) {
                if (r + 1u != record_count)
                    return MXSB_ERR_STRUCTURE;
                if (stage != MXSB_STAGE_FRAGMENT &&
                    (opcode == MXSB_OP_DISCARD || stage != MXSB_STAGE_COMPUTE))
                    return MXSB_ERR_STAGE;
                if (opcode == MXSB_OP_RETURN_VOID && stage == MXSB_STAGE_FRAGMENT) {
                    if (minor < 27u)
                        return MXSB_ERR_VERSION;
                    if (return_precedes(bytes, blocks_base, block_base, entry,
                                        MXSB_OP_RETURN_VALUE))
                        return MXSB_ERR_TYPE;
                }
            } else if (r + 1u == record_count) {
                return MXSB_ERR_STRUCTURE;
            }
            cursor += count;
        }
        if (cursor != block_base + block_words)
            return MXSB_ERR_LENGTH;
    }
    if (cursor != words)
        return MXSB_ERR_LENGTH;
    return MXSB_OK;
}

static int writer_valid(const struct mxsb_writer *writer)
{
    uint32_t body;
    if (!writer || !writer->words || writer->capacity < MXSB_HEADER_WORDS ||
        writer->capacity > UINT32_MAX / sizeof(uint32_t) || writer->count < MXSB_HEADER_WORDS ||
        writer->count > writer->capacity || writer->minor > MXSB_VERSION_MINOR || writer->failed)
        return 0;
    body = writer->count - MXSB_HEADER_WORDS;
    if (writer->binding_count > body / MXSB_BINDING_WORDS)
        return 0;
    body -= writer->binding_count * MXSB_BINDING_WORDS;
    if (writer->entry_count > body / MXSB_ENTRY_WORDS)
        return 0;
    body -= writer->entry_count * MXSB_ENTRY_WORDS;
    return writer->block_count <= body / 4u;
}

int mxsb_writer_init(struct mxsb_writer *writer, uint32_t *words, uint32_t capacity, uint16_t minor)
{
    if (!writer || !words || capacity < MXSB_HEADER_WORDS ||
        capacity > UINT32_MAX / sizeof(uint32_t) || minor > MXSB_VERSION_MINOR)
        return MXSB_ERR_LIMIT;
    memset(words, 0, capacity * sizeof(uint32_t));
    writer->words = words;
    writer->count = MXSB_HEADER_WORDS;
    writer->capacity = capacity;
    writer->minor = minor;
    writer->binding_count = 0;
    writer->entry_count = 0;
    writer->block_count = 0;
    writer->failed = 0;
    words[0] = MXSB_MAGIC;
    words[1] = ((uint32_t)MXSB_VERSION_MAJOR << 16) | minor;
    return MXSB_OK;
}

static int push_words(struct mxsb_writer *writer, const uint32_t *src, uint32_t n)
{
    uint32_t i;
    if (!writer_valid(writer) || (n && !src))
        return MXSB_ERR_LIMIT;
    if (n > writer->capacity - writer->count) {
        writer->failed = 1;
        return MXSB_ERR_LIMIT;
    }
    for (i = 0; i < n; i++)
        writer->words[writer->count++] = src[i];
    return MXSB_OK;
}

int mxsb_writer_binding(struct mxsb_writer *writer, uint32_t id, uint16_t slot, uint32_t kind,
                        uint32_t access, uint32_t value_type, uint32_t element_count)
{
    uint32_t rec[5];
    if (!writer_valid(writer) || writer->entry_count || writer->block_count)
        return MXSB_ERR_STRUCTURE;
    rec[0] = id;
    rec[1] = (uint32_t)slot | (kind << 16) | (access << 20);
    rec[2] = value_type;
    rec[3] = element_count;
    rec[4] = 0;
    if (push_words(writer, rec, 5) != MXSB_OK)
        return MXSB_ERR_LIMIT;
    writer->binding_count++;
    return MXSB_OK;
}

int mxsb_writer_entry(struct mxsb_writer *writer, uint32_t id, uint32_t stage, uint32_t root)
{
    return mxsb_writer_entry_workgroup(writer, id, stage, root, 0, 0, 0);
}

int mxsb_writer_entry_workgroup(struct mxsb_writer *writer, uint32_t id, uint32_t stage,
                                uint32_t root, uint32_t width, uint32_t height, uint32_t depth)
{
    uint32_t rec[7];
    if (!writer_valid(writer) || writer->block_count || id == 0)
        return MXSB_ERR_STRUCTURE;
    rec[0] = id;
    rec[1] = stage;
    rec[2] = root;
    rec[3] = width;
    rec[4] = height;
    rec[5] = depth;
    rec[6] = 0;
    if (push_words(writer, rec, 7) != MXSB_OK)
        return MXSB_ERR_LIMIT;
    writer->entry_count++;
    return MXSB_OK;
}

int mxsb_writer_block(struct mxsb_writer *writer, uint32_t id, uint32_t entry,
                      const uint32_t *const *records, const uint32_t *record_words,
                      uint32_t record_count)
{
    uint32_t header[4];
    uint32_t start;
    uint32_t i;
    uint32_t remaining;
    if (!writer_valid(writer) || !records || !record_words || record_count == 0)
        return MXSB_ERR_STRUCTURE;
    remaining = writer->capacity - writer->count;
    if (remaining < 4u || record_count > remaining - 4u)
        return MXSB_ERR_LIMIT;
    remaining -= 4u;
    for (i = 0; i < record_count; i++) {
        if (!records[i] || !record_words[i])
            return MXSB_ERR_STRUCTURE;
        if (record_words[i] > remaining)
            return MXSB_ERR_LIMIT;
        remaining -= record_words[i];
    }
    start = writer->count;
    header[0] = id;
    header[1] = entry;
    header[2] = 0;
    header[3] = record_count;
    if (push_words(writer, header, 4) != MXSB_OK)
        return MXSB_ERR_LIMIT;
    for (i = 0; i < record_count; i++) {
        if (push_words(writer, records[i], record_words[i]) != MXSB_OK)
            return MXSB_ERR_LIMIT;
    }
    writer->words[start + 2] = writer->count - start;
    writer->block_count++;
    return MXSB_OK;
}

int mxsb_writer_finish(struct mxsb_writer *writer, uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint32_t i;
    struct mxsb_limits limits;
    if (out_len)
        *out_len = 0;
    if (!writer_valid(writer) || writer->entry_count == 0 || writer->block_count == 0)
        return MXSB_ERR_STRUCTURE;
    writer->words[2] = writer->count;
    writer->words[3] = writer->binding_count;
    writer->words[4] = writer->entry_count;
    writer->words[5] = writer->block_count;
    if (!out || cap < writer->count * 4u)
        return MXSB_ERR_LENGTH;
    for (i = 0; i < writer->count; i++)
        mx_w32(out, i * 4u, writer->words[i]);
    mxsb_limits_default(&limits);
    if (mxsb_verify(out, writer->count * 4u, &limits) != MXSB_OK) {
        memset(out, 0, writer->count * 4u);
        return MXSB_ERR_STRUCTURE;
    }
    if (out_len)
        *out_len = writer->count * 4u;
    return MXSB_OK;
}
