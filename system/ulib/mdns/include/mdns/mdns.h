// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MDNS_MAX_DOMAIN_LENGTH 253

typedef enum {
  MDNS_RTYPE_AAAA = 28,
  MDNS_RTYPE_TXT = 16
} mdns_record_type;

typedef struct {
  uint16_t id;
  uint16_t flags;
  uint16_t qcount;
  uint16_t acount;
  uint16_t nscount;
  uint16_t arcount;
} mdns_header;

typedef struct {
  mdns_header header;
  char domain[MDNS_MAX_DOMAIN_LENGTH];
  uint8_t ip[16];
} mdns_answer;

typedef struct {
  mdns_header header;
  char domain[MDNS_MAX_DOMAIN_LENGTH];
  mdns_record_type rrclass;
  bool unicast;
} mdns_query;

void mdns_send(char* domain, const uint8_t ip[16], uint16_t port, char* iface);
void mdns_recv(void* data, size_t len, const uint8_t daddr[16], uint16_t dport,
               const uint8_t saddr[16], uint16_t sport);

#ifdef __cplusplus
}
#endif
