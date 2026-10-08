/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mx_le.h"
#include "mxgpu_wire.h"
#include "mxsb.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define REC(count, opcode) (((uint32_t)(count) << 16) | (opcode))
#define COUNT(record) ((uint32_t)(sizeof(record) / sizeof((record)[0])))

static struct mxgpu_execution_binding buffer_binding(uint16_t slot, uint32_t id)
{
    struct mxgpu_execution_binding b;
    memset(&b, 0, sizeof b);
    b.slot = slot;
    b.access = MXGPU_BIND_ACCESS_READ_WRITE;
    b.kind = MXGPU_BIND_KIND_BUFFER;
    b.resource_id = id;
    b.offset = 16;
    b.size = 256;
    return b;
}

static struct mxgpu_execution_binding texture_binding(uint16_t slot, uint32_t id)
{
    struct mxgpu_execution_binding b;
    memset(&b, 0, sizeof b);
    b.slot = slot;
    b.access = MXGPU_BIND_ACCESS_WRITE;
    b.kind = MXGPU_BIND_KIND_TEXTURE_2D;
    b.resource_id = id;
    return b;
}

static int decode_wire(const uint8_t *wire, uint32_t len, uint32_t cap)
{
    struct mxgpu_compute_submit out;
    struct mxgpu_execution_binding got[4];
    return mxgpu_compute_submit_decode(wire, len, &out, got, cap);
}

