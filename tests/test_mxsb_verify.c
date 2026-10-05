/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxsb.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t words[16384];
static uint8_t bytes[sizeof words];
static uint8_t saved[sizeof words];

static void store(uint32_t index, uint32_t value)
{
    bytes[index * 4u] = (uint8_t)value;
    bytes[index * 4u + 1u] = (uint8_t)(value >> 8);
    bytes[index * 4u + 2u] = (uint8_t)(value >> 16);
    bytes[index * 4u + 3u] = (uint8_t)(value >> 24);
}

static uint32_t module(uint32_t bindings, uint32_t entries, uint32_t constants)
{
    uint32_t cursor = MXSB_HEADER_WORDS;
    uint32_t i;
    uint32_t e;
    memset(words, 0, sizeof words);
    words[0] = MXSB_MAGIC;
    words[1] = ((uint32_t)MXSB_VERSION_MAJOR << 16) | MXSB_VERSION_MINOR;
    words[3] = bindings;
    words[4] = entries;
    words[5] = entries;
    for (i = 0; i < bindings; i++) {
        words[cursor++] = i + 1u;
        words[cursor++] = i | (MXSB_BINDING_UNIFORM << 16) | (MXSB_ACCESS_READ << 20);
        words[cursor++] = MXSB_TYPE_F32X4;
        words[cursor++] = 1;
        words[cursor++] = 0;
    }
    for (i = 0; i < entries; i++) {
        words[cursor++] = i + 1u;
        words[cursor++] = MXSB_STAGE_VERTEX;
        words[cursor++] = i + 1u;
        cursor += 4u;
    }
    for (e = 0; e < entries; e++) {
        uint32_t start = cursor;
        words[cursor++] = e + 1u;
        words[cursor++] = e + 1u;
        cursor++;
        words[cursor++] = constants + 2u;
        for (i = 0; i < constants; i++) {
            words[cursor++] = (4u << 16) | MXSB_OP_CONSTANT;
            words[cursor++] = i + 1u;
            words[cursor++] = MXSB_TYPE_F32;
            words[cursor++] = 0;
        }
        words[cursor++] = (8u << 16) | MXSB_OP_CONSTRUCT;
        words[cursor++] = constants + 1u;
        words[cursor++] = MXSB_TYPE_F32X4;
        words[cursor++] = 4;
        for (i = 0; i < 4u; i++)
            words[cursor++] = 1;
        words[cursor++] = (2u << 16) | MXSB_OP_RETURN_VALUE;
        words[cursor++] = constants + 1u;
        words[start + 2u] = cursor - start;
    }
    assert(cursor <= sizeof words / sizeof words[0]);
    words[2] = cursor;
    for (i = 0; i < cursor; i++)
        store(i, words[i]);
    return cursor * 4u;
}

static void verify_readonly(uint32_t len, const struct mxsb_limits *limits, int expected)
{
    memcpy(saved, bytes, sizeof bytes);
    assert(mxsb_verify(bytes, len, limits) == expected);
    assert(memcmp(saved, bytes, sizeof bytes) == 0);
}

