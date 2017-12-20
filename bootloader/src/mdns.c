// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inet6.h>
#include <mdns/mdns.h>
#include <netifc.h>


// Reads a big-endian halfword from buf.
uint16_t __halfword(char* buf) {
    return buf[1] | buf[0] << 8;
}

int mdns_parse_message(char* buffer, ssize_t buflen, mdns_message* query) {
    char* buf = buffer;

    int res = 0;
    if ((res = mdns_parse_header(buf, buflen, &(query->header)) < 0)) {
        return res;
    }

    buf += HEADER_BYTE_COUNT;
    
    if (query->header.qd_count > 0) {
        mdns_question** q = &query->questions;
        int i;
        for (i=0; i < query->header.qd_count; i++) {
            mdns_question qq;
            *q = &qq;
            buf += mdns_parse_question(buf, *q);
            q = &((*q)->next);
        }
        *q = NULL;
    }
    
    if (query->header.an_count > 0) {
        mdns_rr** a = &query->answers;
        int i;
        for(i = 0; i < query->header.an_count; i++) {
            mdns_rr rr;
            *a = &rr;
            buf += mdns_parse_rr(buf, *a);
            a = &((*a)->next);
        }
        *a = NULL;
    }

    if (query->header.ns_count > 0) {
        mdns_rr** a = &query->authority;
        int i;
        for(i = 0; i < query->header.ns_count; i++) {
            mdns_rr rr;
            *a = &rr;
            buf += mdns_parse_rr(buf, *a);
            a = &((*a)->next);
        }
        *a = NULL;
    }

    if (query->header.ar_count > 0) {
        mdns_rr** a = &query->additional;
        int i;
        for(i = 0; i < query->header.ar_count; i++) {
            mdns_rr rr;
            *a = &rr;
            buf += mdns_parse_rr(buf, *a);
            a = &((*a)->next);
        }
        *a = NULL;
    }
    return 0;
}

int mdns_parse_header(char* buf, ssize_t buflen, mdns_header* header) {
    if (buflen < HEADER_BYTE_COUNT) {
        return -1;
    }

    char* ptr = buf;

    header->id = __halfword(ptr); ptr += 2;
    header->flags = __halfword(ptr); ptr += 2;
    header->qd_count = __halfword(ptr); ptr += 2;
    header->an_count = __halfword(ptr); ptr += 2;
    header->ns_count = __halfword(ptr); ptr += 2;
    header->ar_count = __halfword(ptr); ptr += 2;

    return 0;
}

int mdns_parse_question(char* buffer, mdns_question* dest) {
    memset(dest, 0, sizeof(mdns_question));
    char *buf = buffer;
    int bytes = 0;
    if ((bytes = mdns_parse_domain(buf, &dest->domain)) < 0) {
        return bytes;
    }
    buf += bytes;
    dest->qtype = __halfword(buf); buf += 2;
    dest->qclass = __halfword(buf); buf += 2;
    // Return that we read bytes + 4 bytes;
    return bytes + 4;
}

// FIXME: Incomplete.
int mdns_parse_rr(char* buffer, mdns_rr* record) {
    memset(record, 0, sizeof(mdns_rr));  
    char *buf = buffer;
    int bytes = 0;
    if ((bytes = mdns_parse_domain(buf, &record->name)) < 0) {
        return bytes;
    }
    buf += bytes;
    record->type = __halfword(buf); buf += 2;
    record->class = __halfword(buf); buf += 2;
    record->ttl = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3]; buf += 4;
    return bytes + 8;
}

