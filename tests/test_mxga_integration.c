/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxga.h"
#include "mx_le.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t title[] = {'T', 'e', 'r', 'm'};
static const uint8_t application[] = {'a', 'p', 'p'};
static const uint8_t icon[] = {0, 0, 255, 255};
static struct mxga_integration_segment segment = {5, 3, 4, 16, 8, 0, 0, 16, 8};
static struct mxga_integrated_window window = {.window_id = UINT64_C(0x0102030405060708),
                                               .x = 3,
                                               .y = 4,
                                               .width = 16,
                                               .height = 8,
                                               .flags = MXGA_INTEGRATION_WINDOW_GEOMETRY_RELIABLE,
                                               .title = title,
                                               .title_bytes = sizeof title,
                                               .application_id = application,
                                               .application_id_bytes = sizeof application,
                                               .icon_bgra = icon,
                                               .icon_bgra_bytes = sizeof icon,
                                               .icon_width = 1,
                                               .icon_height = 1,
                                               .scanout_id = 5,
                                               .segment_count = 1,
                                               .output_width = 16,
                                               .output_height = 8,
                                               .segments = &segment};
static struct mxga_integration_status status = {9, MXGA_INTEGRATION_REQUIRED_FLAGS, 1, &window};
static uint8_t payload[MXGA_MAX_FRAME_BYTES + 1], reference[MXGA_MAX_FRAME_BYTES + 1];
static struct mxga_integrated_window decoded_windows[MXGA_INTEGRATION_MAX_WINDOWS];
static struct mxga_integration_segment
    decoded_segments[MXGA_INTEGRATION_MAX_WINDOWS * MXGA_INTEGRATION_MAX_SEGMENTS];
static uint32_t reference_bytes;

static int decode(const uint8_t *bytes, uint32_t count)
{
    struct mxga_integration_status got;
    return mxga_decode_integration_status(
        bytes, count, &got, decoded_windows, MXGA_INTEGRATION_MAX_WINDOWS, decoded_segments,
        MXGA_INTEGRATION_MAX_WINDOWS * MXGA_INTEGRATION_MAX_SEGMENTS);
}

static void roundtrip(void)
{
    struct mxga_integration_status got;
    uint32_t count;
    assert(mxga_encode_integration_status(&status, payload, sizeof payload, &count) == MXGA_OK);
    assert(count == 115);
    assert(mx_r16(payload, 0) == 4 && mx_r16(payload, 2) == 31 && mx_r64(payload, 4) == 9 &&
           mx_r32(payload, 12) == 1);
    assert(mx_r64(payload, 16) == UINT64_C(0x0102030405060708));
    assert(mx_r32(payload, 24) == 3 && mx_r32(payload, 28) == 4 && mx_r32(payload, 32) == 16 &&
           mx_r32(payload, 36) == 8);
    assert(mx_r16(payload, 56) == 5 && mx_r16(payload, 58) == 1 && mx_r32(payload, 60) == 16 &&
           mx_r32(payload, 64) == 8);
    assert(!memcmp(payload + 68, "Termapp", 7) && !memcmp(payload + 75, icon, 4));
    assert(mx_r16(payload, 79) == 5 && !mx_r16(payload, 81));
    assert(mx_r32(payload, 83) == 3 && mx_r32(payload, 87) == 4 && mx_r32(payload, 91) == 16 &&
           mx_r32(payload, 95) == 8);
    assert(!mx_r32(payload, 99) && !mx_r32(payload, 103) && mx_r32(payload, 107) == 16 &&
           mx_r32(payload, 111) == 8);
    assert(mxga_decode_integration_status(payload, count, &got, decoded_windows, 128,
                                          decoded_segments, 4096) == MXGA_OK);
    assert(got.generation == 9 && got.window_count == 1 && got.windows == decoded_windows);
    assert(decoded_windows[0].title == payload + 68 &&
           decoded_windows[0].segments == decoded_segments);
    assert(decoded_windows[0].icon_bgra_bytes == 4 &&
           !memcmp(decoded_windows[0].icon_bgra, icon, 4));
    assert(decoded_segments[0].scanout_id == 5 && decoded_segments[0].destination_height == 8);
    assert(mxga_encode_integration_status(&got, reference, sizeof reference, &reference_bytes) ==
           MXGA_OK);
    assert(reference_bytes == count && !memcmp(reference, payload, count));
    memset(payload, 0xa5, sizeof payload);
    count = 42;
    assert(mxga_encode_integration_status(&status, payload, 114, &count) == MXGA_ERR_CAPACITY &&
           !count && payload[0] == 0xa5);
    assert(mxga_decode_integration_status(reference, reference_bytes, &got, decoded_windows, 0,
                                          decoded_segments, 4096) == MXGA_ERR_CAPACITY);
    assert(mxga_decode_integration_status(reference, reference_bytes, &got, decoded_windows, 128,
                                          decoded_segments, 0) == MXGA_ERR_CAPACITY);
    assert(mxga_decode_integration_status(reference, reference_bytes, &got, NULL, 128,
                                          decoded_segments, 4096) == MXGA_ERR_CAPACITY);
    assert(mxga_decode_integration_status(reference, reference_bytes, &got, decoded_windows, 128,
                                          NULL, 4096) == MXGA_ERR_CAPACITY);
}