static void test_writer_bounds(void)
{
    struct mxsb_writer writer;
    struct mxsb_writer before;
    uint32_t record[] = {(2u << 16) | MXSB_OP_RETURN_VALUE, 1};
    const uint32_t *records[] = {record};
    uint32_t counts[] = {2};
    uint32_t out_len = 9;
    uint32_t count;
    memset(&writer, 0xa5, sizeof writer);
    before = writer;
    memset(words, 0xa5, sizeof words);
    memcpy(saved, words, sizeof words);
    assert(mxsb_writer_init(&writer, words, UINT32_MAX, MXSB_VERSION_MINOR) == MXSB_ERR_LIMIT);
    assert(!memcmp(&writer, &before, sizeof writer) && !memcmp(saved, words, sizeof words));
    assert(mxsb_writer_init(&writer, words, UINT32_MAX / 4u + 1u, MXSB_VERSION_MINOR) ==
           MXSB_ERR_LIMIT);
    assert(!memcmp(&writer, &before, sizeof writer) && !memcmp(saved, words, sizeof words));
    assert(mxsb_writer_init(&writer, words, 128, MXSB_VERSION_MINOR) == MXSB_OK);
    assert(mxsb_writer_entry(&writer, 1, MXSB_STAGE_VERTEX, 1) == MXSB_OK);
    before = writer;
    memcpy(saved, words, sizeof words);
    records[0] = NULL;
    assert(mxsb_writer_block(&writer, 1, 1, records, counts, 1) == MXSB_ERR_STRUCTURE);
    assert(!memcmp(&writer, &before, sizeof writer) && !memcmp(saved, words, sizeof words));
    records[0] = record;
    counts[0] = UINT32_MAX;
    assert(mxsb_writer_block(&writer, 1, 1, records, counts, 1) == MXSB_ERR_LIMIT);
    assert(!memcmp(&writer, &before, sizeof writer) && !memcmp(saved, words, sizeof words));
    counts[0] = 2;
    assert(mxsb_writer_block(&writer, 1, 1, records, counts, UINT32_MAX) == MXSB_ERR_LIMIT);
    assert(!memcmp(&writer, &before, sizeof writer) && !memcmp(saved, words, sizeof words));
    writer.capacity = UINT32_MAX / 4u;
    writer.count = writer.capacity - 1u;
    count = writer.count;
    assert(mxsb_writer_binding(&writer, 1, 0, MXSB_BINDING_UNIFORM, MXSB_ACCESS_READ,
                               MXSB_TYPE_F32X4, 1) == MXSB_ERR_STRUCTURE);
    writer.entry_count = 0;
    assert(mxsb_writer_binding(&writer, 1, 0, MXSB_BINDING_UNIFORM, MXSB_ACCESS_READ,
                               MXSB_TYPE_F32X4, 1) == MXSB_ERR_LIMIT);
    assert(writer.failed && writer.count == count && !memcmp(saved, words, sizeof words));
    writer = before;
    writer.count = UINT32_MAX;
    writer.capacity = UINT32_MAX;
    memset(bytes, 0xa5, sizeof bytes);
    assert(mxsb_writer_finish(&writer, bytes, sizeof bytes, &out_len) == MXSB_ERR_STRUCTURE);
    assert(out_len == 0 && bytes[0] == 0xa5 && !memcmp(saved, words, sizeof words));
    writer = before;
    writer.count = writer.capacity + 1u;
    assert(mxsb_writer_entry(&writer, 2, MXSB_STAGE_VERTEX, 2) == MXSB_ERR_STRUCTURE);
    writer = before;
    writer.words = NULL;
    assert(mxsb_writer_block(&writer, 1, 1, records, counts, 1) == MXSB_ERR_STRUCTURE);
}

