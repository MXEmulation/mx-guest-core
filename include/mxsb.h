/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#ifndef MXSB_H
#define MXSB_H

#include <stddef.h>
#include <stdint.h>

#include "mx_versions.h"

#define MXSB_HEADER_WORDS 8u
#define MXSB_BINDING_WORDS 5u
#define MXSB_ENTRY_WORDS 7u
#define MXSB_OP_CONSTANT 1u
#define MXSB_OP_BUILTIN 2u
#define MXSB_OP_ADD 3u
#define MXSB_OP_SUB 4u
#define MXSB_OP_MUL 5u
#define MXSB_OP_DIV 6u
#define MXSB_OP_NEG 7u
#define MXSB_OP_NOT 8u
#define MXSB_OP_EQ 9u
#define MXSB_OP_LT 10u
#define MXSB_OP_LE 11u
#define MXSB_OP_SELECT 12u
#define MXSB_OP_EXTRACT 14u
#define MXSB_OP_BUFFER_LOAD 15u
#define MXSB_OP_CONSTRUCT 13u
#define MXSB_OP_FABS 36u
#define MXSB_OP_FLOOR 37u
#define MXSB_OP_CEIL 38u
#define MXSB_OP_FRACT 41u
#define MXSB_OP_SQRT 42u
#define MXSB_OP_RSQ 43u
#define MXSB_OP_RCP 44u
#define MXSB_OP_EXP2 45u
#define MXSB_OP_LOG2 46u
#define MXSB_OP_SIN 47u
#define MXSB_OP_COS 48u
#define MXSB_OP_BIT_AND 50u
#define MXSB_OP_BIT_OR 51u
#define MXSB_OP_BIT_XOR 52u
#define MXSB_OP_BIT_NOT 53u
#define MXSB_OP_SHL 54u
#define MXSB_OP_SHR 55u
#define MXSB_OP_IDIV 56u
#define MXSB_OP_IMOD 57u
#define MXSB_OP_I2F 58u
#define MXSB_OP_U2F 59u
#define MXSB_OP_F2I 60u
#define MXSB_OP_F2U 61u
#define MXSB_OP_BITCAST 62u
#define MXSB_OP_STAGE_INPUT 21u
#define MXSB_OP_STAGE_OUTPUT 22u
#define MXSB_OP_TEXTURE_SAMPLE 23u
#define MXSB_OP_TEXTURE_SAMPLE_BOUND 35u
#define MXSB_OP_TEXTURE_SAMPLE_LOD 65u
#define MXSB_OP_TEXTURE_SAMPLE_BOUND_LOD 66u
#define MXSB_OP_TEXTURE_SAMPLE_GRAD 67u
#define MXSB_OP_TEXTURE_SAMPLE_BOUND_GRAD 68u
#define MXSB_OP_TEXTURE_SAMPLE_BOUND_OPERANDS 186u
#define MXSB_OP_DDX 116u
#define MXSB_OP_DDY 117u
#define MXSB_OP_FWIDTH 118u
#define MXSB_OP_RETURN_VOID 0x100u
#define MXSB_OP_RETURN_VALUE 0x101u
#define MXSB_OP_DISCARD 0x105u
#define MXSB_TYPE_BOOL 1u
#define MXSB_TYPE_U32 3u
#define MXSB_TYPE_F32 4u
#define MXSB_TYPE_F32X2 11u
#define MXSB_TYPE_F32X3 12u
#define MXSB_TYPE_F32X4 13u
#define MXSB_STAGE_VERTEX 2u
#define MXSB_STAGE_FRAGMENT 3u
#define MXSB_BINDING_UNIFORM 1u
#define MXSB_BINDING_TEXTURE_2D 3u
#define MXSB_BINDING_SAMPLER 4u
#define MXSB_BINDING_TEXTURE_CUBE 8u
#define MXSB_ACCESS_READ 1u
#define MXSB_BUILTIN_VERTEX_ID 4u
#define MXSB_INTERP_PERSPECTIVE 1u

enum mxsb_status {
    MXSB_OK = 0,
    MXSB_ERR_MAGIC = 1,
    MXSB_ERR_LENGTH = 2,
    MXSB_ERR_VERSION = 3,
    MXSB_ERR_RESERVED = 4,
    MXSB_ERR_OPCODE = 5,
    MXSB_ERR_STRUCTURE = 6,
    MXSB_ERR_TYPE = 7,
    MXSB_ERR_BINDING = 8,
    MXSB_ERR_STAGE = 9,
    MXSB_ERR_VALUE = 10,
    MXSB_ERR_LIMIT = 11
};

struct mxsb_limits {
    uint32_t max_words;
    uint32_t max_bindings;
    uint32_t max_entry_points;
    uint32_t max_blocks;
    uint32_t max_instructions;
};

struct mxsb_writer {
    uint32_t *words;
    uint32_t count;
    uint32_t capacity;
    uint16_t minor;
    uint32_t binding_count;
    uint32_t entry_count;
    uint32_t block_count;
    int failed;
};

int mxsb_limits_default(struct mxsb_limits *out);
int mxsb_verify(const uint8_t *bytes, uint32_t len, const struct mxsb_limits *limits);
int mxsb_writer_init(struct mxsb_writer *writer, uint32_t *words, uint32_t capacity,
                     uint16_t minor);
int mxsb_writer_binding(struct mxsb_writer *writer, uint32_t id, uint16_t slot, uint32_t kind,
                        uint32_t access, uint32_t value_type, uint32_t element_count);
int mxsb_writer_entry(struct mxsb_writer *writer, uint32_t id, uint32_t stage, uint32_t root);
int mxsb_writer_block(struct mxsb_writer *writer, uint32_t id, uint32_t entry,
                      const uint32_t *const *records, const uint32_t *record_words,
                      uint32_t record_count);
int mxsb_writer_finish(struct mxsb_writer *writer, uint8_t *out, uint32_t cap, uint32_t *out_len);

#endif
