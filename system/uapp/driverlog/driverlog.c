// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <zircon/device/device.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void) {

}

int main(int argc, char **argv) {
    int ret = 0;

    if (argc < 2) {
        usage();
        return -1;
    }

    const char* path = argv[1];

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "could not open %s\n", path);
        return -1;
    }

    if (argc == 2) {
        uint32_t flags;
        ret = ioctl_device_get_log_flags(fd, &flags);
        if (ret < 0) {
            fprintf(stderr, "ioctl_device_get_log_flags failed for %s\n", path);
        } else {
            if (flags & ZX_LOG_ERROR) {
                printf("ERROR ");
            }
            if (flags & ZX_LOG_INFO) {
                printf("INFO ");
            }
            if (flags & ZX_LOG_TRACE) {
                printf("TRACE ");
            }
            if (flags & ZX_LOG_SPEW) {
                printf("SPEW");
            }
            printf("\n");
        }
        goto out; 
    }

    uint32_t flags = 0;
    uint32_t mask = 0;

    for (int i = 2; i < argc; i++) {        
        char* arg = argv[i];
        char action = arg[0];
        uint32_t flag = 0;

        // check for leading + or -
        if (action == '+' || action == '-') {
            arg++;
        }
        if (!strcasecmp(arg, "e") || !strcasecmp(arg, "error")) {
            flag = ZX_LOG_ERROR;
        } else if (!strcasecmp(arg, "i") || !strcasecmp(arg, "info")) {
            flag = ZX_LOG_INFO;
        } else if (!strcasecmp(arg, "t") || !strcasecmp(arg, "trace")) {
            flag = ZX_LOG_TRACE;
        } else if (!strcasecmp(arg, "s") || !strcasecmp(arg, "spew")) {
            flag = ZX_LOG_SPEW;
        } else {
            fprintf(stderr, "unknown flag %s\n", arg);
            ret = -1;
            goto out;
        }

        if (action == '-') {
            mask |= flag;
        } else if {
            flags |= flag;
            mask |= flag;
        } 
    }


kkkk

out:
    close(fd);
    return ret;
}
