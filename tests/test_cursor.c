/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxgpu_wire.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void same_update(const struct mxgpu_cursor_update *a, const struct mxgpu_cursor_update *b)
{
    assert(a->resource_id == b->resource_id);
    assert(a->scanout_id == b->scanout_id);
    assert(a->flags == b->flags);
    assert(a->x == b->x && a->y == b->y);
    assert(a->hot_x == b->hot_x && a->hot_y == b->hot_y);
}

static void roundtrip(struct mxgpu_cursor_update update)
{
    uint8_t wire[MXGPU_CURSOR_UPDATE_SIZE];
    struct mxgpu_cursor_update decoded;
    uint32_t bytes = 0;

    assert(mxgpu_cursor_update_encode(&update, wire, sizeof wire, &bytes) == MX_OK);
    assert(bytes == sizeof wire);
    assert(mxgpu_cursor_update_decode(wire, bytes, &decoded) == MX_OK);
    same_update(&update, &decoded);
}

static void reject(struct mxgpu_cursor_update update, int expected)
{
    uint8_t wire[MXGPU_CURSOR_UPDATE_SIZE];
    uint8_t before[MXGPU_CURSOR_UPDATE_SIZE];
    uint32_t bytes = 99;

    memset(wire, 0xa5, sizeof wire);
    memcpy(before, wire, sizeof before);
    assert(mxgpu_cursor_update_encode(&update, wire, sizeof wire, &bytes) == expected);
    assert(bytes == 0 && memcmp(wire, before, sizeof wire) == 0);
}