static void invalid_wire(void)
{
    uint32_t at;
    for (at = 0; at < reference_bytes; at++)
        assert(decode(reference, at) != MXGA_OK);
    memcpy(payload, reference, reference_bytes);
    payload[reference_bytes] = 0;
    assert(decode(payload, reference_bytes + 1) == MXGA_ERR_LENGTH);
    memcpy(payload, reference, reference_bytes);
    mx_w16(payload, 0, 3);
    assert(decode(payload, reference_bytes) == MXGA_ERR_VERSION);
    memcpy(payload, reference, reference_bytes);
    mx_w16(payload, 2, 32);
    assert(decode(payload, reference_bytes) == MXGA_ERR_FLAGS);
    memcpy(payload, reference, reference_bytes);
    mx_w64(payload, 4, 0);
    assert(decode(payload, reference_bytes) == MXGA_ERR_PAYLOAD);
    memcpy(payload, reference, reference_bytes);
    mx_w32(payload, 12, 129);
    assert(decode(payload, reference_bytes) == MXGA_ERR_PAYLOAD);
    memcpy(payload, reference, reference_bytes);
    mx_w64(payload, 16, 0);
    assert(decode(payload, reference_bytes) == MXGA_ERR_PAYLOAD);
    memcpy(payload, reference, reference_bytes);
    mx_w32(payload, 24, UINT32_MAX);
    assert(decode(payload, reference_bytes) == MXGA_ERR_GEOMETRY);
    memcpy(payload, reference, reference_bytes);
    mx_w32(payload, 32, UINT32_MAX);
    assert(decode(payload, reference_bytes) == MXGA_ERR_GEOMETRY);
    memcpy(payload, reference, reference_bytes);
    mx_w32(payload, 40, 2);
    assert(decode(payload, reference_bytes) == MXGA_ERR_FLAGS);
    memcpy(payload, reference, reference_bytes);
    mx_w16(payload, 48, 257);
    assert(decode(payload, reference_bytes) == MXGA_ERR_PAYLOAD);
    memcpy(payload, reference, reference_bytes);
    mx_w32(payload, 52, UINT32_MAX);
    assert(decode(payload, reference_bytes) == MXGA_ERR_LENGTH);
    memcpy(payload, reference, reference_bytes);
    mx_w16(payload, 58, 0);
    assert(decode(payload, reference_bytes) == MXGA_ERR_GEOMETRY);
    memcpy(payload, reference, reference_bytes);
    mx_w16(payload, 81, 1);
    assert(decode(payload, reference_bytes) == MXGA_ERR_FLAGS);
    memcpy(payload, reference, reference_bytes);
    mx_w32(payload, 83, 2);
    assert(decode(payload, reference_bytes) == MXGA_ERR_GEOMETRY);
    memcpy(payload, reference, reference_bytes);
    mx_w32(payload, 107, 15);
    assert(decode(payload, reference_bytes) == MXGA_ERR_GEOMETRY);
    memcpy(payload, reference, reference_bytes);
    mx_w32(payload, 99, UINT32_MAX);
    assert(decode(payload, reference_bytes) == MXGA_ERR_GEOMETRY);
    memcpy(payload, reference, reference_bytes);
    payload[68] = 0x80;
    assert(decode(payload, reference_bytes) == MXGA_ERR_UTF8);
}

