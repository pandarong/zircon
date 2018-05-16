// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <kvstore/kvstore.h>
#include <zircon/device/block.h>
#include <zircon/hw/gpt.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEV_BLOCK "/dev/class/block"

#define BOOT_KEY "boot"

static const uint8_t sysconfig_guid[GPT_GUID_LEN] = GUID_SYS_CONFIG_VALUE;

static void usage(void) {
    fprintf(stderr,
            "Usage: sysconfig <command> [options]\n"
            "\n"
            "Command \"boot\": set boot partition\n"
            "  options are:\n"
            "    \"A\" or \"a\":    Zircon-A partition\n"
            "    \"B\" or \"b\":    Zircon-B partition\n"
            "    \"R\" or \"r\":    Zircon-R partition\n"
            "\n"
            );
}

// returns a file descriptor to the raw sysconfig partition
static int open_sysconfig(size_t* out_size) {
    struct dirent* de;
    DIR* dir = opendir(DEV_BLOCK);
    if (!dir) {
        printf("Error opening %s\n", DEV_BLOCK);
        return -1;
    }
    while ((de = readdir(dir)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
            continue;
        }
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", DEV_BLOCK, de->d_name);
        int fd = open(path, O_RDWR);
        if (fd < 0) {
            fprintf(stderr, "Error opening %s\n", path);
            continue;
        }

        uint8_t guid[GPT_GUID_LEN];
        if (ioctl_block_get_type_guid(fd, &guid, sizeof(guid)) < 0) {
            close(fd);
            continue;
        }
        if (memcmp(guid, sysconfig_guid, sizeof(sysconfig_guid))) {
            close(fd);
            continue;
        }

        block_info_t info;
        if (ioctl_block_get_info(fd, &info) < 0) {
            fprintf(stderr, "ioctl_block_get_info failed for %s\n", path);
            close(fd);
            continue;
        }
        *out_size = info.block_count * info.block_size;
        return fd;
    }
    closedir(dir);
    return -1;
}

int main(int argc, char **argv) {
    int ret;

    if (argc < 2) {
        usage();
        return -1;
    }

    const char* command = argv[1];
    if (strcmp(command, "boot")) {
        fprintf(stderr, "Unsupported command %s\n", command);
        usage();
        return -1;
    }

    const char* boot_part = (argc > 2 ? argv[2] : NULL);
    if (boot_part) {
        if (strcasecmp(boot_part, "a") && strcasecmp(boot_part, "b") && strcasecmp(boot_part, "r")) {
            fprintf(stderr, "Unsupported boot partition %s\n", boot_part);
            usage();    
            return -1;
        }
    }

    size_t partition_size = 0;
    int fd = open_sysconfig(&partition_size);
    if (fd < 0) {
        fprintf(stderr, "could not find sysconfig partition\n");
        return -1;
    }

    void* buffer = calloc(1, partition_size);
    if (!buffer) {
        fprintf(stderr, "could not allocate buffer for sysconfig partition\n");
        goto fail;
    }
    if ((ret = read(fd, buffer, partition_size)) != (int)partition_size) {
        fprintf(stderr, "could not read sysconfig partition: %d\n", ret);
        goto fail;
    }

    struct kvstore kvs;

    if (boot_part) {
        kvs_init(&kvs, buffer, partition_size);

        ret = kvs_add(&kvs, BOOT_KEY, boot_part);
        if (ret < 0) {
            fprintf(stderr, "kvs_add failed: %d\n", ret);
            goto fail;
        }

        kvs_save(&kvs);
        lseek(fd, 0, SEEK_SET);     
        if ((ret = write(fd, buffer, partition_size)) != (int)partition_size) {
            fprintf(stderr, "could not write sysconfig partition: %d\n", ret);
            goto fail;
        }
    } else {
        ret = kvs_load(&kvs, buffer, partition_size);
        if (ret == KVS_ERR_PARSE_HDR) {
            printf("initializing empty or corrupt sysconfig partition\n");
        } else if (ret < 0) {
            fprintf(stderr, "kvs_load failed: %d\n", ret);
            goto fail;
        }

        const char* current = kvs_get(&kvs, BOOT_KEY, "");
        if (current && current[0]) {
            printf("current boot partition: %s\n", current);
        } else {
            printf("boot partition not set\n");
        }
    }

    free(buffer);
    close(fd);
    return 0;
fail:
    free(buffer);
    close(fd);
    return -1;   
}
