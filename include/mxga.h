/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#ifndef MXGA_H
#define MXGA_H

#include <stddef.h>
#include <stdint.h>

#include "mx_versions.h"

#define MXGA_HEADER_BYTES 24u
#define MXGA_MAX_FRAME_BYTES (1024u * 1024u)
#define MXGA_STATS_HEADER_BYTES 72u
#define MXGA_STATS_CPU_BYTES 24u
#define MXGA_MAX_REPORTED_CPUS 1024u

#define MXGA_OP_HELLO 1u
#define MXGA_OP_HEARTBEAT 2u
#define MXGA_OP_CLIPBOARD_CHANGED 3u
#define MXGA_OP_COMMAND_RESULT 4u
#define MXGA_OP_SYSTEM_STATS 6u
#define MXGA_OP_INTEGRATION_STATUS 10u
#define MXGA_OP_SHUTDOWN 0x100u
#define MXGA_OP_RESTART 0x101u
#define MXGA_OP_CLIPBOARD_WRITE 0x102u
#define MXGA_OP_INTEGRATION_WINDOW_ACTION 0x106u

#define MXGA_CAP_SHUTDOWN (1ull << 0)
#define MXGA_CAP_RESTART (1ull << 1)
#define MXGA_CAP_CLIPBOARD_READ (1ull << 2)
#define MXGA_CAP_CLIPBOARD_WRITE (1ull << 3)
#define MXGA_CAP_SYSTEM_STATS (1ull << 5)
#define MXGA_CAP_INTEGRATION (1ull << 8)

#define MXGA_INTEGRATION_STATUS_VERSION 4u
#define MXGA_INTEGRATION_STATUS_HEADER_BYTES 16u
#define MXGA_INTEGRATION_WINDOW_BYTES 52u
#define MXGA_INTEGRATION_SEGMENT_BYTES 36u
#define MXGA_INTEGRATION_MAX_WINDOWS 128u
#define MXGA_INTEGRATION_MAX_SEGMENTS 32u
#define MXGA_INTEGRATION_MAX_TEXT_BYTES 4096u
#define MXGA_INTEGRATION_MAX_ICON_DIMENSION 256u
#define MXGA_INTEGRATION_MXGPU_PRESENT (1u << 0)
#define MXGA_INTEGRATION_MXGPU_DRIVER_READY (1u << 1)
#define MXGA_INTEGRATION_DESKTOP_BRIDGE_READY (1u << 2)
#define MXGA_INTEGRATION_WINDOW_INVENTORY_READY (1u << 3)
#define MXGA_INTEGRATION_ICON_RESOLUTION_READY (1u << 4)
#define MXGA_INTEGRATION_REQUIRED_FLAGS                                                            \
    (MXGA_INTEGRATION_MXGPU_PRESENT | MXGA_INTEGRATION_MXGPU_DRIVER_READY |                        \
     MXGA_INTEGRATION_DESKTOP_BRIDGE_READY | MXGA_INTEGRATION_WINDOW_INVENTORY_READY |             \
     MXGA_INTEGRATION_ICON_RESOLUTION_READY)
#define MXGA_INTEGRATION_WINDOW_GEOMETRY_RELIABLE (1u << 0)
#define MXGA_INTEGRATION_ACTION_VERSION 1u
#define MXGA_INTEGRATION_ACTION_BYTES 20u
#define MXGA_INTEGRATION_ACTION_ACTIVATE 1u
#define MXGA_INTEGRATION_ACTION_CLOSE 2u

#define MXGA_COMMAND_STATUS_SUCCESS 0u

#define MXGA_CPU_FREQ_NONE 0u
#define MXGA_CPU_FREQ_CPUFREQ 1u
#define MXGA_CPU_FREQ_CPUINFO 2u
#define MXGA_CPU_FREQ_FIRMWARE 3u
#define MXGA_CPU_FREQ_PERFORMANCE_COUNTER 4u
#define MXGA_CPU_FREQ_CALIBRATION_LOOP 5u