static void text_tests(void)
{
    static const uint8_t valid[] = {0,    0xc2, 0x80, 0xdf, 0xbf, 0xe0, 0xa0, 0x80,
                                    0xed, 0x9f, 0xbf, 0xef, 0xbf, 0xbf, 0xf0, 0x90,
                                    0x80, 0x80, 0xf4, 0x8f, 0xbf, 0xbf};
    static const uint8_t invalid[][4] = {{0x80},
                                         {0xc0, 0xaf},
                                         {0xc2},
                                         {0xe0, 0x80, 0x80},
                                         {0xed, 0xa0, 0x80},
                                         {0xf4, 0x90, 0x80, 0x80},
                                         {0xf5, 0x80, 0x80, 0x80},
                                         {0xf0, 0x80, 0x80, 0x80}};
    static const uint16_t lengths[] = {1, 2, 1, 3, 3, 4, 4, 4};
    struct mxga_integrated_window altered = window;
    struct mxga_integration_status test = status;
    uint32_t count;
    unsigned i;
    test.windows = &altered;
    altered.title = valid;
    altered.title_bytes = sizeof valid;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) == MXGA_OK &&
           decode(payload, count) == MXGA_OK);
    for (i = 0; i < sizeof lengths / sizeof lengths[0]; i++) {
        altered.title = invalid[i];
        altered.title_bytes = lengths[i];
        assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) ==
                   MXGA_ERR_UTF8 &&
               !count);
    }
    altered.title = NULL;
    altered.title_bytes = 1;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) ==
           MXGA_ERR_PAYLOAD);
    altered.title_bytes = 0;
    altered.application_id = NULL;
    altered.application_id_bytes = 0;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) == MXGA_OK &&
           decode(payload, count) == MXGA_OK);
}

static void segments_tests(void)
{
    struct mxga_integration_segment pieces[32];
    struct mxga_integrated_window altered = window;
    struct mxga_integration_status test = status;
    uint32_t count;
    unsigned i;
    test.windows = &altered;
    altered.segments = pieces;
    altered.segment_count = 32;
    altered.x = altered.y = 0;
    altered.width = 1;
    altered.height = 8;
    altered.output_width = 32;
    altered.output_height = 8;
    altered.scanout_id = 0;
    for (i = 0; i < 32; i++)
        pieces[i] = (struct mxga_integration_segment){(uint16_t)i, 0, 0, 1, 8, i, 0, 1, 8};
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) == MXGA_OK &&
           decode(payload, count) == MXGA_OK);
    pieces[31].destination_x = 30;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) ==
           MXGA_ERR_GEOMETRY);
    pieces[31].destination_x = 31;
    pieces[31].scanout_id = 30;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) ==
           MXGA_ERR_GEOMETRY);
    pieces[31].scanout_id = 31;
    altered.output_width = 33;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) ==
           MXGA_ERR_GEOMETRY);
    altered.output_width = 32;
    altered.segment_count = 33;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) ==
           MXGA_ERR_GEOMETRY);
    altered.segment_count = 1;
    altered.output_width = altered.output_height = UINT32_MAX;
    altered.width = altered.height = UINT32_MAX;
    pieces[0].width = pieces[0].height = UINT32_MAX;
    pieces[0].destination_width = pieces[0].destination_height = UINT32_MAX;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) == MXGA_OK &&
           decode(payload, count) == MXGA_OK);
    pieces[0].destination_x = 1;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) ==
           MXGA_ERR_GEOMETRY);
}