static void test_compute_submit(void)
{
    struct mxgpu_compute_submit in, out;
    struct mxgpu_execution_binding bindings[2], got[2], dup[2];
    uint8_t wire[256], mutated[256], zeros[12];
    uint32_t len = 99;
    uint32_t i;

    memset(&in, 0, sizeof in);
    memset(zeros, 0, sizeof zeros);
    bindings[0] = buffer_binding(0, 10);
    bindings[1] = texture_binding(1, 11);
    in.pipeline_id = 7;
    in.binding_count = 2;
    in.dispatch_kind = MXGPU_DISPATCH_THREADGROUPS;
    in.dimensions[0] = 4;
    in.dimensions[1] = 5;
    in.dimensions[2] = 6;

    assert(MXGPU_OP_COMPUTE_SUBMIT == 0x0200u && MXGPU_COMPUTE_SUBMIT_HEADER_SIZE == 32u);
    assert(MXGPU_DISPATCH_THREADS == 1u && MXGPU_DISPATCH_THREADGROUPS == 2u);
    assert(mxgpu_compute_submit_encode(&in, bindings, wire, sizeof wire, &len) == MX_OK);
    assert(len == MXGPU_COMPUTE_SUBMIT_HEADER_SIZE + 2u * MXGPU_EXECUTION_BINDING_SIZE);
    assert(mx_r32(wire, 0) == 7 && mx_r16(wire, 4) == 2 && mx_r16(wire, 6) == 2);
    assert(mx_r32(wire, 8) == 4 && mx_r32(wire, 12) == 5 && mx_r32(wire, 16) == 6);
    assert(memcmp(wire + 20, zeros, 12) == 0);
    assert(mx_r16(wire, 32 + 2) == MXGPU_BIND_ACCESS_READ_WRITE);
    assert(mx_r16(wire, 64 + 2) == MXGPU_BIND_ACCESS_WRITE);

    memset(&out, 0xa5, sizeof out);
    assert(mxgpu_compute_submit_decode(wire, len, &out, got, 2) == MX_OK);
    assert(out.pipeline_id == 7 && out.binding_count == 2 &&
           out.dispatch_kind == MXGPU_DISPATCH_THREADGROUPS);
    assert(out.dimensions[0] == 4 && out.dimensions[1] == 5 && out.dimensions[2] == 6);
    assert(memcmp(&got[0], &bindings[0], sizeof got[0]) == 0);
    assert(memcmp(&got[1], &bindings[1], sizeof got[1]) == 0);

    in.binding_count = 0;
    in.dispatch_kind = MXGPU_DISPATCH_THREADS;
    assert(mxgpu_compute_submit_encode(&in, NULL, wire, sizeof wire, &len) == MX_OK &&
           len == MXGPU_COMPUTE_SUBMIT_HEADER_SIZE);
    assert(mxgpu_compute_submit_decode(wire, len, &out, NULL, 0) == MX_OK &&
           out.binding_count == 0 && out.dispatch_kind == MXGPU_DISPATCH_THREADS);
    in.binding_count = 2;
    in.dispatch_kind = MXGPU_DISPATCH_THREADGROUPS;

    in.pipeline_id = 0;
    len = 99;
    assert(mxgpu_compute_submit_encode(&in, bindings, wire, sizeof wire, &len) == MX_ERR_RESOURCE &&
           len == 0);
    in.pipeline_id = 7;
    for (i = 0; i < 3; i++) {
        in.dimensions[i] = 0;
        assert(mxgpu_compute_submit_encode(&in, bindings, wire, sizeof wire, &len) ==
               MX_ERR_RANGE);
        in.dimensions[i] = 4;
    }
    in.dimensions[0] = 0xffffffffu;
    in.dimensions[1] = 0xffffffffu;
    in.dimensions[2] = 2;
    assert(mxgpu_compute_submit_encode(&in, bindings, wire, sizeof wire, &len) == MX_ERR_RANGE);
    in.dimensions[2] = 1;
    assert(mxgpu_compute_submit_encode(&in, bindings, wire, sizeof wire, &len) == MX_OK);
    in.dimensions[0] = 4;
    in.dimensions[1] = 5;
    in.dimensions[2] = 6;
    in.dispatch_kind = 0;
    assert(mxgpu_compute_submit_encode(&in, bindings, wire, sizeof wire, &len) == MX_ERR_STATE);
    in.dispatch_kind = 3;
    assert(mxgpu_compute_submit_encode(&in, bindings, wire, sizeof wire, &len) == MX_ERR_STATE);
    in.dispatch_kind = MXGPU_DISPATCH_THREADGROUPS;
    dup[0] = bindings[0];
    dup[1] = bindings[1];
    dup[1].slot = dup[0].slot;
    assert(mxgpu_compute_submit_encode(&in, dup, wire, sizeof wire, &len) == MX_ERR_BINDING);
    dup[1] = bindings[1];
    dup[1].resource_id = 0;
    assert(mxgpu_compute_submit_encode(&in, dup, wire, sizeof wire, &len) == MX_ERR_BINDING);
    assert(mxgpu_compute_submit_encode(&in, NULL, wire, sizeof wire, &len) == MX_ERR_STATE);
    assert(mxgpu_compute_submit_encode(&in, bindings, wire, 95, &len) == MX_ERR_LENGTH &&
           len == 0);
    assert(mxgpu_compute_submit_encode(NULL, bindings, wire, sizeof wire, &len) == MX_ERR_RESOURCE);

    assert(mxgpu_compute_submit_encode(&in, bindings, wire, sizeof wire, &len) == MX_OK);
    assert(decode_wire(wire, len, 2) == MX_OK);
    assert(decode_wire(wire, len, 1) == MX_ERR_LIMIT);
    assert(mxgpu_compute_submit_decode(wire, len, &out, NULL, 2) == MX_ERR_LIMIT);
    assert(decode_wire(wire, 31, 2) == MX_ERR_LENGTH);
    assert(decode_wire(wire, len - 1, 2) == MX_ERR_LENGTH);
    assert(decode_wire(wire, len + 1, 2) == MX_ERR_LENGTH);
    assert(mxgpu_compute_submit_decode(NULL, len, &out, got, 2) == MX_ERR_STATE);

    memcpy(mutated, wire, len);
    mx_w32(mutated, 0, 0);
    assert(decode_wire(mutated, len, 2) == MX_ERR_RESOURCE);
    for (i = 0; i < 3; i++) {
        memcpy(mutated, wire, len);
        mx_w32(mutated, 8 + i * 4, 0);
        assert(decode_wire(mutated, len, 2) == MX_ERR_RANGE);
    }
    memcpy(mutated, wire, len);
    mx_w32(mutated, 8, 0xffffffffu);
    mx_w32(mutated, 12, 0xffffffffu);
    mx_w32(mutated, 16, 2);
    assert(decode_wire(mutated, len, 2) == MX_ERR_RANGE);
    for (i = 0; i < 3; i++) {
        memcpy(mutated, wire, len);
        mx_w32(mutated, 20 + i * 4, 1);
        assert(decode_wire(mutated, len, 2) == MX_ERR_RESERVED);
    }
    for (i = 0; i < 5; i += 3) {
        memcpy(mutated, wire, len);
        mx_w16(mutated, 6, (uint16_t)i);
        assert(decode_wire(mutated, len, 2) == MX_ERR_STATE);
    }
    memcpy(mutated, wire, len);
    mx_w16(mutated, 32, 1);
    assert(decode_wire(mutated, len, 2) == MX_ERR_BINDING);
    memcpy(mutated, wire, len);
    mx_w16(mutated, 32 + 4, 99);
    assert(decode_wire(mutated, len, 2) == MX_ERR_BINDING);
    memcpy(mutated, wire, len);
    mx_w16(mutated, 32 + 2, 4);
    assert(decode_wire(mutated, len, 2) == MX_ERR_FLAGS);
    memcpy(mutated, wire, len);
    mx_w16(mutated, 4, 3);
    assert(decode_wire(mutated, len, 4) == MX_ERR_LENGTH);

    /* A refused decode leaves the output record untouched. */
    memset(&out, 0x5a, sizeof out);
    memcpy(mutated, wire, len);
    mx_w32(mutated, 20, 1);
    assert(mxgpu_compute_submit_decode(mutated, len, &out, got, 2) == MX_ERR_RESERVED);
    assert(out.pipeline_id == 0x5a5a5a5au);
}

