/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxga.h"

#include "mx_le.h"

#include <string.h>

int mxga_encode_frame(uint16_t minor, uint16_t opcode, uint64_t sequence, const uint8_t *payload,
                      uint32_t payload_len, uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint32_t total;
    if (out_len)
        *out_len = 0;
    if (minor > MXGA_PROTOCOL_MINOR || (payload_len && !payload))
        return MXGA_ERR_VERSION;
    if (payload_len > MXGA_MAX_FRAME_BYTES - MXGA_HEADER_BYTES)
        return MXGA_ERR_LENGTH;
    total = MXGA_HEADER_BYTES + payload_len;
    if (!out || cap < total)
        return MXGA_ERR_LENGTH;
    memset(out, 0, total);
    mx_w32(out, 0, MXGA_MAGIC);
    mx_w16(out, 4, MXGA_PROTOCOL_MAJOR);
    mx_w16(out, 6, minor);
    mx_w16(out, 8, opcode);
    mx_w16(out, 10, 0);
    mx_w64(out, 12, sequence);
    mx_w32(out, 20, payload_len);
    if (payload_len)
        memcpy(out + MXGA_HEADER_BYTES, payload, payload_len);
    if (out_len)
        *out_len = total;
    return MXGA_OK;
}

int mxga_decode_frame(const uint8_t *in, uint32_t len, struct mxga_frame *out)
{
    uint32_t payload_len;
    uint16_t major;
    if (!in || !out || len < MXGA_HEADER_BYTES)
        return MXGA_ERR_LENGTH;
    if (mx_r32(in, 0) != MXGA_MAGIC)
        return MXGA_ERR_MAGIC;
    major = mx_r16(in, 4);
    out->minor = mx_r16(in, 6);
    if (major != MXGA_PROTOCOL_MAJOR || out->minor > MXGA_PROTOCOL_MINOR)
        return MXGA_ERR_VERSION;
    if (mx_r16(in, 10) != 0)
        return MXGA_ERR_FLAGS;
    out->opcode = mx_r16(in, 8);
    out->sequence = mx_r64(in, 12);
    payload_len = mx_r32(in, 20);
    if ((uint64_t)MXGA_HEADER_BYTES + payload_len != len || len > MXGA_MAX_FRAME_BYTES)
        return MXGA_ERR_LENGTH;
    out->payload_len = payload_len;
    out->payload = payload_len ? in + MXGA_HEADER_BYTES : NULL;
    return MXGA_OK;
}

int mxga_encode_capabilities(uint64_t capabilities, uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    if (out_len)
        *out_len = 0;
    if (!out || cap < 8)
        return MXGA_ERR_LENGTH;
    mx_w64(out, 0, capabilities);
    if (out_len)
        *out_len = 8;
    return MXGA_OK;
}

int mxga_decode_capabilities(const uint8_t *payload, uint32_t len, uint64_t *capabilities)
{
    if (!payload || !capabilities || len != 8)
        return MXGA_ERR_PAYLOAD;
    *capabilities = mx_r64(payload, 0);
    return MXGA_OK;
}

int mxga_encode_command_result(uint16_t command_opcode, uint16_t status, uint8_t *out, uint32_t cap,
                               uint32_t *out_len)
{
    if (out_len)
        *out_len = 0;
    if (!out || cap < 4)
        return MXGA_ERR_LENGTH;
    mx_w16(out, 0, command_opcode);
    mx_w16(out, 2, status);
    if (out_len)
        *out_len = 4;
    return MXGA_OK;
}