static uint32_t random_word(uint32_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

static void coverage_tests(void)
{
    struct mxga_integration_segment pieces[4];
    struct mxga_integrated_window altered = window;
    struct mxga_integration_status test = status;
    uint32_t random = 0x528147u, iteration, accepted = 0;
    test.windows = &altered;
    altered.segments = pieces;
    altered.output_width = 4;
    altered.output_height = 3;
    altered.scanout_id = 0;
    altered.x = altered.y = 0;
    for (iteration = 0; iteration < 20000; iteration++) {
        unsigned cells[3][4] = {{0}}, i, x, y, exact = 1;
        uint32_t bytes;
        int result;
        altered.segment_count = (uint16_t)(1 + random_word(&random) % 4);
        for (i = 0; i < altered.segment_count; i++) {
            struct mxga_integration_segment *piece = &pieces[i];
            piece->scanout_id = (uint16_t)i;
            piece->x = piece->y = 0;
            piece->destination_x = random_word(&random) % 4;
            piece->destination_y = random_word(&random) % 3;
            piece->width = piece->destination_width =
                1 + random_word(&random) % (4 - piece->destination_x);
            piece->height = piece->destination_height =
                1 + random_word(&random) % (3 - piece->destination_y);
            for (y = piece->destination_y; y < piece->destination_y + piece->destination_height;
                 y++)
                for (x = piece->destination_x; x < piece->destination_x + piece->destination_width;
                     x++)
                    cells[y][x]++;
        }
        altered.width = pieces[0].width;
        altered.height = pieces[0].height;
        for (y = 0; y < 3; y++)
            for (x = 0; x < 4; x++)
                if (cells[y][x] != 1)
                    exact = 0;
        result = mxga_encode_integration_status(&test, payload, sizeof payload, &bytes);
        assert((result == MXGA_OK) == exact);
        if (exact) {
            accepted++;
            assert(decode(payload, bytes) == MXGA_OK);
        } else
            assert(result == MXGA_ERR_GEOMETRY && !bytes);
    }
    assert(accepted);
}

static void inventory_tests(void)
{
    struct mxga_integrated_window windows[128];
    struct mxga_integration_status test = status, got, untouched;
    struct mxga_integrated_window unchanged_window;
    struct mxga_integration_segment unchanged_segment;
    uint8_t *large_icon = malloc(256u * 256u * 4u), text[4096];
    uint32_t count, i;
    assert(large_icon);
    memset(large_icon, 0, 256u * 256u * 4u);
    memset(text, 'a', sizeof text);
    windows[0] = window;
    windows[0].title = windows[0].application_id = text;
    windows[0].title_bytes = windows[0].application_id_bytes = sizeof text;
    test.windows = windows;
    test.window_count = 1;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) == MXGA_OK &&
           decode(payload, count) == MXGA_OK);
    windows[0].title_bytes++;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) ==
               MXGA_ERR_PAYLOAD &&
           !count);
    for (i = 0; i < 128; i++) {
        windows[i] = window;
        windows[i].window_id = i + 1;
    }
    test.windows = windows;
    test.window_count = 128;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) == MXGA_OK &&
           decode(payload, count) == MXGA_OK);
    test.window_count = 129;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) ==
           MXGA_ERR_PAYLOAD);
    test.window_count = 2;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) == MXGA_OK);
    mx_w16(payload, 16 + 99 + 42, 0);
    memset(&got, 0xa5, sizeof got);
    untouched = got;
    memset(decoded_windows, 0xa5, sizeof decoded_windows);
    unchanged_window = decoded_windows[0];
    memset(decoded_segments, 0xa5, sizeof decoded_segments);
    unchanged_segment = decoded_segments[0];
    assert(mxga_decode_integration_status(payload, count, &got, decoded_windows, 128,
                                          decoded_segments, 4096) != MXGA_OK);
    assert(!memcmp(&got, &untouched, sizeof got) &&
           !memcmp(decoded_windows, &unchanged_window, sizeof unchanged_window) &&
           !memcmp(decoded_segments, &unchanged_segment, sizeof unchanged_segment));
    test.window_count = 4;
    for (i = 0; i < 4; i++) {
        windows[i] = window;
        windows[i].title = NULL;
        windows[i].title_bytes = 0;
        windows[i].application_id = NULL;
        windows[i].application_id_bytes = 0;
        windows[i].icon_width = windows[i].icon_height = i == 3 ? 255 : 256;
        windows[i].icon_bgra_bytes = windows[i].icon_width * windows[i].icon_height * 4u;
        windows[i].icon_bgra = large_icon;
    }
    windows[0].title = text;
    windows[0].title_bytes = 1652;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) == MXGA_OK);
    assert(count == MXGA_MAX_FRAME_BYTES - MXGA_HEADER_BYTES && decode(payload, count) == MXGA_OK);
    assert(mxga_encode_frame(MXGA_PROTOCOL_MINOR, MXGA_OP_INTEGRATION_STATUS, 1, payload, count,
                             reference, sizeof reference, &count) == MXGA_OK &&
           count == MXGA_MAX_FRAME_BYTES);
    windows[0].title_bytes++;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) ==
               MXGA_ERR_LENGTH &&
           !count);
    test.window_count = 0;
    test.windows = NULL;
    test.flags = 0;
    assert(mxga_encode_integration_status(&test, payload, sizeof payload, &count) == MXGA_OK &&
           count == 16);
    assert(mxga_decode_integration_status(payload, count, &got, NULL, 0, NULL, 0) == MXGA_OK &&
           !got.window_count && !got.flags);
    free(large_icon);
}