int mdns_parse_domain(char* dom, char** dest) {
    char buf[MAX_DOMAIN_LENGTH];
    int bufpos = 0;
    int i = 0;
    int size = 0;

    memset(buf, 0, MAX_DOMAIN_LENGTH);

    while (i < MAX_DOMAIN_LENGTH && dom[i] != 0) {
        size = (int)dom[i];
        i += 1;
        char field[size];
        memset(field, 0, size);
        memcpy(field, &dom[i], size);
        memcpy(buf + bufpos, field, size);
        bufpos += size + 1;
        buf[bufpos-1] = '.';
        i += (int)size;
    }
    if (i >= MAX_DOMAIN_LENGTH) {
        return -1; // Too long to be valid domain name.
    }

    buf[bufpos-1]= '\0'; // Replace last '.' with null terminator.  
    char destBuf[sizeof(char) * (bufpos - 1)];
    *dest = destBuf;
    memcpy(*dest, buf, bufpos-1);
    return i+1;
}

// Functions for creating an DNS message

// Writes a domain name to dest as a set of length-prefixed labels.
// Ignores compression.
int domain_to_labels(char* domain, uint8_t* dest) {
    uint8_t* destPtr = dest;
    uint8_t labelSize;
    
    // Keep from destroying original domain string.
    size_t domainLen = strlen(domain);
    char domainBuf[domainLen + 1];
    memcpy(domainBuf, domain, domainLen);
    domainBuf[domainLen] = '\0';

    const char dot[1] = ".";
    const char* label = strtok(domainBuf, dot);
    int i;
    while (label != NULL) {
        labelSize = strlen(label);
        *destPtr++ = labelSize;
        for (i=0; i < labelSize; i++) {
            *destPtr++ = *label++;
        }
        label = strtok(NULL, dot);
    }
    *destPtr = '\0';
    return 0;
}

// Writes a domain name to buf as a set of length-prefixed labels.
// Ignores compression. Returns the number of bytes written to buf.
// FIXME: Combine this with domain_to_labels above.
void pack_domain(uint8_t** buf, char* domain) {
    uint8_t labels[MAX_DOMAIN_LENGTH];
    domain_to_labels(domain, labels);
    size_t labelsLen = strlen((char*)labels);
    memcpy((void*)(*buf), labels, labelsLen);
    (*buf) += labelsLen + 1;
}


void pack8(uint8_t** buf, uint8_t byte) {
    **buf = 0xFF & byte;
    (*buf)++;
}

void pack(uint8_t** buf, int n, uint8_t* bytes) {
    int i;
    for (i=0; i < n; i++, bytes++) pack8(buf, *bytes);
}

void pack16(uint8_t** buf, uint16_t bytes) {
    pack8(buf, 0xFF & (bytes >> 8));
    pack8(buf, 0xFF & bytes);
}

void pack32(uint8_t** buf, uint32_t bytes) {
    pack16(buf, 0xFFFF & (bytes >> 16));
    pack16(buf, 0xFFFF & bytes);
}

uint8_t* pack_message(uint8_t* buf, mdns_header* header, 
    mdns_question* questions, mdns_rr* answers, mdns_rr* authorities, 
    mdns_rr* additionals) {
    uint8_t* bufptr = buf;

    // Header section
    uint16_t qd_count = 0;
    mdns_question* q = questions;
    for (; q != NULL; q = q->next) qd_count++;

    uint16_t an_count = 0;
    mdns_rr* rr = answers;
    for (; rr != NULL; rr = rr->next) an_count++;

    uint16_t ns_count = 0;
    for (rr = authorities; rr != NULL; rr = rr->next) ns_count++;

    uint16_t ar_count = 0;
    for (rr = additionals; rr != NULL; rr = rr->next) ar_count++;

    pack16(&bufptr, header->id);
    pack16(&bufptr, header->flags);
    pack16(&bufptr, qd_count);
    pack16(&bufptr, an_count);
    pack16(&bufptr, ns_count);
    pack16(&bufptr, ar_count);

    // Question section
    if (questions != NULL) {
        mdns_question* question = questions;
        while (question != NULL) {
            pack_domain(&bufptr, question->domain);
            pack16(&bufptr, question->qtype);
            pack16(&bufptr, question->qclass);
            question = question->next;
        }
    }

    // Answer section
    if (answers != NULL) {
        mdns_rr* answer = answers;
        while (answer != NULL) {
            pack_domain(&bufptr, answer->name);
            pack16(&bufptr, answer->type);
            pack16(&bufptr, answer->class);
            pack32(&bufptr, answer->ttl);
            pack16(&bufptr, answer->rdlength);
            pack(&bufptr, answer->rdlength, answer->rdata);
            answer = answer->next;
        }
    }

    return bufptr;
}

