/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxga.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "share test failed line %d: %s\n", __LINE__, #condition);              \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static uint8_t buffer[MXGA_MAX_FRAME_BYTES];
static uint8_t data[MXGA_FS_MAX_IO_BYTES + 1];

static uint32_t r32(const uint8_t *p, uint32_t at)
{
    return (uint32_t)p[at] | (uint32_t)p[at + 1] << 8 | (uint32_t)p[at + 2] << 16 |
           (uint32_t)p[at + 3] << 24;
}

static void test_network(void)
{
    struct mxga_network_address addresses[3] = {
        {MXGA_NETWORK_FAMILY_IPV4, 24, {10, 0, 0, 15}},
        {MXGA_NETWORK_FAMILY_IPV6, 64, {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8}},
        {MXGA_NETWORK_FAMILY_IPV4, 8, {127, 0, 0, 1}}};
    struct mxga_network_interface interfaces[2], got[2];
    struct mxga_network_address got_addresses[3];
    uint32_t len = 0, count = 0;
    memset(interfaces, 0, sizeof interfaces);
    memcpy(interfaces[0].name, "enp0s4", 6);
    memcpy(interfaces[0].mac, "\x02\x11\x22\x33\x44\x55", 6);
    interfaces[0].flags = MXGA_NETWORK_FLAG_UP | MXGA_NETWORK_FLAG_RUNNING;
    interfaces[0].address_count = 2;
    interfaces[0].addresses = addresses;
    memcpy(interfaces[1].name, "abcdefghijklmnop", 16);
    interfaces[1].flags = MXGA_NETWORK_FLAG_LOOPBACK | MXGA_NETWORK_FLAG_POINT_TO_POINT;
    interfaces[1].address_count = 1;
    interfaces[1].addresses = addresses + 2;
    CHECK(mxga_encode_network_info(interfaces, 2, buffer, sizeof buffer, &len) == MXGA_OK);
    CHECK(len == 8 + 2 * 28 + 3 * 20);
    CHECK(r32(buffer, 0) == 2 && r32(buffer, 4) == 0);
    CHECK(!memcmp(buffer + 8, "enp0s4\0\0\0\0\0\0\0\0\0\0", 16) && buffer[24] == 2);
    CHECK(buffer[30] == 3 && buffer[31] == 0 && r32(buffer, 32) == 2);
    CHECK(buffer[36] == 4 && buffer[37] == 24 && buffer[40] == 10 && buffer[43] == 15 &&
          buffer[44] == 0 && buffer[55] == 0);
    CHECK(buffer[56] == 6 && buffer[57] == 64 && buffer[60] == 0xfe && buffer[75] == 8);
    CHECK(!memcmp(buffer + 76, "abcdefghijklmnop", 16) && buffer[98] == 12);
    CHECK(mxga_decode_network_info(buffer, len, got, 2, got_addresses, 3, &count) == MXGA_OK);
    CHECK(count == 2 && got[0].address_count == 2 && got[1].addresses == got_addresses + 2 &&
          got[0].addresses[1].prefix_length == 64 && got[1].flags == interfaces[1].flags);
    CHECK(mxga_decode_network_info(buffer, len - 1, got, 2, got_addresses, 3, &count) ==
          MXGA_ERR_LENGTH);
    CHECK(mxga_decode_network_info(buffer, len, got, 2, got_addresses, 2, &count) ==
          MXGA_ERR_CAPACITY);
    buffer[len] = 0;
    CHECK(mxga_decode_network_info(buffer, len + 1, got, 2, got_addresses, 3, &count) ==
          MXGA_ERR_LENGTH);
    CHECK(mxga_encode_network_info(interfaces, 2, buffer, len - 1, &len) == MXGA_ERR_LENGTH &&
          len == 0);
    interfaces[0].flags = 16;
    CHECK(mxga_encode_network_info(interfaces, 2, buffer, sizeof buffer, &len) ==
          MXGA_ERR_PAYLOAD);
    interfaces[0].flags = MXGA_NETWORK_FLAG_UP;
    addresses[0].prefix_length = 33;
    CHECK(mxga_encode_network_info(interfaces, 2, buffer, sizeof buffer, &len) ==
          MXGA_ERR_PAYLOAD);
    addresses[0].prefix_length = 32;
    addresses[1].prefix_length = 129;
    CHECK(mxga_encode_network_info(interfaces, 2, buffer, sizeof buffer, &len) ==
          MXGA_ERR_PAYLOAD);
    addresses[1].prefix_length = 128;
    addresses[1].family = 5;
    CHECK(mxga_encode_network_info(interfaces, 2, buffer, sizeof buffer, &len) ==
          MXGA_ERR_PAYLOAD);
    addresses[1].family = MXGA_NETWORK_FAMILY_IPV6;
    interfaces[0].name[0] = 0xff;
    CHECK(mxga_encode_network_info(interfaces, 2, buffer, sizeof buffer, &len) == MXGA_ERR_UTF8);
    interfaces[0].name[0] = 'e';
    interfaces[0].address_count = MXGA_NETWORK_MAX_ADDRESSES + 1;
    CHECK(mxga_encode_network_info(interfaces, 2, buffer, sizeof buffer, &len) ==
          MXGA_ERR_PAYLOAD);
    CHECK(mxga_encode_network_info(interfaces, MXGA_NETWORK_MAX_INTERFACES + 1, buffer,
                                   sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    CHECK(mxga_encode_network_info(NULL, 0, buffer, sizeof buffer, &len) == MXGA_OK && len == 8);
    CHECK(mxga_decode_network_info(buffer, 8, NULL, 0, NULL, 0, &count) == MXGA_OK && !count);
}

static void test_mount(void)
{
    struct mxga_share_mount mount, got;
    uint32_t len = 0, id = 0;
    memset(&mount, 0, sizeof mount);
    mount.share_id = 7;
    mount.flags = MXGA_SHARE_FLAG_READ_ONLY | MXGA_SHARE_FLAG_AUTOMOUNT;
    memcpy(mount.name, "Documents", 9);
    mount.name_bytes = 9;
    memcpy(mount.mount_point, "/media/docs", 11);
    mount.mount_point_bytes = 11;
    CHECK(mxga_encode_mount_share(&mount, buffer, sizeof buffer, &len) == MXGA_OK && len == 328);
    CHECK(r32(buffer, 0) == 7 && r32(buffer, 4) == 3 && !memcmp(buffer + 8, "Documents", 9) &&
          buffer[17] == 0 && !memcmp(buffer + 72, "/media/docs", 11));
    CHECK(mxga_decode_mount_share(buffer, len, &got) == MXGA_OK && got.share_id == 7 &&
          got.name_bytes == 9 && !strcmp(got.name, "Documents") &&
          !strcmp(got.mount_point, "/media/docs") && got.flags == 3);
    CHECK(mxga_decode_mount_share(buffer, len - 1, &got) == MXGA_ERR_LENGTH);
    buffer[4] = 4;
    CHECK(mxga_decode_mount_share(buffer, len, &got) == MXGA_ERR_FLAGS);
    buffer[4] = 3;
    buffer[8] = 0;
    CHECK(mxga_decode_mount_share(buffer, len, &got) == MXGA_ERR_PAYLOAD);
    memset(buffer + 8, 'n', 64);
    memset(buffer + 72, 0, 256);
    CHECK(mxga_decode_mount_share(buffer, len, &got) == MXGA_OK && got.name_bytes == 64 &&
          got.mount_point_bytes == 0 && got.name[64] == 0);
    buffer[72 + 5] = 'x';
    CHECK(mxga_decode_mount_share(buffer, len, &got) == MXGA_ERR_PAYLOAD);
    CHECK(mxga_encode_unmount_share(9, buffer, sizeof buffer, &len) == MXGA_OK && len == 8);
    CHECK(mxga_decode_unmount_share(buffer, len, &id) == MXGA_OK && id == 9);
    buffer[4] = 1;
    CHECK(mxga_decode_unmount_share(buffer, len, &id) == MXGA_ERR_PAYLOAD);
    CHECK(mxga_decode_unmount_share(buffer, 7, &id) == MXGA_ERR_LENGTH);
}

static void test_share_status(void)
{
    struct mxga_share_status shares[2], got[2];
    uint32_t len = 0, count = 0;
    memset(shares, 0, sizeof shares);
    shares[0].share_id = 1;
    shares[0].state = MXGA_SHARE_STATE_MOUNTED;
    shares[0].flags = MXGA_SHARE_FLAG_AUTOMOUNT;
    shares[0].name = (const uint8_t *)"home";
    shares[0].name_bytes = 4;
    shares[0].mount_point = (const uint8_t *)"/media/mxshare/home";
    shares[0].mount_point_bytes = 19;
    shares[1].share_id = 2;
    shares[1].state = MXGA_SHARE_STATE_FAILED;
    shares[1].error = -13;
    shares[1].name = (const uint8_t *)"x";
    shares[1].name_bytes = 1;
    CHECK(mxga_encode_share_status(shares, 2, buffer, sizeof buffer, &len) == MXGA_OK);
    CHECK(len == 8 + 2 * 336 && r32(buffer, 0) == 2 && r32(buffer, 8) == 1 &&
          r32(buffer, 12) == 1 && r32(buffer, 20) == 2 && !memcmp(buffer + 24, "home", 4) &&
          !memcmp(buffer + 88, "/media/mxshare/home", 19) && r32(buffer, 344) == 2 &&
          r32(buffer, 348) == 2 && r32(buffer, 352) == (uint32_t)-13);
    CHECK(mxga_decode_share_status(buffer, len, got, 2, &count) == MXGA_OK && count == 2 &&
          got[0].mount_point_bytes == 19 && got[1].error == -13);
    CHECK(mxga_encode_share_status(NULL, 0, buffer, sizeof buffer, &len) == MXGA_OK && len == 8);
    shares[1].error = 0;
    CHECK(mxga_encode_share_status(shares, 2, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    shares[1].error = -5;
    shares[1].state = 3;
    CHECK(mxga_encode_share_status(shares, 2, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    shares[1].state = MXGA_SHARE_STATE_FAILED;
    shares[1].flags = 4;
    CHECK(mxga_encode_share_status(shares, 2, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    shares[1].flags = 0;
    shares[0].error = -1;
    CHECK(mxga_encode_share_status(shares, 2, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    shares[0].error = 0;
    shares[0].name_bytes = 65;
    CHECK(mxga_encode_share_status(shares, 2, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    shares[0].name = (const uint8_t *)"a\0b";
    shares[0].name_bytes = 3;
    CHECK(mxga_encode_share_status(shares, 2, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    shares[0].name = (const uint8_t *)"\xc0\x80";
    shares[0].name_bytes = 2;
    CHECK(mxga_encode_share_status(shares, 2, buffer, sizeof buffer, &len) == MXGA_ERR_UTF8);
    CHECK(mxga_encode_share_status(shares, MXGA_SHARE_MAX + 1, buffer, sizeof buffer, &len) ==
          MXGA_ERR_PAYLOAD);
}

static void test_paths(void)
{
    static const char *good[] = {"", "a", "a/b", "dir/file.txt", "\xc3\xa9t\xc3\xa9", "..a/.b"};
    static const char *bad[] = {"/a", "a/", "a//b", ".", "..", "a/./b", "a/../b", "a\\b", "\xff"};
    char long_name[257], long_path[4098];
    unsigned i;
    for (i = 0; i < sizeof good / sizeof good[0]; i++)
        CHECK(mxga_fs_path_valid((const uint8_t *)good[i], (uint32_t)strlen(good[i])) == MXGA_OK);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++)
        CHECK(mxga_fs_path_valid((const uint8_t *)bad[i], (uint32_t)strlen(bad[i])) != MXGA_OK);
    CHECK(mxga_fs_path_valid((const uint8_t *)"a\0b", 3) == MXGA_ERR_PAYLOAD);
    memset(long_name, 'n', sizeof long_name);
    CHECK(mxga_fs_path_valid((const uint8_t *)long_name, 255) == MXGA_OK);
    CHECK(mxga_fs_path_valid((const uint8_t *)long_name, 256) == MXGA_ERR_PAYLOAD);
    for (i = 0; i < sizeof long_path; i++)
        long_path[i] = i % 64 == 62 ? '/' : 'p';
    CHECK(mxga_fs_path_valid((const uint8_t *)long_path, 4096) == MXGA_OK);
    CHECK(mxga_fs_path_valid((const uint8_t *)long_path, 4097) == MXGA_ERR_PAYLOAD);
}

static void test_fs_request(void)
{
    struct mxga_fs_request request, got;
    uint8_t attributes[MXGA_FS_ATTRIBUTES_BYTES] = {0};
    uint32_t len = 0;
    memset(&request, 0, sizeof request);
    request.request_id = 0x1122334455667788ull;
    request.share_id = 3;
    request.operation = MXGA_FS_OP_RENAME;
    request.path = (const uint8_t *)"a/b";
    request.path_bytes = 3;
    request.second_path = (const uint8_t *)"c";
    request.second_path_bytes = 1;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_OK && len == 60);
    CHECK(buffer[0] == 0x88 && r32(buffer, 24) == 3 && r32(buffer, 28) == 11 &&
          r32(buffer, 44) == 3 && r32(buffer, 48) == 1 && r32(buffer, 52) == 0 &&
          !memcmp(buffer + 56, "a/bc", 4));
    CHECK(mxga_decode_fs_request(buffer, len, &got) == MXGA_OK && got.path_bytes == 3 &&
          got.second_path_bytes == 1 && got.second_path == buffer + 59 && !got.data);
    CHECK(mxga_decode_fs_request(buffer, len + 1, &got) == MXGA_ERR_LENGTH);
    request.second_path_bytes = 0;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    request.operation = MXGA_FS_OP_GETATTR;
    request.path_bytes = 0;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_OK && len == 56);
    request.operation = MXGA_FS_OP_UNLINK;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    request.operation = MXGA_FS_OP_READ;
    request.length = MXGA_FS_MAX_IO_BYTES;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_OK);
    request.length = MXGA_FS_MAX_IO_BYTES + 1;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    request.length = 4096;
    request.path_bytes = 1;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    request.path_bytes = 0;
    request.operation = MXGA_FS_OP_WRITE;
    request.length = 0;
    request.data = data;
    request.data_bytes = MXGA_FS_MAX_IO_BYTES;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_OK &&
          len == 56 + MXGA_FS_MAX_IO_BYTES);
    CHECK(mxga_decode_fs_request(buffer, len, &got) == MXGA_OK &&
          got.data_bytes == MXGA_FS_MAX_IO_BYTES && got.data == buffer + 56);
    request.data_bytes = MXGA_FS_MAX_IO_BYTES + 1;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_ERR_LENGTH);
    request.operation = MXGA_FS_OP_SETATTR;
    request.flags = MXGA_FS_SETATTR_SIZE;
    request.data = attributes;
    request.data_bytes = 79;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    request.data_bytes = 80;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_OK);
    request.flags = 0x40;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    memset(&request, 0, sizeof request);
    request.operation = MXGA_FS_OP_CREATE;
    request.path = (const uint8_t *)"new";
    request.path_bytes = 3;
    request.flags = MXGA_FS_OPEN_WRITE | MXGA_FS_OPEN_EXCLUSIVE;
    request.mode = 0644;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_OK);
    request.mode = 010000;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    request.mode = 0;
    request.flags = 0x80;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    request.flags = 0;
    request.operation = MXGA_FS_OP_UNLINK;
    request.mode = 0644;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    request.mode = 0;
    request.path = (const uint8_t *)"../x";
    request.path_bytes = 4;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    request.operation = 17;
    request.path = (const uint8_t *)"x";
    request.path_bytes = 1;
    CHECK(mxga_encode_fs_request(&request, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    request.operation = MXGA_FS_OP_STATFS;
    CHECK(mxga_encode_fs_request(&request, buffer, 56, &len) == MXGA_ERR_LENGTH && len == 0);
}

static void test_fs_response(void)
{
    struct mxga_fs_response response, got;
    struct mxga_fs_attributes attributes, decoded_attributes;
    struct mxga_fs_statfs statfs = {4096, 10, 9, 8, 7, 6, 255}, decoded_statfs;
    struct mxga_fs_dirent entry, decoded_entry;
    uint8_t records[64];
    uint32_t len = 0, used = 0, cursor = 0;
    memset(&response, 0, sizeof response);
    response.request_id = 5;
    response.handle = 9;
    response.status = -2;
    response.operation = MXGA_FS_OP_OPEN;
    CHECK(mxga_encode_fs_response(&response, buffer, sizeof buffer, &len) == MXGA_OK && len == 32);
    CHECK(mxga_decode_fs_response(buffer, len, &got) == MXGA_OK && got.request_id == 5 &&
          got.handle == 9 && got.status == -2 && !got.data_bytes && !got.data);
    buffer[28] = 1;
    CHECK(mxga_decode_fs_response(buffer, len, &got) == MXGA_ERR_PAYLOAD);
    buffer[28] = 0;
    buffer[16] = 1;
    buffer[17] = buffer[18] = buffer[19] = 0;
    CHECK(mxga_decode_fs_response(buffer, len, &got) == MXGA_ERR_PAYLOAD);
    response.status = 0;
    response.data = (const uint8_t *)"abc";
    response.data_bytes = 3;
    CHECK(mxga_encode_fs_response(&response, buffer, sizeof buffer, &len) == MXGA_OK && len == 35);
    CHECK(mxga_decode_fs_response(buffer, len - 1, &got) == MXGA_ERR_LENGTH);
    CHECK(mxga_decode_fs_response(buffer, len, &got) == MXGA_OK && got.data == buffer + 32);
    CHECK(mxga_decode_fs_response(buffer, 31, &got) == MXGA_ERR_LENGTH);
    memset(&attributes, 0, sizeof attributes);
    attributes.ino = 42;
    attributes.size = 1ull << 40;
    attributes.mode = 0100644;
    attributes.nlink = 1;
    attributes.mtime_ns = 999999999u;
    CHECK(mxga_encode_fs_attributes(&attributes, buffer, sizeof buffer, &len) == MXGA_OK &&
          len == 80 && r32(buffer, 60) == 0100644 && r32(buffer, 52) == 999999999u);
    CHECK(mxga_decode_fs_attributes(buffer, len, &decoded_attributes) == MXGA_OK &&
          !memcmp(&attributes, &decoded_attributes, sizeof attributes));
    CHECK(mxga_decode_fs_attributes(buffer, 79, &decoded_attributes) == MXGA_ERR_LENGTH);
    attributes.atime_ns = 1000000000u;
    CHECK(mxga_encode_fs_attributes(&attributes, buffer, sizeof buffer, &len) == MXGA_ERR_PAYLOAD);
    CHECK(mxga_encode_fs_statfs(&statfs, buffer, sizeof buffer, &len) == MXGA_OK && len == 56);
    CHECK(mxga_decode_fs_statfs(buffer, len, &decoded_statfs) == MXGA_OK &&
          decoded_statfs.name_max == 255 && decoded_statfs.blocks_available == 8);
    entry.inode = 77;
    entry.mode = 040755;
    entry.name = (const uint8_t *)"docs";
    entry.name_bytes = 4;
    CHECK(mxga_encode_fs_dirent(&entry, records, sizeof records, &len) == MXGA_OK && len == 20);
    used = len;
    entry.name = (const uint8_t *)"f";
    entry.name_bytes = 1;
    CHECK(mxga_encode_fs_dirent(&entry, records + used, sizeof records - used, &len) == MXGA_OK);
    used += len;
    CHECK(mxga_decode_fs_dirent(records, used, &cursor, &decoded_entry) == MXGA_OK &&
          cursor == 20 && decoded_entry.inode == 77 && decoded_entry.name_bytes == 4);
    CHECK(mxga_decode_fs_dirent(records, used, &cursor, &decoded_entry) == MXGA_OK &&
          cursor == used && decoded_entry.name[0] == 'f');
    cursor = 0;
    CHECK(mxga_decode_fs_dirent(records, 19, &cursor, &decoded_entry) == MXGA_ERR_LENGTH &&
          cursor == 0);
    entry.name = (const uint8_t *)"..";
    entry.name_bytes = 2;
    CHECK(mxga_encode_fs_dirent(&entry, records, sizeof records, &len) == MXGA_ERR_PAYLOAD);
    entry.name = (const uint8_t *)"a/b";
    entry.name_bytes = 3;
    CHECK(mxga_encode_fs_dirent(&entry, records, sizeof records, &len) == MXGA_ERR_PAYLOAD);
}

int main(void)
{
    test_network();
    test_mount();
    test_share_status();
    test_paths();
    test_fs_request();
    test_fs_response();
    if (failures)
        return 1;
    puts("mxga share codecs ok");
    return 0;
}