static void test_compute_command(void)
{
    struct mxgpu_command_header header, decoded;
    struct mxgpu_compute_submit in;
    struct mxgpu_execution_binding binding = buffer_binding(0, 10);
    uint8_t payload[64], command[128];
    const uint8_t *body;
    uint32_t payload_len, command_len, body_len;
    uint16_t queue = 0;

    memset(&in, 0, sizeof in);
    in.pipeline_id = 3;
    in.binding_count = 1;
    in.dispatch_kind = MXGPU_DISPATCH_THREADS;
    in.dimensions[0] = 1024;
    in.dimensions[1] = 1;
    in.dimensions[2] = 1;
    assert(mxgpu_compute_submit_encode(&in, &binding, payload, sizeof payload, &payload_len) ==
           MX_OK);
    assert(mxgpu_opcode_queue(MXGPU_OP_COMPUTE_SUBMIT, &queue) == MX_OK &&
           queue == MXGPU_QUEUE_COMPUTE);

    memset(&header, 0, sizeof header);
    header.opcode = MXGPU_OP_COMPUTE_SUBMIT;
    header.context_id = 1;
    header.queue = MXGPU_QUEUE_COMPUTE;
    header.sequence = 1;
    assert(mxgpu_command_encode(&header, payload, payload_len, 4096, command, sizeof command,
                                &command_len) == MX_OK);
    assert(mxgpu_command_decode(command, command_len, 4096, &decoded, &body, &body_len) == MX_OK);
    assert(decoded.opcode == MXGPU_OP_COMPUTE_SUBMIT && body_len == payload_len);
    assert(memcmp(body, payload, payload_len) == 0);
    header.queue = MXGPU_QUEUE_RENDER;
    assert(mxgpu_command_encode(&header, payload, payload_len, 4096, command, sizeof command,
                                &command_len) == MX_ERR_QUEUE);

    assert(mxgpu_compute_submit_features(0) == MX_ERR_FEATURE);
    assert(mxgpu_compute_submit_features(MXGPU_FEAT_RENDER) == MX_ERR_FEATURE);
    assert(mxgpu_compute_submit_features(MXGPU_FEAT_COMPUTE | MXGPU_FEAT_RENDER) == MX_OK);
}

/* One binding, one entry and one block assembled word by word. */
struct spec {
    uint16_t minor;
    uint32_t stage;
    uint32_t workgroup[3];
    uint32_t kind;
    uint32_t access;
    uint32_t value_type;
    uint32_t elements;
    const uint32_t *const *records;
    const uint32_t *counts;
    uint32_t record_count;
};

static uint32_t words[1024];
static uint8_t bytes[sizeof words];