int main(void)
{
    const uint8_t expected[MXGPU_CURSOR_UPDATE_SIZE] = {
        0x78, 0x56, 0x34, 0x12, 0x34, 0x12, 0x00, 0x00, 0xff, 0xff, 0xff,
        0xff, 0x00, 0x00, 0x00, 0x80, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    struct mxgpu_cursor_update image = {0x12345678, 0x1234, 0, -1, INT32_MIN, 3, 4};
    struct mxgpu_cursor_update move = {0, 65535, MXGPU_CURSOR_MOVE_ONLY, INT32_MAX, INT32_MIN,
                                       0, 0};
    struct mxgpu_cursor_update hide = {0, 65535, MXGPU_CURSOR_HIDE, 0, 0, 0, 0};
    struct mxgpu_cursor_update bad, decoded, untouched;
    uint8_t wire[MXGPU_CURSOR_UPDATE_SIZE + 1], before[sizeof wire];
    uint8_t command[MXGPU_COMMAND_HEADER_SIZE + MXGPU_CURSOR_UPDATE_SIZE];
    struct mxgpu_command_header header = {0}, decoded_header;
    const uint8_t *payload;
    uint32_t bytes, payload_bytes, i, bit;
    uint16_t queue;

    assert(mxgpu_cursor_update_encode(&image, wire, sizeof wire, &bytes) == MX_OK);
    assert(bytes == sizeof expected && memcmp(wire, expected, sizeof expected) == 0);
    roundtrip(image);
    roundtrip(move);
    roundtrip(hide);
    image.x = INT32_MAX;
    image.y = 0;
    image.hot_x = UINT32_MAX;
    image.hot_y = UINT32_MAX;
    roundtrip(image);
    bad = image;
    bad.resource_id = 0;
    reject(bad, MX_ERR_RESOURCE);
    bad = hide;
    bad.flags |= MXGPU_CURSOR_MOVE_ONLY;
    reject(bad, MX_ERR_FLAGS);
    for (bit = 2; bit < 16; bit++) {
        bad = image;
        bad.flags = (uint16_t)(1u << bit);
        reject(bad, MX_ERR_FLAGS);
    }
    for (i = 0; i < 5; i++) {
        bad = hide;
        if (i == 0)
            bad.resource_id = 1;
        if (i == 1)
            bad.x = -1;
        if (i == 2)
            bad.y = 1;
        if (i == 3)
            bad.hot_x = 1;
        if (i == 4)
            bad.hot_y = 1;
        reject(bad, MX_ERR_SHAPE);
    }
    for (i = 0; i < 3; i++) {
        bad = move;
        if (i == 0)
            bad.resource_id = 1;
        if (i == 1)
            bad.hot_x = 1;
        if (i == 2)
            bad.hot_y = 1;
        reject(bad, MX_ERR_SHAPE);
    }
    assert(mxgpu_cursor_update_encode(&image, wire, sizeof wire, &bytes) == MX_OK);
    memset(&decoded, 0x5a, sizeof decoded);
    untouched = decoded;
    for (i = 0; i <= sizeof wire; i++) {
        if (i == MXGPU_CURSOR_UPDATE_SIZE)
            continue;
        assert(mxgpu_cursor_update_decode(wire, i, &decoded) == MX_ERR_LENGTH);
        assert(memcmp(&decoded, &untouched, sizeof decoded) == 0);
    }
    for (i = 24; i < MXGPU_CURSOR_UPDATE_SIZE; i++) {
        for (bit = 0; bit < 8; bit++) {
            wire[i] = (uint8_t)(1u << bit);
            assert(mxgpu_cursor_update_decode(wire, MXGPU_CURSOR_UPDATE_SIZE, &decoded) ==
                   MX_ERR_RESERVED);
            assert(memcmp(&decoded, &untouched, sizeof decoded) == 0);
        }
        wire[i] = 0;
    }
    wire[6] = 3;
    assert(mxgpu_cursor_update_decode(wire, MXGPU_CURSOR_UPDATE_SIZE, &decoded) == MX_ERR_FLAGS);
    assert(memcmp(&decoded, &untouched, sizeof decoded) == 0);
    wire[6] = 0;
    memset(wire, 0xa5, sizeof wire);
    memcpy(before, wire, sizeof wire);
    for (i = 0; i < MXGPU_CURSOR_UPDATE_SIZE; i++) {
        bytes = 99;
        assert(mxgpu_cursor_update_encode(&image, wire, i, &bytes) == MX_ERR_LENGTH);
        assert(bytes == 0 && memcmp(wire, before, sizeof wire) == 0);
    }
    assert(mxgpu_cursor_update_encode(NULL, wire, sizeof wire, &bytes) == MX_ERR_STATE);
    assert(mxgpu_cursor_update_encode(&image, NULL, 0, &bytes) == MX_OK);
    assert(bytes == MXGPU_CURSOR_UPDATE_SIZE);
    assert(mxgpu_cursor_update_decode(NULL, MXGPU_CURSOR_UPDATE_SIZE, &decoded) == MX_ERR_LENGTH);
    assert(mxgpu_cursor_update_decode(expected, sizeof expected, NULL) == MX_ERR_STATE);
    assert(mxgpu_opcode_queue(MXGPU_OP_CURSOR_UPDATE, &queue) == MX_OK &&
           queue == MXGPU_QUEUE_CURSOR);
    header.opcode = MXGPU_OP_CURSOR_UPDATE;
    header.context_id = 1;
    header.queue = MXGPU_QUEUE_CURSOR;
    header.sequence = 1;
    assert(mxgpu_command_encode(&header, expected, sizeof expected, sizeof command, command,
                                sizeof command, &bytes) == MX_OK);
    assert(mxgpu_command_decode(command, bytes, sizeof command, &decoded_header, &payload,
                                &payload_bytes) == MX_OK);
    assert(decoded_header.opcode == MXGPU_OP_CURSOR_UPDATE);
    assert(payload_bytes == sizeof expected && memcmp(payload, expected, sizeof expected) == 0);
    header.queue = MXGPU_QUEUE_RENDER;
    assert(mxgpu_command_encode(&header, expected, sizeof expected, sizeof command, command,
                                sizeof command, &bytes) == MX_ERR_QUEUE);
    puts("cursor image, move, hide, coordinates, hostile mutations, queue and atomic refusal PASS");
    return 0;
}
