/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxga.h"
#include "mx_le.h"
#include <string.h>

static int text_valid(const uint8_t *text, uint32_t bytes)
{
    uint32_t at = 0;
    if (bytes && !text)
        return MXGA_ERR_PAYLOAD;
    while (at < bytes) {
        uint32_t value = text[at++], remaining, minimum;
        if (value < 128u)
            continue;
        if (value >= 0xc2u && value <= 0xdfu) {
            value &= 0x1fu;
            remaining = 1;
            minimum = 0x80u;
        } else if (value >= 0xe0u && value <= 0xefu) {
            value &= 0x0fu;
            remaining = 2;
            minimum = 0x800u;
        } else if (value >= 0xf0u && value <= 0xf4u) {
            value &= 7u;
            remaining = 3;
            minimum = 0x10000u;
        } else
            return MXGA_ERR_UTF8;
        if (remaining > bytes - at)
            return MXGA_ERR_UTF8;
        while (remaining--) {
            uint32_t next = text[at++];
            if ((next & 0xc0u) != 0x80u)
                return MXGA_ERR_UTF8;
            value = (value << 6) | (next & 0x3fu);
        }
        if (value < minimum || value > 0x10ffffu || (value >= 0xd800u && value <= 0xdfffu))
            return MXGA_ERR_UTF8;
    }
    return MXGA_OK;
}

static int32_t read_coordinate(const uint8_t *bytes, uint32_t offset)
{
    uint32_t value = mx_r32(bytes, offset);
    return value <= (UINT32_MAX >> 1) ? (int32_t)value : -1;
}

static void read_segment(const uint8_t *bytes, struct mxga_integration_segment *segment)
{
    segment->scanout_id = mx_r16(bytes, 0);
    segment->x = read_coordinate(bytes, 4);
    segment->y = read_coordinate(bytes, 8);
    segment->width = mx_r32(bytes, 12);
    segment->height = mx_r32(bytes, 16);
    segment->destination_x = mx_r32(bytes, 20);
    segment->destination_y = mx_r32(bytes, 24);
    segment->destination_width = mx_r32(bytes, 28);
    segment->destination_height = mx_r32(bytes, 32);
}

static int source_valid(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    return x >= 0 && y >= 0 && width && height && width <= UINT32_MAX - (uint32_t)x &&
           height <= UINT32_MAX - (uint32_t)y;
}

static int segments_valid(const struct mxga_integrated_window *window, const uint8_t *wire)
{
    uint64_t covered = 0, area = (uint64_t)window->output_width * window->output_height;
    uint32_t i, j;
    if (!window->segment_count || window->segment_count > MXGA_INTEGRATION_MAX_SEGMENTS ||
        !window->output_width || !window->output_height || (!wire && !window->segments))
        return MXGA_ERR_GEOMETRY;
    for (i = 0; i < window->segment_count; i++) {
        struct mxga_integration_segment segment;
        uint64_t piece;
        if (wire) {
            const uint8_t *record = wire + i * MXGA_INTEGRATION_SEGMENT_BYTES;
            if (mx_r16(record, 2))
                return MXGA_ERR_FLAGS;
            read_segment(record, &segment);
        } else
            segment = window->segments[i];
        if (!source_valid(segment.x, segment.y, segment.width, segment.height) ||
            !segment.destination_width || !segment.destination_height ||
            segment.destination_x > window->output_width ||
            segment.destination_y > window->output_height ||
            segment.destination_width > window->output_width - segment.destination_x ||
            segment.destination_height > window->output_height - segment.destination_y)
            return MXGA_ERR_GEOMETRY;
        if (!i && (segment.scanout_id != window->scanout_id || segment.x != window->x ||
                   segment.y != window->y || segment.width != window->width ||
                   segment.height != window->height))
            return MXGA_ERR_GEOMETRY;
        for (j = 0; j < i; j++) {
            struct mxga_integration_segment earlier;
            if (wire)
                read_segment(wire + j * MXGA_INTEGRATION_SEGMENT_BYTES, &earlier);
            else
                earlier = window->segments[j];
            if (earlier.scanout_id == segment.scanout_id ||
                (earlier.destination_x < segment.destination_x + segment.destination_width &&
                 segment.destination_x < earlier.destination_x + earlier.destination_width &&
                 earlier.destination_y < segment.destination_y + segment.destination_height &&
                 segment.destination_y < earlier.destination_y + earlier.destination_height))
                return MXGA_ERR_GEOMETRY;
        }
        piece = (uint64_t)segment.destination_width * segment.destination_height;
        if (piece > area - covered)
            return MXGA_ERR_GEOMETRY;
        covered += piece;
    }
    return covered == area ? MXGA_OK : MXGA_ERR_GEOMETRY;
}

