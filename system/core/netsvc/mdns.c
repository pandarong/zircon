#define _POSIX_C_SOURCE 200112L

#include "netsvc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mdns/mdns.h>


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

// mdns interface
void mdns_recv(void* data, size_t len, bool is_mcast,
               const ip6_addr_t* daddr, uint16_t dport,
               const ip6_addr_t* saddr, uint16_t sport) {
    printf("mdns: Got respnse\n");
    mdns_message message;
    memset(&message, 0, sizeof message);
    if (mdns_parse_message(data, len, &message) < 0) {
        printf("mdns_parse_message error\n");
        return;
    }
    dump_message(&message);
}