enum mxga_status {
    MXGA_OK = 0,
    MXGA_ERR_LENGTH = 1,
    MXGA_ERR_MAGIC = 2,
    MXGA_ERR_VERSION = 3,
    MXGA_ERR_FLAGS = 4,
    MXGA_ERR_PAYLOAD = 5,
    MXGA_ERR_CAPACITY = 6,
    MXGA_ERR_UTF8 = 7,
    MXGA_ERR_GEOMETRY = 8
};

struct mxga_frame {
    uint16_t minor;
    uint16_t opcode;
    uint64_t sequence;
    const uint8_t *payload;
    uint32_t payload_len;
};

#define MXGA_CLIPBOARD_METADATA_BYTES 16u
#define MXGA_CLIPBOARD_MAX_TEXT_BYTES (MXGA_MAX_FRAME_BYTES - MXGA_HEADER_BYTES - MXGA_CLIPBOARD_METADATA_BYTES)

struct mxga_clipboard {
    uint64_t origin;
    uint64_t generation;
    const uint8_t *text;
    uint32_t text_bytes;
};

/* Text and output may overlap during encoding. out_len, when supplied, must
 * be disjoint from both ranges. Invalid or overlapping metadata spans are
 * refused without modifying them. */
int mxga_encode_clipboard(uint64_t origin, uint64_t generation, const uint8_t *text,
                          uint32_t text_bytes, uint8_t *out, uint32_t cap, uint32_t *out_len);
/* The decoded text borrows the immutable payload for its complete lifetime.
 * The output record must be disjoint from the entire payload. */
int mxga_decode_clipboard(const uint8_t *payload, uint32_t len, struct mxga_clipboard *out);

struct mxga_cpu_stat {
    uint32_t usage_permille;
    uint32_t current_khz;
    uint32_t minimum_khz;
    uint32_t maximum_khz;
    uint32_t frequency_source;
};

struct mxga_system_stats {
    uint64_t uptime_seconds;
    uint64_t memory_total_bytes;
    uint64_t memory_available_bytes;
    uint64_t memory_used_bytes;
    uint64_t swap_total_bytes;
    uint64_t swap_used_bytes;
    uint32_t load_average_permille[3];
    uint32_t process_count;
    uint32_t cpu_usage_permille;
    uint32_t cpu_count;
    const struct mxga_cpu_stat *cpus;
};

struct mxga_integration_segment {
    uint16_t scanout_id;
    int32_t x, y;
    uint32_t width, height;
    uint32_t destination_x, destination_y;
    uint32_t destination_width, destination_height;
};

struct mxga_integrated_window {
    uint64_t window_id;
    int32_t x, y;
    uint32_t width, height, flags;
    const uint8_t *title, *application_id, *icon_bgra;
    uint16_t title_bytes, application_id_bytes, icon_width, icon_height;
    uint32_t icon_bgra_bytes;
    uint16_t scanout_id, segment_count;
    uint32_t output_width, output_height;
    const struct mxga_integration_segment *segments;
};

struct mxga_integration_status {
    uint64_t generation;
    uint16_t flags;
    uint32_t window_count;
    const struct mxga_integrated_window *windows;
};

struct mxga_integration_action {
    uint16_t action;
    uint64_t generation, window_id;
};