static void action_tests(void)
{
    static const uint8_t golden[20] = {1, 0, 2, 0, 9, 0, 0, 0, 0, 0, 0, 0, 8, 7, 6, 5, 4, 3, 2, 1};
    struct mxga_integration_action action = {MXGA_INTEGRATION_ACTION_CLOSE, 9,
                                             UINT64_C(0x0102030405060708)},
                                   got;
    uint32_t count;
    assert(mxga_encode_integration_action(&action, payload, sizeof payload, &count) == MXGA_OK &&
           count == 20 && !memcmp(payload, golden, 20));
    assert(mxga_decode_integration_action(golden, 20, &got) == MXGA_OK && got.action == 2 &&
           got.window_id == action.window_id);
    action.action = MXGA_INTEGRATION_ACTION_ACTIVATE;
    assert(mxga_encode_integration_action(&action, payload, sizeof payload, &count) == MXGA_OK &&
           payload[2] == 1);
    action.action = 3;
    assert(mxga_encode_integration_action(&action, payload, sizeof payload, &count) ==
               MXGA_ERR_PAYLOAD &&
           !count);
    assert(mxga_decode_integration_action(golden, 19, &got) == MXGA_ERR_LENGTH);
    memcpy(payload, golden, 20);
    mx_w16(payload, 0, 2);
    assert(mxga_decode_integration_action(payload, 20, &got) == MXGA_ERR_VERSION);
    memcpy(payload, golden, 20);
    mx_w64(payload, 4, 0);
    assert(mxga_decode_integration_action(payload, 20, &got) == MXGA_ERR_PAYLOAD);
    memcpy(payload, golden, 20);
    mx_w64(payload, 12, 0);
    assert(mxga_decode_integration_action(payload, 20, &got) == MXGA_ERR_PAYLOAD);
}

int main(void)
{
    roundtrip();
    invalid_wire();
    text_tests();
    segments_tests();
    coverage_tests();
    inventory_tests();
    action_tests();
    puts(
        "PASS MXGA integration v4: wire fields, actions, strict UTF8, geometry and coverage, malformed/truncated records, caller capacities, atomic decode, 128 windows, 32 segments, exact 1MiB frame bound");
    return 0;
}