void dump_message(mdns_message* query) {
    printf("Query size: %lu\n", sizeof(*query));
    mdns_header nheader = query->header;
    printf("> Header:\n");
    printf("  id:              %d\n", nheader.id);
    printf("  flags:           0x%4X\n", nheader.flags);
    printf("  question count:  0x%4X\n", nheader.qd_count);
    printf("  answer count:    0x%4X\n", nheader.an_count);
    printf("  authority count: 0x%4X\n", nheader.ns_count);
    printf("  rr count:        0x%4X\n", nheader.ar_count);

    if (nheader.qd_count > 0) {
        printf("  > Questions:\n");
        mdns_question* q = query->questions;
        while (q != NULL) {
            printf("    Domain: %s\n", q->domain);
            printf("    Type:   0x%2X\n", q->qtype);
            printf("    Class:  0x%2X\n", q->qclass);
            q = q->next;
        }
    }

    if (nheader.an_count > 0) {
        printf("  > Answers:\n");
        mdns_rr* a = query->answers;
        while (a != NULL) {
            printf("    Name:  %s\n",  a->name);
            printf("    Type:  %4X\n", a->type);
            printf("    Class: %4X\n", a->class);
            printf("    TTL:   %8X\n", a->ttl);
            printf("    RDlen: %4X\n", a->rdlength);
            printf("    RDATA: %s\n", a->rdata);
            a = a->next;
        }
    }
}

static int mdns_fastcount = 0;
static int mdns_online = 0;
static int nb_active = 0;


// Sends an mDNS message (Class: QR, Type: Standard) to the ipv6 address
// ff02::fb, expecting a response from the Fuchsia bootserver.
static void mdns_advertise(void) {
    char hostname[19] = "fxbootserver.local";
    const int server_port = 5353;// 33331
    const int client_port = MDNS_QUERY_PORT;

    const ip6_addr bootserver_address = ip6_ll_all_nodes;
    // {
    //     .x = {0xFF, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFB},
    // };

    mdns_message q;
    memset(&q, 0, sizeof(q));

    // Header
    memset(&q.header, 0, sizeof(q.header));
    q.header.id = 07734; // "Hello" in digits. For debugging.
    q.header.flags = (uint16_t)0x8000; // QR, Standard query
    
    // Question
    mdns_question question;
    memset(&question, 0, sizeof(question));
    question.qtype = RR_A;
    question.qclass = QCLASS_IN;
    question.domain = hostname;
    q.questions = &question;

    // Pack message
    uint8_t msg[512];
    memset(&msg, 0, 512);
    uint8_t* end = pack_message(msg, &q.header, q.questions, NULL, NULL, NULL);

    // Send it!
    udp6_send(msg, (char*)end - (char*)msg, 
            &bootserver_address, server_port, client_port);
}


int mdns_poll() {
    if (netifc_active()) {
        if (mdns_online==0) {
            printf("mdns: interface online\n");
            mdns_online = 1;
            netifc_set_timer(100);
            mdns_advertise();
        }
    } else {
        if (mdns_online == 1) {
            printf("mdns: interface offline\n");
            mdns_online = 0;
        }
        return 0;
    }
    if (netifc_timer_expired()) {
        if (mdns_fastcount) {
            mdns_fastcount--;
            netifc_set_timer(100);
        } else {
            netifc_set_timer(1000);
        }
        if (nb_active) {
            nb_active = 0;
        } else {
            mdns_advertise();
        }
    }

    netifc_poll();
    
    
    return 0;
}