int mxga_encode_frame(uint16_t minor, uint16_t opcode, uint64_t sequence, const uint8_t *payload,
                      uint32_t payload_len, uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxga_decode_frame(const uint8_t *in, uint32_t len, struct mxga_frame *out);
int mxga_encode_capabilities(uint64_t capabilities, uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxga_decode_capabilities(const uint8_t *payload, uint32_t len, uint64_t *capabilities);
int mxga_encode_command_result(uint16_t command_opcode, uint16_t status, uint8_t *out, uint32_t cap,
                               uint32_t *out_len);
int mxga_encode_system_stats(const struct mxga_system_stats *stats, uint8_t *out, uint32_t cap,
                             uint32_t *out_len);
int mxga_decode_system_stats(const uint8_t *payload, uint32_t len, struct mxga_system_stats *stats,
                             struct mxga_cpu_stat *cpus, uint32_t cpu_cap);
int mxga_encode_integration_status(const struct mxga_integration_status *status, uint8_t *out,
                                   uint32_t cap, uint32_t *out_len);
/* Decoded text and icon views borrow payload; windows and segments are caller-owned. */
int mxga_decode_integration_status(const uint8_t *payload, uint32_t len,
                                   struct mxga_integration_status *status,
                                   struct mxga_integrated_window *windows, uint32_t window_cap,
                                   struct mxga_integration_segment *segments, uint32_t segment_cap);
int mxga_encode_integration_action(const struct mxga_integration_action *action, uint8_t *out,
                                   uint32_t cap, uint32_t *out_len);
int mxga_decode_integration_action(const uint8_t *payload, uint32_t len,
                                   struct mxga_integration_action *action);

#define MXGA_OP_NETWORK_INFO 7u
#define MXGA_OP_SHARE_STATUS 8u
#define MXGA_OP_FS_REQUEST 9u
#define MXGA_OP_MOUNT_SHARE 0x103u
#define MXGA_OP_UNMOUNT_SHARE 0x104u
#define MXGA_OP_FS_RESPONSE 0x105u

#define MXGA_CAP_NETWORK_INFO (1ull << 6)
#define MXGA_CAP_FOLDER_SHARING (1ull << 7)

#define MXGA_NETWORK_HEADER_BYTES 8u
#define MXGA_NETWORK_INTERFACE_BYTES 28u
#define MXGA_NETWORK_ADDRESS_BYTES 20u
#define MXGA_NETWORK_NAME_BYTES 16u
#define MXGA_NETWORK_MAC_BYTES 6u
#define MXGA_NETWORK_MAX_INTERFACES 64u
#define MXGA_NETWORK_MAX_ADDRESSES 32u
#define MXGA_NETWORK_FLAG_UP (1u << 0)
#define MXGA_NETWORK_FLAG_RUNNING (1u << 1)
#define MXGA_NETWORK_FLAG_LOOPBACK (1u << 2)
#define MXGA_NETWORK_FLAG_POINT_TO_POINT (1u << 3)
#define MXGA_NETWORK_FLAGS_KNOWN                                                                   \
    (MXGA_NETWORK_FLAG_UP | MXGA_NETWORK_FLAG_RUNNING | MXGA_NETWORK_FLAG_LOOPBACK |               \
     MXGA_NETWORK_FLAG_POINT_TO_POINT)
#define MXGA_NETWORK_FAMILY_IPV4 4u
#define MXGA_NETWORK_FAMILY_IPV6 6u

struct mxga_network_address {
    uint8_t family;
    uint8_t prefix_length;
    uint8_t address[16];
};

/* name is NUL-padded UTF-8 and may fill all 16 bytes. */
struct mxga_network_interface {
    uint8_t name[MXGA_NETWORK_NAME_BYTES];
    uint8_t mac[MXGA_NETWORK_MAC_BYTES];
    uint16_t flags;
    uint32_t address_count;
    const struct mxga_network_address *addresses;
};

int mxga_encode_network_info(const struct mxga_network_interface *interfaces, uint32_t count,
                             uint8_t *out, uint32_t cap, uint32_t *out_len);
/* Interface address views point into the caller-owned addresses array. */
int mxga_decode_network_info(const uint8_t *payload, uint32_t len,
                             struct mxga_network_interface *interfaces, uint32_t interface_cap,
                             struct mxga_network_address *addresses, uint32_t address_cap,
                             uint32_t *count);

#define MXGA_SHARE_NAME_BYTES 64u
#define MXGA_SHARE_PATH_BYTES 256u
#define MXGA_SHARE_MAX 32u
#define MXGA_SHARE_MOUNT_BYTES 328u
#define MXGA_SHARE_UNMOUNT_BYTES 8u
#define MXGA_SHARE_STATUS_HEADER_BYTES 8u
#define MXGA_SHARE_STATUS_RECORD_BYTES 336u
#define MXGA_SHARE_FLAG_READ_ONLY (1u << 0)
#define MXGA_SHARE_FLAG_AUTOMOUNT (1u << 1)
#define MXGA_SHARE_FLAGS_KNOWN (MXGA_SHARE_FLAG_READ_ONLY | MXGA_SHARE_FLAG_AUTOMOUNT)
#define MXGA_SHARE_STATE_UNMOUNTED 0u
#define MXGA_SHARE_STATE_MOUNTED 1u
#define MXGA_SHARE_STATE_FAILED 2u

/* Decoded text is copied and NUL-terminated; name_bytes is 1 to 64, mount_point_bytes 0 to 256. */
struct mxga_share_mount {
    uint32_t share_id;
    uint32_t flags;
    char name[MXGA_SHARE_NAME_BYTES + 1];
    uint32_t name_bytes;
    char mount_point[MXGA_SHARE_PATH_BYTES + 1];
    uint32_t mount_point_bytes;
};

/* error is a negated errno when state is FAILED and zero otherwise. */
struct mxga_share_status {
    uint32_t share_id;
    uint32_t state;
    int32_t error;
    uint32_t flags;
    const uint8_t *name;
    uint32_t name_bytes;
    const uint8_t *mount_point;
    uint32_t mount_point_bytes;
};

int mxga_encode_mount_share(const struct mxga_share_mount *mount, uint8_t *out, uint32_t cap,
                            uint32_t *out_len);
int mxga_decode_mount_share(const uint8_t *payload, uint32_t len, struct mxga_share_mount *mount);
int mxga_encode_unmount_share(uint32_t share_id, uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxga_decode_unmount_share(const uint8_t *payload, uint32_t len, uint32_t *share_id);
int mxga_encode_share_status(const struct mxga_share_status *shares, uint32_t count, uint8_t *out,
                             uint32_t cap, uint32_t *out_len);
/* Decoded text views borrow payload and exclude NUL padding. */
int mxga_decode_share_status(const uint8_t *payload, uint32_t len,
                             struct mxga_share_status *shares, uint32_t share_cap,
                             uint32_t *count);

#define MXGA_FS_REQUEST_HEADER_BYTES 56u
#define MXGA_FS_RESPONSE_HEADER_BYTES 32u
#define MXGA_FS_MAX_PATH_BYTES 4096u
#define MXGA_FS_MAX_NAME_BYTES 255u
#define MXGA_FS_MAX_IO_BYTES (512u * 1024u)
#define MXGA_FS_MAX_RESPONSE_DATA_BYTES                                                            \
    (MXGA_MAX_FRAME_BYTES - MXGA_HEADER_BYTES - MXGA_FS_RESPONSE_HEADER_BYTES)
#define MXGA_FS_ATTRIBUTES_BYTES 80u
#define MXGA_FS_STATFS_BYTES 56u
#define MXGA_FS_DIRENT_HEADER_BYTES 16u

#define MXGA_FS_OP_STATFS 1u
#define MXGA_FS_OP_GETATTR 2u
#define MXGA_FS_OP_READDIR 3u
#define MXGA_FS_OP_OPEN 4u
#define MXGA_FS_OP_READ 5u
#define MXGA_FS_OP_WRITE 6u
#define MXGA_FS_OP_CREATE 7u
#define MXGA_FS_OP_MKDIR 8u
#define MXGA_FS_OP_UNLINK 9u
#define MXGA_FS_OP_RMDIR 10u
#define MXGA_FS_OP_RENAME 11u
#define MXGA_FS_OP_TRUNCATE 12u
#define MXGA_FS_OP_RELEASE 13u
#define MXGA_FS_OP_FSYNC 14u
#define MXGA_FS_OP_SETATTR 15u
#define MXGA_FS_OP_READLINK 16u

#define MXGA_FS_OPEN_READ (1u << 0)
#define MXGA_FS_OPEN_WRITE (1u << 1)
#define MXGA_FS_OPEN_APPEND (1u << 2)
#define MXGA_FS_OPEN_TRUNCATE (1u << 3)
#define MXGA_FS_OPEN_CREATE (1u << 4)
#define MXGA_FS_OPEN_EXCLUSIVE (1u << 5)
#define MXGA_FS_OPEN_DIRECTORY (1u << 6)
#define MXGA_FS_OPEN_KNOWN 0x7fu

#define MXGA_FS_SETATTR_MODE (1u << 0)
#define MXGA_FS_SETATTR_UID (1u << 1)
#define MXGA_FS_SETATTR_GID (1u << 2)
#define MXGA_FS_SETATTR_SIZE (1u << 3)
#define MXGA_FS_SETATTR_ATIME (1u << 4)
#define MXGA_FS_SETATTR_MTIME (1u << 5)
#define MXGA_FS_SETATTR_KNOWN 0x3fu

struct mxga_fs_request {
    uint64_t request_id;
    uint64_t handle;
    uint64_t offset;
    uint32_t share_id;
    uint32_t operation;
    uint32_t length;
    uint32_t flags;
    uint32_t mode;
    const uint8_t *path;
    uint32_t path_bytes;
    const uint8_t *second_path;
    uint32_t second_path_bytes;
    const uint8_t *data;
    uint32_t data_bytes;
};

struct mxga_fs_response {
    uint64_t request_id;
    uint64_t handle;
    int32_t status;
    uint32_t operation;
    const uint8_t *data;
    uint32_t data_bytes;
};

struct mxga_fs_attributes {
    uint64_t ino, size, blocks;
    uint64_t atime_s, mtime_s, ctime_s;
    uint32_t atime_ns, mtime_ns, ctime_ns;
    uint32_t mode, uid, gid, nlink;
};

struct mxga_fs_statfs {
    uint64_t block_size, blocks, blocks_free, blocks_available, files, files_free, name_max;
};

struct mxga_fs_dirent {
    uint64_t inode;
    uint32_t mode;
    const uint8_t *name;
    uint32_t name_bytes;
};

/* A share-relative path: UTF-8, no NUL or backslash, no leading '/', no empty, "." or ".."
 * component, each component at most 255 bytes. The empty path names the share root. */
int mxga_fs_path_valid(const uint8_t *path, uint32_t bytes);
/* Refuses any request whose operation, flags, path presence or lengths break the contract. */
int mxga_encode_fs_request(const struct mxga_fs_request *request, uint8_t *out, uint32_t cap,
                           uint32_t *out_len);
/* Decoded paths and data borrow payload. */
int mxga_decode_fs_request(const uint8_t *payload, uint32_t len, struct mxga_fs_request *request);
int mxga_encode_fs_response(const struct mxga_fs_response *response, uint8_t *out, uint32_t cap,
                            uint32_t *out_len);
/* Decoded data borrows payload. status is zero or a negated errno. */
int mxga_decode_fs_response(const uint8_t *payload, uint32_t len,
                            struct mxga_fs_response *response);
int mxga_encode_fs_attributes(const struct mxga_fs_attributes *attributes, uint8_t *out,
                              uint32_t cap, uint32_t *out_len);
int mxga_decode_fs_attributes(const uint8_t *data, uint32_t len,
                              struct mxga_fs_attributes *attributes);
int mxga_encode_fs_statfs(const struct mxga_fs_statfs *statfs, uint8_t *out, uint32_t cap,
                          uint32_t *out_len);
int mxga_decode_fs_statfs(const uint8_t *data, uint32_t len, struct mxga_fs_statfs *statfs);
int mxga_encode_fs_dirent(const struct mxga_fs_dirent *entry, uint8_t *out, uint32_t cap,
                          uint32_t *out_len);
/* Decodes the entry at *cursor and advances it; the name borrows data. */
int mxga_decode_fs_dirent(const uint8_t *data, uint32_t len, uint32_t *cursor,
                          struct mxga_fs_dirent *entry);

#endif