static int window_valid(const struct mxga_integrated_window *window, const uint8_t *wire)
{
    int result;
    uint32_t icon_bytes;
    if (!window->window_id || window->title_bytes > MXGA_INTEGRATION_MAX_TEXT_BYTES ||
        window->application_id_bytes > MXGA_INTEGRATION_MAX_TEXT_BYTES ||
        window->icon_width > MXGA_INTEGRATION_MAX_ICON_DIMENSION ||
        window->icon_height > MXGA_INTEGRATION_MAX_ICON_DIMENSION)
        return MXGA_ERR_PAYLOAD;
    if (window->flags & ~MXGA_INTEGRATION_WINDOW_GEOMETRY_RELIABLE)
        return MXGA_ERR_FLAGS;
    if (!source_valid(window->x, window->y, window->width, window->height))
        return MXGA_ERR_GEOMETRY;
    icon_bytes = (uint32_t)window->icon_width * window->icon_height * 4u;
    if (icon_bytes != window->icon_bgra_bytes || (icon_bytes && !window->icon_bgra))
        return MXGA_ERR_PAYLOAD;
    result = text_valid(window->title, window->title_bytes);
    if (result)
        return result;
    result = text_valid(window->application_id, window->application_id_bytes);
    return result ? result : segments_valid(window, wire);
}

static void write_segment(const struct mxga_integration_segment *segment, uint8_t *out)
{
    mx_w16(out, 0, segment->scanout_id);
    mx_w16(out, 2, 0);
    mx_w32(out, 4, (uint32_t)segment->x);
    mx_w32(out, 8, (uint32_t)segment->y);
    mx_w32(out, 12, segment->width);
    mx_w32(out, 16, segment->height);
    mx_w32(out, 20, segment->destination_x);
    mx_w32(out, 24, segment->destination_y);
    mx_w32(out, 28, segment->destination_width);
    mx_w32(out, 32, segment->destination_height);
}