static int run(const struct spec *s)
{
    struct mxsb_limits limits;
    uint32_t n = MXSB_HEADER_WORDS;
    uint32_t block;
    uint32_t i;
    uint32_t j;
    memset(words, 0, sizeof words);
    words[0] = MXSB_MAGIC;
    words[1] = ((uint32_t)MXSB_VERSION_MAJOR << 16) | s->minor;
    words[3] = 1;
    words[4] = 1;
    words[5] = 1;
    words[n++] = 1;
    words[n++] = (s->kind << 16) | (s->access << 20);
    words[n++] = s->value_type;
    words[n++] = s->elements;
    words[n++] = 0;
    words[n++] = 1;
    words[n++] = s->stage;
    words[n++] = 1;
    words[n++] = s->workgroup[0];
    words[n++] = s->workgroup[1];
    words[n++] = s->workgroup[2];
    words[n++] = 0;
    block = n;
    words[n++] = 1;
    words[n++] = 1;
    n++;
    words[n++] = s->record_count;
    for (i = 0; i < s->record_count; i++) {
        for (j = 0; j < s->counts[i]; j++)
            words[n++] = s->records[i][j];
    }
    words[block + 2] = n - block;
    words[2] = n;
    for (i = 0; i < n; i++)
        mx_w32(bytes, i * 4u, words[i]);
    assert(mxsb_limits_default(&limits) == MXSB_OK);
    return mxsb_verify(bytes, n * 4u, &limits);
}

static struct spec compute_spec(const uint32_t *const *records, const uint32_t *counts,
                                uint32_t record_count)
{
    struct spec s;
    memset(&s, 0, sizeof s);
    s.minor = MXSB_VERSION_MINOR;
    s.stage = MXSB_STAGE_COMPUTE;
    s.workgroup[0] = 64;
    s.workgroup[1] = 1;
    s.workgroup[2] = 1;
    s.kind = MXSB_BINDING_STORAGE;
    s.access = MXSB_ACCESS_READ_WRITE;
    s.value_type = MXSB_TYPE_U32;
    s.elements = 16;
    s.records = records;
    s.counts = counts;
    s.record_count = record_count;
    return s;
}

/* Runs a module of one or two records over the given spec. */
static int run_pair(struct spec s, const uint32_t *a, uint32_t an, const uint32_t *b, uint32_t bn)
{
    const uint32_t *records[2];
    uint32_t counts[2];
    records[0] = a;
    records[1] = b;
    counts[0] = an;
    counts[1] = bn;
    s.records = records;
    s.counts = counts;
    s.record_count = b ? 2u : 1u;
    return run(&s);
}

static const uint32_t r_global[] = {REC(4, MXSB_OP_BUILTIN), 10, MXSB_TYPE_U32X3,
                                    MXSB_BUILTIN_GLOBAL_INVOCATION_ID};
static const uint32_t r_index[] = {REC(5, MXSB_OP_EXTRACT), 11, MXSB_TYPE_U32, 10, 0};
static const uint32_t r_value[] = {REC(4, MXSB_OP_CONSTANT), 12, MXSB_TYPE_U32, 7};
static const uint32_t r_store[] = {REC(4, MXSB_OP_BUFFER_STORE), 1, 11, 12};
static const uint32_t r_return[] = {REC(1, MXSB_OP_RETURN_VOID)};
static const uint32_t *const store_records[] = {r_global, r_index, r_value, r_store, r_return};
static const uint32_t store_counts[] = {4, 5, 4, 4, 1};

static void test_writer_module(void)
{
    struct mxsb_writer writer;
    uint32_t module[256];
    uint8_t out[1024];
    uint32_t out_len = 0;
    uint32_t entry = (MXSB_HEADER_WORDS + MXSB_BINDING_WORDS) * 4u;
    struct mxsb_limits limits;

    assert(mxsb_limits_default(&limits) == MXSB_OK);
    assert(mxsb_writer_init(&writer, module, 256, MXSB_VERSION_MINOR) == MXSB_OK);
    assert(mxsb_writer_binding(&writer, 1, 0, MXSB_BINDING_STORAGE, MXSB_ACCESS_READ_WRITE,
                               MXSB_TYPE_U32, 16) == MXSB_OK);
    assert(mxsb_writer_entry_workgroup(&writer, 1, MXSB_STAGE_COMPUTE, 1, 64, 1, 1) == MXSB_OK);
    assert(mxsb_writer_block(&writer, 1, 1, store_records, store_counts, 5) == MXSB_OK);
    assert(mxsb_writer_finish(&writer, out, sizeof out, &out_len) == MXSB_OK && out_len > 0);
    assert(mxsb_verify(out, out_len, &limits) == MXSB_OK);
    assert(mx_r32(out, entry + 4) == MXSB_STAGE_COMPUTE);
    assert(mx_r32(out, entry + 12) == 64 && mx_r32(out, entry + 16) == 1 &&
           mx_r32(out, entry + 20) == 1);

    /* A zero workgroup axis is refused when the module is finished. */
    assert(mxsb_writer_init(&writer, module, 256, MXSB_VERSION_MINOR) == MXSB_OK);
    assert(mxsb_writer_binding(&writer, 1, 0, MXSB_BINDING_STORAGE, MXSB_ACCESS_READ_WRITE,
                               MXSB_TYPE_U32, 16) == MXSB_OK);
    assert(mxsb_writer_entry_workgroup(&writer, 1, MXSB_STAGE_COMPUTE, 1, 64, 0, 1) == MXSB_OK);
    assert(mxsb_writer_block(&writer, 1, 1, store_records, store_counts, 5) == MXSB_OK);
    assert(mxsb_writer_finish(&writer, out, sizeof out, &out_len) == MXSB_ERR_STRUCTURE);
    assert(out_len == 0);

    /* The original entry function still writes zero workgroup words. */
    assert(mxsb_writer_init(&writer, module, 256, MXSB_VERSION_MINOR) == MXSB_OK);
    assert(mxsb_writer_entry(&writer, 1, MXSB_STAGE_COMPUTE, 1) == MXSB_OK);
    assert(module[MXSB_HEADER_WORDS + 3] == 0 && module[MXSB_HEADER_WORDS + 4] == 0 &&
           module[MXSB_HEADER_WORDS + 5] == 0);
}

