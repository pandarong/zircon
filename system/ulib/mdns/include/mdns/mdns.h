// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <stdint.h>

// The IPv4 address where mDNS multicast queries must be sent.
extern const char* MDNS_IPV4;

// The IPv6 address where mDNS multicast queries must be sent.
extern const char* MDNS_IPV6;

// DNS record types.
//
// This list contains only the subset of the DNS record types useful to the
// bootloader.  See http://edgedirector.com/app/type.htm for a full list of
// DNS record types.

// TXT RRs are used to hold descriptive text.  
//
// The semantics of the text depends on the domain where the record is found.
#define RR_TXT 0x0010

// A RRs specify ip addresses for a given host.
//
// These records are used for conversion of domain names to corresponding IP
// addresses.
#define RR_A 0x0001

// DNS query classes.
//
// This list contains only the subset of the DNS query types useful to the
// bootloader.  See http://edgedirector.com/app/type.htm for a full list of
// DNS query classes.

// The internet.
#define QCLASS_IN 0x0001


// The default port where mdns queries are sent.
#define MDNS_PORT 5353

// The default port from which mdns queries are sent.
#define MDNS_QUERY_PORT 5350

// The maxinum number of characters in a domain name.
#define MAX_DOMAIN_LENGTH 253
#define MAX_DOMAIN_LABEL 63

// The number of bytes in a DNS message header.
#define HEADER_BYTE_COUNT 12

// // We can send and receive packets up to 9000 bytes.
// #define MAX_DNS_MESSAGE_DATA 8940

// A DNS message header.
//
// id is a unique identifier used to match queries with responses.
//  
// flags is a set of flags represented as a collection of sub-fields.
// The format of the flags section is as follows:
//
// Bit no. | Meaning
// -------------------
// 1        0 = query
//          1 = reply
//
// 2-5      0000 = standard query
//          0100 = inverse
//          0010 & 0001 not used.
//
// 6        0 = non-authoritative DNS answer
//          1 = authoritative DNS answer
//
// 7        0 = message not truncated
//          1 = message truncated
//
// 8        0 = non-recursive query
//          1 = recursive query
//
// 9        0 = recursion not available
//          1 = recursion available
//
// 10 & 12  reserved
//
// 11       0 = answer/authority portion was not authenticated by the
//              server
//          1 = answer/authority portion was authenticated by the
//              server
//
// 13 - 16  0000 = no error
//          0100 = Format error in query
//          0010 = Server failure
//          0001 = Name does not exist
typedef struct mdns_header_t {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} mdns_header;

// An mDNS question.
// FIXME: Change to type and class.
typedef struct mdns_question_t {
    char* domain;
    uint16_t qtype;
    uint16_t qclass;
    struct mdns_question_t* next;
} mdns_question;

// An mDNS resource record
typedef struct mdns_rr_t {
    char* name;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t rdlength;
    uint8_t* rdata;
    struct mdns_rr_t* next;
} mdns_rr;

// An mDNS query packet
typedef struct mdns_message_t {
    mdns_header header;
    mdns_question* questions;
    mdns_rr* answers;
    mdns_rr* authority;
    mdns_rr* additional;
} mdns_message;


// Creates a socket from the given address family, address and port.
//
// Returns zero on success.  Otherwise, returns -1.
//
// Example: Create socket to recieve packets at the IPv6 address ff02::fb
//   `create_socket(AF_INET6, mdns::IPV6, mdns::PORT);`
int mdns_socket(int ai_family, const char* addr, int port);

// Parses a mDNS query from buf into the give query struct.
//
// Returns a value < 0 if an error occurred.
// FIXME: Rename to parse_message.
int mdns_parse_message(char* buf, ssize_t buflen, mdns_message* query);

// Packs the given sections of a query message into a uint8_t buffer.
// FIXME: Rename to mdns_pack_message.
uint8_t* pack_message(uint8_t* buf, mdns_header* header, 
    mdns_question* questions, mdns_rr* answers, mdns_rr* authorities, 
    mdns_rr* additionals);

/* --- Fuchsia bootloader specific headers --- */

// Sends a client request from the bootloader to find the bootserver.
int mdns_poll(void);

// FIXME: HIDE ALL PARSING METHODS BELOW HERE.

// Parses an mDNS message question.
int mdns_parse_question(char* buf, mdns_question* dest);

// Parses a mDNS message header from buf into the given header struct.
//
// Returns a value < 0 if an error occurred.
int mdns_parse_header(char* buf, ssize_t buflen, mdns_header* header);

// Parses a domain name from buf into the given buffer.
//
// Returns a value < 0 if an error occurred.
int mdns_parse_domain(char* buf, char** dest);

// Parses a resource record.
int mdns_parse_rr(char *buf, mdns_rr *rr);


// Prints the query to stdout. For debugging only.
void dump_message(mdns_message* query);