int main(void)
{
    struct mxsb_limits limits;
    uint32_t len;
    uint32_t block;
    uint32_t construct;
    uint32_t i;
    test_writer_bounds();
    assert(mxsb_limits_default(&limits) == MXSB_OK);
    len = module(129, 65, 1);
    verify_readonly(len, &limits, MXSB_ERR_LIMIT);
    limits.max_bindings = UINT32_MAX;
    limits.max_entry_points = UINT32_MAX;
    limits.max_blocks = UINT32_MAX;
    limits.max_instructions = UINT32_MAX;
    verify_readonly(len, &limits, MXSB_OK);
    store(3, UINT32_MAX);
    verify_readonly(len, &limits, MXSB_ERR_LENGTH);
    len = module(0, 1, 300);
    verify_readonly(len, &limits, MXSB_OK);
    assert(words[1] == (((uint32_t)MXSB_VERSION_MAJOR << 16) | 72u));
    block = MXSB_HEADER_WORDS + MXSB_ENTRY_WORDS;
    store(block + 4u, (4u << 16) | 196u);
    verify_readonly(len, &limits, MXSB_ERR_OPCODE);
    store(block + 4u, words[block + 4u]);
    /* Both conflicting definitions follow 256 distinct values. */
    store(block + 4u + 299u * 4u + 1u, 277);
    verify_readonly(len, &limits, MXSB_ERR_VALUE);
    len = module(0, 2, 1);
    store(MXSB_HEADER_WORDS + 2u * MXSB_ENTRY_WORDS +
              words[MXSB_HEADER_WORDS + 2u * MXSB_ENTRY_WORDS + 2u] + 1u,
          1);
    verify_readonly(len, &limits, MXSB_ERR_VALUE);
    len = module(0, 1, 300);
    limits.max_instructions = 301;
    verify_readonly(len, &limits, MXSB_ERR_LIMIT);
    limits.max_instructions = 302;
    verify_readonly(len, &limits, MXSB_OK);
    len = module(0, 2, 1);
    limits.max_instructions = 5;
    verify_readonly(len, &limits, MXSB_ERR_LIMIT);
    limits.max_instructions = UINT32_MAX;
    len = module(0, 1, 1);
    construct = block + 8u;
    store(construct + 2u, MXSB_TYPE_F32X2);
    verify_readonly(len, &limits, MXSB_ERR_TYPE);
    len = module(0, 1, 1);
    store(len / 4u - 1u, 900);
    verify_readonly(len, &limits, MXSB_ERR_VALUE);
    len = module(0, 1, 1);
    store(len / 4u - 1u, 1);
    verify_readonly(len, &limits, MXSB_ERR_TYPE);
    len = module(0, 1, 1);
    store(block + 4u, (1u << 16) | MXSB_OP_CONSTRUCT);
    verify_readonly(len, &limits, MXSB_ERR_STRUCTURE);
    len = module(0, 1, 1);
    store(block + 2u, UINT32_MAX);
    verify_readonly(len, &limits, MXSB_ERR_LENGTH);
    len = module(0, 1, 1);
    store(block + 3u, UINT32_MAX);
    verify_readonly(len, &limits, MXSB_ERR_LENGTH);
    len = module(0, 1, 1);
    store(block + 8u, (2u << 16) | MXSB_OP_RETURN_VALUE);
    verify_readonly(len, &limits, MXSB_ERR_STRUCTURE);
    len = module(0, 1, 1);
    for (i = 0; i < len; i++) {
        assert(mxsb_verify(bytes, i, &limits) != MXSB_OK);
    }
    len = module(0, 1, 1);
    store(MXSB_HEADER_WORDS + 1u, MXSB_STAGE_FRAGMENT);
    store(block + 2u, 5);
    store(block + 3u, 1);
    store(block + 4u, (1u << 16) | MXSB_OP_RETURN_VOID);
    len = (block + 5u) * 4u;
    store(2, len / 4u);
    verify_readonly(len, &limits, MXSB_OK);
    store(1, ((uint32_t)MXSB_VERSION_MAJOR << 16) | 26u);
    verify_readonly(len, &limits, MXSB_ERR_VERSION);
    store(1, ((uint32_t)MXSB_VERSION_MAJOR << 16) | MXSB_VERSION_MINOR);
    store(MXSB_HEADER_WORDS + 1u, MXSB_STAGE_VERTEX);
    verify_readonly(len, &limits, MXSB_ERR_STAGE);
    {
        struct mxsb_writer writer;
        uint32_t record[] = {(2u << 16) | MXSB_OP_RETURN_VALUE, 99};
        const uint32_t *records[] = {record};
        uint32_t counts[] = {2};
        uint32_t out_len = 99;
        assert(mxsb_writer_init(&writer, words, 128, MXSB_VERSION_MINOR) == MXSB_OK);
        assert(mxsb_writer_entry(&writer, 1, MXSB_STAGE_VERTEX, 1) == MXSB_OK);
        assert(mxsb_writer_block(&writer, 1, 1, records, counts, 1) == MXSB_OK);
        memset(bytes, 0xa5, sizeof bytes);
        assert(mxsb_writer_finish(&writer, bytes, sizeof bytes, &out_len) == MXSB_ERR_STRUCTURE);
        assert(out_len == 0);
        for (i = 0; i < writer.count * 4u; i++)
            assert(bytes[i] == 0);
        for (; i < sizeof bytes; i++)
            assert(bytes[i] == 0xa5);
    }
    puts(
        "MXSB elevated metadata limits, complete result uniqueness, typed returns, record bounds and readonly refusal PASS");
    return 0;
}