static void test_entry_and_binding_rules(void)
{
    struct spec s = compute_spec(store_records, store_counts, 5);
    uint32_t i;

    assert(run(&s) == MXSB_OK);

    for (i = 0; i < 3; i++) {
        s = compute_spec(store_records, store_counts, 5);
        s.workgroup[i] = 0;
        assert(run(&s) == MXSB_ERR_STRUCTURE);
    }
    s = compute_spec(store_records, store_counts, 5);
    s.workgroup[0] = 65536;
    s.workgroup[1] = 65536;
    assert(run(&s) == MXSB_ERR_STRUCTURE);
    s.workgroup[1] = 65535;
    assert(run(&s) == MXSB_OK);
    s = compute_spec(store_records, store_counts, 5);
    s.stage = MXSB_STAGE_VERTEX;
    assert(run(&s) == MXSB_ERR_STRUCTURE);
    s.stage = 7;
    assert(run(&s) == MXSB_ERR_STAGE);

    s = compute_spec(store_records, store_counts, 5);
    s.access = 0;
    assert(run(&s) == MXSB_ERR_BINDING);
    s.access = 4;
    assert(run(&s) == MXSB_ERR_BINDING);
    s.access = MXSB_ACCESS_WRITE;
    assert(run(&s) == MXSB_OK);
    s = compute_spec(store_records, store_counts, 5);
    s.kind = MXSB_BINDING_UNIFORM;
    s.value_type = MXSB_TYPE_F32X4;
    s.elements = 1;
    assert(run(&s) == MXSB_ERR_BINDING);
    s = compute_spec(store_records, store_counts, 5);
    s.elements = 0;
    assert(run(&s) == MXSB_ERR_BINDING);
    s = compute_spec(store_records, store_counts, 5);
    s.value_type = MXSB_TYPE_BOOL;
    assert(run(&s) == MXSB_ERR_BINDING);
    s = compute_spec(store_records, store_counts, 5);
    s.kind = MXSB_BINDING_TEXTURE_CUBE;
    s.value_type = MXSB_TYPE_F32X4;
    s.elements = 0;
    s.access = MXSB_ACCESS_WRITE;
    s.minor = 65;
    assert(run(&s) == MXSB_ERR_BINDING);
}

static void test_store_rules(void)
{
    struct spec s;
    const uint32_t *recs[5];
    uint32_t counts[5];
    uint32_t i;
    static const uint32_t bad_index[] = {REC(5, MXSB_OP_EXTRACT), 11, MXSB_TYPE_I32, 10, 0};
    static const uint32_t bad_value[] = {REC(4, MXSB_OP_CONSTANT), 12, MXSB_TYPE_F32, 7};
    static const uint32_t missing[] = {REC(4, MXSB_OP_BUFFER_STORE), 1, 11, 99};
    static const uint32_t missing_binding[] = {REC(4, MXSB_OP_BUFFER_STORE), 9, 11, 12};

    for (i = 0; i < 5; i++) {
        recs[i] = store_records[i];
        counts[i] = store_counts[i];
    }
    s = compute_spec(store_records, store_counts, 5);
    s.access = MXSB_ACCESS_READ;
    assert(run(&s) == MXSB_ERR_BINDING);

    s = compute_spec(recs, counts, 5);
    recs[1] = bad_index;
    assert(run(&s) == MXSB_ERR_TYPE);
    recs[1] = store_records[1];
    recs[2] = bad_value;
    assert(run(&s) == MXSB_ERR_TYPE);
    recs[2] = store_records[2];
    recs[3] = missing;
    assert(run(&s) == MXSB_ERR_VALUE);
    recs[3] = missing_binding;
    assert(run(&s) == MXSB_ERR_BINDING);
    recs[3] = store_records[3];
    assert(run(&s) == MXSB_OK);
}