int mxga_encode_integration_status(const struct mxga_integration_status *status, uint8_t *out,
                                   uint32_t cap, uint32_t *out_len)
{
    uint32_t i, j, required = MXGA_INTEGRATION_STATUS_HEADER_BYTES, at;
    if (out_len)
        *out_len = 0;
    if (!status || !status->generation || status->window_count > MXGA_INTEGRATION_MAX_WINDOWS ||
        (status->window_count && !status->windows))
        return MXGA_ERR_PAYLOAD;
    if (status->flags & ~MXGA_INTEGRATION_REQUIRED_FLAGS)
        return MXGA_ERR_FLAGS;
    for (i = 0; i < status->window_count; i++) {
        const struct mxga_integrated_window *window = &status->windows[i];
        uint32_t bytes;
        int result = window_valid(window, NULL);
        if (result)
            return result;
        bytes = MXGA_INTEGRATION_WINDOW_BYTES + window->title_bytes + window->application_id_bytes +
                window->icon_bgra_bytes + window->segment_count * MXGA_INTEGRATION_SEGMENT_BYTES;
        if (bytes > MXGA_MAX_FRAME_BYTES - MXGA_HEADER_BYTES - required)
            return MXGA_ERR_LENGTH;
        required += bytes;
    }
    if (!out || cap < required)
        return MXGA_ERR_CAPACITY;
    mx_w16(out, 0, MXGA_INTEGRATION_STATUS_VERSION);
    mx_w16(out, 2, status->flags);
    mx_w64(out, 4, status->generation);
    mx_w32(out, 12, status->window_count);
    at = MXGA_INTEGRATION_STATUS_HEADER_BYTES;
    for (i = 0; i < status->window_count; i++) {
        const struct mxga_integrated_window *window = &status->windows[i];
        uint8_t *record = out + at;
        mx_w64(record, 0, window->window_id);
        mx_w32(record, 8, (uint32_t)window->x);
        mx_w32(record, 12, (uint32_t)window->y);
        mx_w32(record, 16, window->width);
        mx_w32(record, 20, window->height);
        mx_w32(record, 24, window->flags);
        mx_w16(record, 28, window->title_bytes);
        mx_w16(record, 30, window->application_id_bytes);
        mx_w16(record, 32, window->icon_width);
        mx_w16(record, 34, window->icon_height);
        mx_w32(record, 36, window->icon_bgra_bytes);
        mx_w16(record, 40, window->scanout_id);
        mx_w16(record, 42, window->segment_count);
        mx_w32(record, 44, window->output_width);
        mx_w32(record, 48, window->output_height);
        at += MXGA_INTEGRATION_WINDOW_BYTES;
        if (window->title_bytes)
            memcpy(out + at, window->title, window->title_bytes);
        at += window->title_bytes;
        if (window->application_id_bytes)
            memcpy(out + at, window->application_id, window->application_id_bytes);
        at += window->application_id_bytes;
        if (window->icon_bgra_bytes)
            memcpy(out + at, window->icon_bgra, window->icon_bgra_bytes);
        at += window->icon_bgra_bytes;
        for (j = 0; j < window->segment_count; j++) {
            write_segment(&window->segments[j], out + at);
            at += MXGA_INTEGRATION_SEGMENT_BYTES;
        }
    }
    if (out_len)
        *out_len = required;
    return MXGA_OK;
}

static int parse_window(const uint8_t *payload, uint32_t len, uint32_t *at,
                        struct mxga_integrated_window *window, const uint8_t **wire)
{
    const uint8_t *record;
    uint32_t content, segment_bytes;
    if (*at > len || MXGA_INTEGRATION_WINDOW_BYTES > len - *at)
        return MXGA_ERR_LENGTH;
    record = payload + *at;
    memset(window, 0, sizeof *window);
    window->window_id = mx_r64(record, 0);
    window->x = read_coordinate(record, 8);
    window->y = read_coordinate(record, 12);
    window->width = mx_r32(record, 16);
    window->height = mx_r32(record, 20);
    window->flags = mx_r32(record, 24);
    window->title_bytes = mx_r16(record, 28);
    window->application_id_bytes = mx_r16(record, 30);
    window->icon_width = mx_r16(record, 32);
    window->icon_height = mx_r16(record, 34);
    window->icon_bgra_bytes = mx_r32(record, 36);
    window->scanout_id = mx_r16(record, 40);
    window->segment_count = mx_r16(record, 42);
    window->output_width = mx_r32(record, 44);
    window->output_height = mx_r32(record, 48);
    *at += MXGA_INTEGRATION_WINDOW_BYTES;
    content = (uint32_t)window->title_bytes + window->application_id_bytes;
    if (window->icon_bgra_bytes > len - *at || content > len - *at - window->icon_bgra_bytes)
        return MXGA_ERR_LENGTH;
    window->title = payload + *at;
    *at += window->title_bytes;
    window->application_id = payload + *at;
    *at += window->application_id_bytes;
    window->icon_bgra = payload + *at;
    *at += window->icon_bgra_bytes;
    segment_bytes = (uint32_t)window->segment_count * MXGA_INTEGRATION_SEGMENT_BYTES;
    if (segment_bytes > len - *at)
        return MXGA_ERR_LENGTH;
    *wire = payload + *at;
    *at += segment_bytes;
    return window_valid(window, *wire);
}

