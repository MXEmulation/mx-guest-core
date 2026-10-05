/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxgpu_wire.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void record(struct mxgpu_render_extended *r, struct mxgpu_execution_binding *bindings,
                   int maximum)
{
    uint32_t i;
    memset(r, 0, sizeof *r);
    memset(bindings, 0, 32 * sizeof *bindings);
    r->pipeline_id = 1;
    r->rasterizer_state_id = 2;
    r->blend_state_id = 3;
    r->color_target_count = maximum ? 8 : 1;
    r->viewport_count = maximum ? 16 : 1;
    r->scissor_count = maximum ? 16 : 1;
    r->vertex_buffer_count = maximum ? 32 : 1;
    r->binding_count = maximum ? 32 : 1;
    r->draw_kind = MXGPU_DRAW_NON_INDEXED;
    r->primitive = MXGPU_PRIM_TRIANGLE;
    r->element_count = 6;
    r->instance_count = 2;
    r->blend_factor[0] = 0x3f800000;
    for (i = 0; i < r->color_target_count; i++) {
        r->color_targets[i].resource_id = 20 + i;
        r->color_targets[i].format = MXGPU_FMT_BGRA8_UNORM;
        r->color_targets[i].load_action = MXGPU_LOAD_CLEAR;
        r->color_targets[i].store_action = MXGPU_STORE_STORE;
        r->color_targets[i].clear_rgba[3] = 0x3f800000;
    }
    for (i = 0; i < r->viewport_count; i++)
        r->viewports[i] = (struct mxgpu_viewport){0, 0, 0x42800000, 0x42800000, 0, 0x3f800000};
    for (i = 0; i < r->scissor_count; i++)
        r->scissors[i] = (struct mxgpu_scissor){0, 0, 64, 64};
    for (i = 0; i < r->vertex_buffer_count; i++)
        r->vertex_buffers[i] = (struct mxgpu_vertex_buffer_binding){100 + i, 16, i * 16};
    for (i = 0; i < r->binding_count; i++) {
        bindings[i].slot = (uint16_t)i;
        bindings[i].access = MXGPU_BIND_ACCESS_READ;
        bindings[i].kind = MXGPU_BIND_KIND_BUFFER;
        bindings[i].resource_id = 100 + i;
        bindings[i].size = 16;
    }
}

int main(int argc, char **argv)
{
    struct mxgpu_render_extended r, decoded, before;
    struct mxgpu_execution_binding bindings[32], output[32], untouched[32];
    uint8_t wire[4096], damaged[4096];
    union {
        max_align_t alignment;
        uint8_t bytes[8192];
    } alias;
    uint32_t size, i, bit, checks = 0;
    int maximum, rc;
    for (maximum = 0; maximum <= 1; maximum++) {
        record(&r, bindings, maximum);
        assert(mxgpu_render_extended_encode(&r, bindings, wire, sizeof wire, &size) == MX_OK);
        assert(mxgpu_render_extended_decode(wire, size, &decoded, output, 32) == MX_OK);
        assert(memcmp(&decoded, &r, sizeof r) == 0);
        assert(memcmp(output, bindings, r.binding_count * sizeof *bindings) == 0);
        memset(&before, 0x5a, sizeof before);
        memset(untouched, 0x6b, sizeof untouched);
        for (i = 0; i < size; i++) {
            decoded = before;
            memcpy(output, untouched, sizeof output);
            assert(mxgpu_render_extended_decode(wire, i, &decoded, output, 32) != MX_OK);
            assert(memcmp(&decoded, &before, sizeof before) == 0);
            assert(memcmp(output, untouched, sizeof output) == 0);
            checks++;
        }
        memcpy(damaged, wire, size);
        for (i = 0; i < size; i++) {
            for (bit = 0; bit < 8; bit++) {
                damaged[i] ^= (uint8_t)(1u << bit);
                decoded = before;
                memcpy(output, untouched, sizeof output);
                rc = mxgpu_render_extended_decode(damaged, size, &decoded, output, 32);
                if (rc != MX_OK) {
                    assert(memcmp(&decoded, &before, sizeof before) == 0);
                    assert(memcmp(output, untouched, sizeof output) == 0);
                }
                damaged[i] ^= (uint8_t)(1u << bit);
                checks++;
            }
        }
        for (i = 0; i < r.binding_count; i++) {
            decoded = before;
            memcpy(output, untouched, sizeof output);
            assert(mxgpu_render_extended_decode(wire, size, &decoded, output, i) == MX_ERR_LENGTH);
            assert(memcmp(&decoded, &before, sizeof before) == 0);
            assert(memcmp(output, untouched, sizeof output) == 0);
        }
        for (i = 0; i < 3; i++) {
            uint32_t input_offset = 1024;
            uint32_t output_offset = i == 0 ? 1024 : i == 1 ? 1088 : 960;
            struct mxgpu_render_extended *overlap = (void *)(alias.bytes + output_offset);
            memcpy(alias.bytes + input_offset, wire, size);
            assert(mxgpu_render_extended_decode(alias.bytes + input_offset, size, overlap, output,
                                                32) == MX_OK);
            assert(memcmp(overlap, &r, sizeof r) == 0);
            assert(memcmp(output, bindings, r.binding_count * sizeof *bindings) == 0);
        }
        if (argc == 2 && maximum) {
            FILE *file = fopen(argv[1], "wb");
            assert(file);
            assert(fwrite(wire, 1, size, file) == size);
            assert(fclose(file) == 0);
        }
    }
    printf(
        "render decode maximum shapes, %u hostile mutations, atomic refusal, capacities and overlapping buffers PASS\n",
        checks);
    return 0;
}