static void test_builtin_and_stage_rules(void)
{
    struct spec s = compute_spec(NULL, NULL, 0);
    static const uint32_t wrong_type[] = {REC(4, MXSB_OP_BUILTIN), 10, MXSB_TYPE_U32,
                                          MXSB_BUILTIN_GLOBAL_INVOCATION_ID};
    static const uint32_t unknown[] = {REC(4, MXSB_OP_BUILTIN), 10, MXSB_TYPE_U32X3, 99};
    static const uint32_t local_index[] = {REC(4, MXSB_OP_BUILTIN), 10, MXSB_TYPE_U32,
                                           MXSB_BUILTIN_LOCAL_INVOCATION_INDEX};
    static const uint32_t barrier[] = {REC(1, MXSB_OP_CONTROL_BARRIER)};
    static const uint32_t memory_barrier[] = {REC(1, MXSB_OP_MEMORY_BARRIER)};
    static const uint32_t device_barrier[] = {REC(1, MXSB_OP_DEVICE_MEMORY_BARRIER)};
    static const uint32_t workgroup_memory[] = {REC(2, MXSB_OP_WORKGROUP_MEMORY), 64};
    static const uint32_t discard[] = {REC(1, MXSB_OP_DISCARD)};
    static const uint32_t return_value[] = {REC(2, MXSB_OP_RETURN_VALUE), 10};
    static const uint32_t atomic_load[] = {REC(5, MXSB_OP_BUFFER_ATOMIC_LOAD), 10, MXSB_TYPE_U32,
                                           1, 11};

    assert(run_pair(s, wrong_type, COUNT(wrong_type), r_return, COUNT(r_return)) ==
           MXSB_ERR_TYPE);
    assert(run_pair(s, unknown, COUNT(unknown), r_return, COUNT(r_return)) == MXSB_ERR_VALUE);

    s.stage = MXSB_STAGE_VERTEX;
    s.workgroup[0] = s.workgroup[1] = s.workgroup[2] = 0;
    assert(run_pair(s, r_global, COUNT(r_global), r_return, COUNT(r_return)) == MXSB_ERR_STAGE);
    s.stage = MXSB_STAGE_FRAGMENT;
    assert(run_pair(s, r_global, COUNT(r_global), r_return, COUNT(r_return)) == MXSB_ERR_STAGE);

    s = compute_spec(NULL, NULL, 0);
    assert(run_pair(s, local_index, COUNT(local_index), r_return, COUNT(r_return)) == MXSB_OK);
    s.minor = 50;
    assert(run_pair(s, local_index, COUNT(local_index), r_return, COUNT(r_return)) ==
           MXSB_ERR_VERSION);

    s = compute_spec(NULL, NULL, 0);
    assert(run_pair(s, barrier, COUNT(barrier), r_return, COUNT(r_return)) == MXSB_OK);
    assert(run_pair(s, memory_barrier, COUNT(memory_barrier), r_return, COUNT(r_return)) ==
           MXSB_OK);
    s.stage = MXSB_STAGE_FRAGMENT;
    s.workgroup[0] = s.workgroup[1] = s.workgroup[2] = 0;
    assert(run_pair(s, barrier, COUNT(barrier), r_return, COUNT(r_return)) == MXSB_ERR_STAGE);
    assert(run_pair(s, memory_barrier, COUNT(memory_barrier), r_return, COUNT(r_return)) ==
           MXSB_ERR_STAGE);
    assert(run_pair(s, workgroup_memory, COUNT(workgroup_memory), r_return, COUNT(r_return)) ==
           MXSB_ERR_STAGE);
    assert(run_pair(s, device_barrier, COUNT(device_barrier), r_return, COUNT(r_return)) ==
           MXSB_OK);
    s.stage = MXSB_STAGE_VERTEX;
    assert(run_pair(s, memory_barrier, COUNT(memory_barrier), r_return, COUNT(r_return)) ==
           MXSB_ERR_STAGE);

    s = compute_spec(NULL, NULL, 0);
    assert(run_pair(s, discard, COUNT(discard), NULL, 0) == MXSB_ERR_STAGE);
    assert(run_pair(s, r_global, COUNT(r_global), return_value, COUNT(return_value)) ==
           MXSB_ERR_TYPE);

    s.minor = 15;
    assert(run_pair(s, workgroup_memory, COUNT(workgroup_memory), r_return, COUNT(r_return)) ==
           MXSB_ERR_OPCODE);
    s.minor = 16;
    assert(run_pair(s, workgroup_memory, COUNT(workgroup_memory), r_return, COUNT(r_return)) ==
           MXSB_OK);
    s.minor = 19;
    assert(run_pair(s, atomic_load, COUNT(atomic_load), r_return, COUNT(r_return)) ==
           MXSB_ERR_OPCODE);
}