int mxga_decode_integration_status(const uint8_t *payload, uint32_t len,
                                   struct mxga_integration_status *status,
                                   struct mxga_integrated_window *windows, uint32_t window_cap,
                                   struct mxga_integration_segment *segments, uint32_t segment_cap)
{
    struct mxga_integration_status decoded;
    uint32_t i, j, at, used = 0;
    if (!payload || !status || len < MXGA_INTEGRATION_STATUS_HEADER_BYTES ||
        len > MXGA_MAX_FRAME_BYTES - MXGA_HEADER_BYTES)
        return MXGA_ERR_LENGTH;
    if (mx_r16(payload, 0) != MXGA_INTEGRATION_STATUS_VERSION)
        return MXGA_ERR_VERSION;
    decoded.flags = mx_r16(payload, 2);
    decoded.generation = mx_r64(payload, 4);
    decoded.window_count = mx_r32(payload, 12);
    decoded.windows = windows;
    if (!decoded.generation || decoded.window_count > MXGA_INTEGRATION_MAX_WINDOWS)
        return MXGA_ERR_PAYLOAD;
    if (decoded.flags & ~MXGA_INTEGRATION_REQUIRED_FLAGS)
        return MXGA_ERR_FLAGS;
    if (decoded.window_count > window_cap || (decoded.window_count && !windows))
        return MXGA_ERR_CAPACITY;
    at = MXGA_INTEGRATION_STATUS_HEADER_BYTES;
    for (i = 0; i < decoded.window_count; i++) {
        struct mxga_integrated_window window;
        const uint8_t *wire;
        int result = parse_window(payload, len, &at, &window, &wire);
        if (result)
            return result;
        if (!segments || used > segment_cap || window.segment_count > segment_cap - used)
            return MXGA_ERR_CAPACITY;
        used += window.segment_count;
    }
    if (at != len)
        return MXGA_ERR_LENGTH;
    at = MXGA_INTEGRATION_STATUS_HEADER_BYTES;
    used = 0;
    for (i = 0; i < decoded.window_count; i++) {
        struct mxga_integrated_window window;
        const uint8_t *wire;
        int result = parse_window(payload, len, &at, &window, &wire);
        if (result)
            return result;
        window.segments = segments + used;
        for (j = 0; j < window.segment_count; j++)
            read_segment(wire + j * MXGA_INTEGRATION_SEGMENT_BYTES, &segments[used++]);
        windows[i] = window;
    }
    *status = decoded;
    return MXGA_OK;
}

static int action_valid(const struct mxga_integration_action *action)
{
    if (!action || !action->generation || !action->window_id ||
        (action->action != MXGA_INTEGRATION_ACTION_ACTIVATE &&
         action->action != MXGA_INTEGRATION_ACTION_CLOSE))
        return MXGA_ERR_PAYLOAD;
    return MXGA_OK;
}

int mxga_encode_integration_action(const struct mxga_integration_action *action, uint8_t *out,
                                   uint32_t cap, uint32_t *out_len)
{
    int result = action_valid(action);
    if (out_len)
        *out_len = 0;
    if (result)
        return result;
    if (!out || cap < MXGA_INTEGRATION_ACTION_BYTES)
        return MXGA_ERR_CAPACITY;
    mx_w16(out, 0, MXGA_INTEGRATION_ACTION_VERSION);
    mx_w16(out, 2, action->action);
    mx_w64(out, 4, action->generation);
    mx_w64(out, 12, action->window_id);
    if (out_len)
        *out_len = MXGA_INTEGRATION_ACTION_BYTES;
    return MXGA_OK;
}

int mxga_decode_integration_action(const uint8_t *payload, uint32_t len,
                                   struct mxga_integration_action *action)
{
    struct mxga_integration_action decoded;
    int result;
    if (!payload || !action || len != MXGA_INTEGRATION_ACTION_BYTES)
        return MXGA_ERR_LENGTH;
    if (mx_r16(payload, 0) != MXGA_INTEGRATION_ACTION_VERSION)
        return MXGA_ERR_VERSION;
    decoded.action = mx_r16(payload, 2);
    decoded.generation = mx_r64(payload, 4);
    decoded.window_id = mx_r64(payload, 12);
    result = action_valid(&decoded);
    if (result)
        return result;
    *action = decoded;
    return MXGA_OK;
}