int mxga_encode_system_stats(const struct mxga_system_stats *stats, uint8_t *out, uint32_t cap,
                             uint32_t *out_len)
{
    uint32_t total;
    uint32_t i;
    if (out_len)
        *out_len = 0;
    if (!stats || (stats->cpu_count && !stats->cpus))
        return MXGA_ERR_PAYLOAD;
    if (stats->cpu_count > MXGA_MAX_REPORTED_CPUS)
        return MXGA_ERR_PAYLOAD;
    total = MXGA_STATS_HEADER_BYTES + stats->cpu_count * MXGA_STATS_CPU_BYTES;
    if (!out || cap < total)
        return MXGA_ERR_LENGTH;
    memset(out, 0, total);
    mx_w64(out, 0, stats->uptime_seconds);
    mx_w64(out, 8, stats->memory_total_bytes);
    mx_w64(out, 16, stats->memory_available_bytes);
    mx_w64(out, 24, stats->memory_used_bytes);
    mx_w64(out, 32, stats->swap_total_bytes);
    mx_w64(out, 40, stats->swap_used_bytes);
    mx_w32(out, 48, stats->load_average_permille[0]);
    mx_w32(out, 52, stats->load_average_permille[1]);
    mx_w32(out, 56, stats->load_average_permille[2]);
    mx_w32(out, 60, stats->process_count);
    mx_w32(out, 64, stats->cpu_usage_permille);
    mx_w32(out, 68, stats->cpu_count);
    for (i = 0; i < stats->cpu_count; i++) {
        uint32_t base = MXGA_STATS_HEADER_BYTES + i * MXGA_STATS_CPU_BYTES;
        const struct mxga_cpu_stat *cpu = &stats->cpus[i];
        if (cpu->usage_permille > 1000u || cpu->frequency_source > MXGA_CPU_FREQ_CALIBRATION_LOOP)
            return MXGA_ERR_PAYLOAD;
        mx_w32(out, base, cpu->usage_permille);
        mx_w32(out, base + 4, cpu->current_khz);
        mx_w32(out, base + 8, cpu->minimum_khz);
        mx_w32(out, base + 12, cpu->maximum_khz);
        mx_w32(out, base + 16, cpu->frequency_source);
    }
    if (out_len)
        *out_len = total;
    return MXGA_OK;
}

int mxga_decode_system_stats(const uint8_t *payload, uint32_t len, struct mxga_system_stats *stats,
                             struct mxga_cpu_stat *cpus, uint32_t cpu_cap)
{
    uint32_t count;
    uint32_t i;
    if (!payload || !stats || len < MXGA_STATS_HEADER_BYTES)
        return MXGA_ERR_PAYLOAD;
    count = mx_r32(payload, 68);
    if (count > MXGA_MAX_REPORTED_CPUS)
        return MXGA_ERR_PAYLOAD;
    if (len != MXGA_STATS_HEADER_BYTES + count * MXGA_STATS_CPU_BYTES)
        return MXGA_ERR_LENGTH;
    if (count && (!cpus || cpu_cap < count))
        return MXGA_ERR_PAYLOAD;
    stats->uptime_seconds = mx_r64(payload, 0);
    stats->memory_total_bytes = mx_r64(payload, 8);
    stats->memory_available_bytes = mx_r64(payload, 16);
    stats->memory_used_bytes = mx_r64(payload, 24);
    stats->swap_total_bytes = mx_r64(payload, 32);
    stats->swap_used_bytes = mx_r64(payload, 40);
    stats->load_average_permille[0] = mx_r32(payload, 48);
    stats->load_average_permille[1] = mx_r32(payload, 52);
    stats->load_average_permille[2] = mx_r32(payload, 56);
    stats->process_count = mx_r32(payload, 60);
    stats->cpu_usage_permille = mx_r32(payload, 64);
    stats->cpu_count = count;
    stats->cpus = cpus;
    for (i = 0; i < count; i++) {
        uint32_t base = MXGA_STATS_HEADER_BYTES + i * MXGA_STATS_CPU_BYTES;
        cpus[i].usage_permille = mx_r32(payload, base);
        cpus[i].current_khz = mx_r32(payload, base + 4);
        cpus[i].minimum_khz = mx_r32(payload, base + 8);
        cpus[i].maximum_khz = mx_r32(payload, base + 12);
        cpus[i].frequency_source = mx_r32(payload, base + 16);
        if (cpus[i].frequency_source > MXGA_CPU_FREQ_CALIBRATION_LOOP)
            return MXGA_ERR_PAYLOAD;
    }
    return MXGA_OK;
}