static const uint32_t m_memory[] = {REC(2, MXSB_OP_WORKGROUP_MEMORY), 64};
static const uint32_t m_local[] = {REC(4, MXSB_OP_BUILTIN), 30, MXSB_TYPE_U32,
                                   MXSB_BUILTIN_LOCAL_INVOCATION_INDEX};
static const uint32_t m_one[] = {REC(4, MXSB_OP_CONSTANT), 31, MXSB_TYPE_U32, 1};
static const uint32_t m_store[] = {REC(5, MXSB_OP_WORKGROUP_STORE), MXSB_TYPE_U32, 0, 30, 31};
static const uint32_t m_barrier[] = {REC(1, MXSB_OP_CONTROL_BARRIER)};
static const uint32_t m_load[] = {REC(5, MXSB_OP_WORKGROUP_LOAD), 32, MXSB_TYPE_U32, 0, 30};
static const uint32_t m_atomic[] = {REC(6, MXSB_OP_BUFFER_ATOMIC_ADD), 33, MXSB_TYPE_U32, 1, 30,
                                    32};
static const uint32_t m_buffer_load[] = {REC(5, MXSB_OP_BUFFER_LOAD), 34, MXSB_TYPE_U32, 1, 30};
static const uint32_t m_device[] = {REC(1, MXSB_OP_DEVICE_MEMORY_BARRIER)};
static const uint32_t *const m_records[] = {m_memory, m_local,  m_one,  m_store,       m_barrier,
                                            m_load,   m_atomic, m_device, m_buffer_load, r_return};
static const uint32_t m_counts[] = {2, 4, 4, 5, 1, 5, 6, 1, 5, 1};

