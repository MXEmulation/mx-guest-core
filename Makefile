# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Zak Noble-Clarke

CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror
CPPFLAGS += -Iinclude -Isrc
LDFLAGS ?=

MANIFEST = $(shell cat sources.list)
SRCS = $(filter src/%.c,$(MANIFEST))
HEADERS = $(filter %.h,$(MANIFEST))

.PHONY: test clean

test: build/test_protocol build/test_mxga build/test_mxga_integration build/test_cursor build/test_render_decode build/test_mxsb_verify build/test_mxio build/test_aperture build/test_vertex build/test_stage_interface
	./build/test_protocol tests/fixtures
	./build/test_mxga
	./build/test_mxga_integration
	./build/test_cursor
	./build/test_render_decode
	./build/test_mxsb_verify
	./build/test_mxio
	./build/test_aperture
	./build/test_vertex
	./build/test_stage_interface

build/test_protocol: tests/test_protocol.c $(SRCS) $(HEADERS) sources.list
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_protocol.c $(SRCS) $(LDFLAGS) -o $@

build/test_mxga: tests/test_mxga.c src/mxga_codec.c $(HEADERS) sources.list
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_mxga.c src/mxga_codec.c $(LDFLAGS) -o $@

build/test_mxga_integration: tests/test_mxga_integration.c src/mxga_integration.c src/mxga_codec.c $(HEADERS) sources.list
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_mxga_integration.c src/mxga_integration.c src/mxga_codec.c $(LDFLAGS) -o $@

build/test_cursor: tests/test_cursor.c src/mxgpu_codec.c $(HEADERS) sources.list
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_cursor.c src/mxgpu_codec.c $(LDFLAGS) -o $@

build/test_render_decode: tests/test_render_decode.c src/mxgpu_codec.c $(HEADERS) sources.list
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_render_decode.c src/mxgpu_codec.c $(LDFLAGS) -o $@

build/test_mxsb_verify: tests/test_mxsb_verify.c src/mxsb_codec.c $(HEADERS) sources.list
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_mxsb_verify.c src/mxsb_codec.c $(LDFLAGS) -o $@

build/test_mxio: tests/test_mxio.c src/mxio_codec.c $(HEADERS) sources.list
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_mxio.c src/mxio_codec.c $(LDFLAGS) -o $@

build/test_aperture: tests/test_aperture.c src/mxgpu_aperture_codec.c $(HEADERS) sources.list
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_aperture.c src/mxgpu_aperture_codec.c $(LDFLAGS) -o $@

build/test_vertex: tests/test_vertex.c src/mxgpu_vertex_codec.c src/mxgpu_codec.c $(HEADERS) sources.list
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_vertex.c src/mxgpu_vertex_codec.c src/mxgpu_codec.c $(LDFLAGS) -o $@

build/test_stage_interface: tests/test_stage_interface.c src/mxsb_codec.c $(HEADERS) sources.list
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_stage_interface.c src/mxsb_codec.c $(LDFLAGS) -o $@

clean:
	rm -rf build