static void test_workgroup_and_atomics(void)
{
    struct spec s = compute_spec(m_records, m_counts, 10);
    const uint32_t *recs[10];
    uint32_t counts[10];
    uint32_t i;
    uint16_t minor = 0;
    static const uint32_t far_store[] = {REC(5, MXSB_OP_WORKGROUP_STORE), MXSB_TYPE_U32, 64, 30,
                                         31};
    static const uint32_t last_store[] = {REC(5, MXSB_OP_WORKGROUP_STORE), MXSB_TYPE_U32, 60, 30,
                                          31};
    static const uint32_t skewed_store[] = {REC(5, MXSB_OP_WORKGROUP_STORE), MXSB_TYPE_U32, 2, 30,
                                            31};
    static const uint32_t float_store[] = {REC(5, MXSB_OP_WORKGROUP_STORE), MXSB_TYPE_F32, 0, 30,
                                           31};
    static const uint32_t float_atomic[] = {REC(6, MXSB_OP_BUFFER_ATOMIC_ADD), 33, MXSB_TYPE_F32,
                                            1, 30, 32};
    static const uint32_t odd_memory[] = {REC(2, MXSB_OP_WORKGROUP_MEMORY), 66};

    assert(run(&s) == MXSB_OK);

    for (i = 0; i < 10; i++) {
        recs[i] = m_records[i];
        counts[i] = m_counts[i];
    }
    s = compute_spec(recs, counts, 10);
    recs[3] = far_store;
    assert(run(&s) == MXSB_ERR_VALUE);
    recs[3] = last_store;
    assert(run(&s) == MXSB_OK);
    recs[3] = skewed_store;
    assert(run(&s) == MXSB_ERR_VALUE);
    recs[3] = float_store;
    assert(run(&s) == MXSB_ERR_TYPE);
    recs[3] = m_records[3];
    recs[6] = float_atomic;
    assert(run(&s) == MXSB_ERR_TYPE);
    recs[6] = m_records[6];
    recs[0] = odd_memory;
    assert(run(&s) == MXSB_ERR_VALUE);
    recs[0] = m_records[0];
    assert(run(&s) == MXSB_OK);

    /* Loads and stores without a declaration are refused. */
    s = compute_spec(m_records + 1, m_counts + 1, 9);
    assert(run(&s) == MXSB_ERR_STRUCTURE);

    /* The declaration must be the first record of the root block. */
    recs[0] = m_local;
    counts[0] = 4;
    recs[1] = m_memory;
    counts[1] = 2;
    s = compute_spec(recs, counts, 10);
    assert(run(&s) == MXSB_ERR_STRUCTURE);

    s = compute_spec(m_records, m_counts, 10);
    s.access = MXSB_ACCESS_READ;
    assert(run(&s) == MXSB_ERR_BINDING);
    s.access = MXSB_ACCESS_WRITE;
    assert(run(&s) == MXSB_ERR_BINDING);

    assert(mxsb_opcode_minimum_minor(MXSB_OP_BUFFER_STORE, &minor) == MXSB_OK && minor == 0);
    assert(mxsb_opcode_minimum_minor(MXSB_OP_TEXTURE_STORE, &minor) == MXSB_OK && minor == 0);
    assert(mxsb_opcode_minimum_minor(MXSB_OP_WORKGROUP_MEMORY, &minor) == MXSB_OK && minor == 16);
    assert(mxsb_opcode_minimum_minor(MXSB_OP_CONTROL_BARRIER, &minor) == MXSB_OK && minor == 16);
    assert(mxsb_opcode_minimum_minor(MXSB_OP_BUFFER_ATOMIC_LOAD, &minor) == MXSB_OK &&
           minor == 20);
    assert(mxsb_opcode_minimum_minor(MXSB_OP_DEVICE_MEMORY_BARRIER, &minor) == MXSB_OK &&
           minor == 20);
}

static void test_texture_store(void)
{
    static const uint32_t c_zero[] = {REC(4, MXSB_OP_CONSTANT), 41, MXSB_TYPE_U32, 0};
    static const uint32_t c_float[] = {REC(4, MXSB_OP_CONSTANT), 44, MXSB_TYPE_F32, 0};
    static const uint32_t coords2[] = {REC(6, MXSB_OP_CONSTRUCT), 42, MXSB_TYPE_U32X2, 2, 41, 41};
    static const uint32_t coords3[] = {REC(7, MXSB_OP_CONSTRUCT), 42, MXSB_TYPE_U32X3, 3, 41, 41,
                                       41};
    static const uint32_t color[] = {REC(8, MXSB_OP_CONSTRUCT), 43, MXSB_TYPE_F32X4, 4, 44, 44,
                                     44, 44};
    static const uint32_t store[] = {REC(4, MXSB_OP_TEXTURE_STORE), 1, 42, 43};
    const uint32_t *recs[6];
    uint32_t counts[6] = {4, 4, 6, 8, 4, 1};
    struct spec s;

    recs[0] = c_zero;
    recs[1] = c_float;
    recs[2] = coords2;
    recs[3] = color;
    recs[4] = store;
    recs[5] = r_return;
    s = compute_spec(recs, counts, 6);
    s.kind = MXSB_BINDING_TEXTURE_2D;
    s.access = MXSB_ACCESS_WRITE;
    s.value_type = MXSB_TYPE_F32X4;
    s.elements = 0;
    assert(run(&s) == MXSB_OK);
    s.access = MXSB_ACCESS_READ;
    assert(run(&s) == MXSB_ERR_BINDING);
    s.access = MXSB_ACCESS_READ_WRITE;
    assert(run(&s) == MXSB_OK);
    recs[2] = coords3;
    counts[2] = 7;
    assert(run(&s) == MXSB_ERR_TYPE);
}

int main(void)
{
    test_compute_submit();
    test_compute_command();
    test_writer_module();
    test_entry_and_binding_rules();
    test_store_rules();
    test_builtin_and_stage_rules();
    test_workgroup_and_atomics();
    test_texture_store();
    puts("compute submit codec, compute stage, storage bindings, barriers, workgroup memory and atomics PASS");
    return 0;
}
