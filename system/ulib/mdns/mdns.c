#include <arpa/inet.h>
#include <mdns/mdns.h>

// This assumes that mdns_header's layout agrees with the DNS header spec
static uint8_t* mdns_build_header(const mdns_header* header, uint8_t* dst,
                                  uint8_t* end) {
    if ((size_t)(end - dst) < sizeof(mdns_header)) return NULL;
    mdns_header* oheader = (mdns_header*)(void*)dst;
    oheader->id = htons(header->id);
    oheader->flags = htons(header->flags);
    oheader->qcount = htons(header->qcount);
    oheader->acount = htons(header->acount);
    oheader->nscount = htons(header->nscount);
    oheader->arcount = htons(header->arcount);
    return dst + sizeof(mdns_header);
}

static uint8_t* mdns_build_domain(const char* domain, uint8_t* dst,
                                  uint8_t* end) {
    if (end - dst < 1) return NULL;
    uint8_t* size_ptr = dst++;
    while(*domain) {
        while(*domain && *domain != '.') {
          if (dst >= end) return NULL;
          *dst++ = *domain++;
        }
        // Set the size of this part and update location of size_ptr
        *size_ptr = dst - size_ptr;
        size_ptr = dst;
    }
    // size_ptr should be dst and dst just point one past the end.
    // we need to add a length zero domain terminator now. dst
    // could have been exuasted so make sure that we can place the
    // last byte there.
    if (size_ptr >= end) return NULL;
    *size_ptr = 0;
    return size_ptr + 1;
}

uint8_t* mdns_build_query(const char* domain, uint8_t* dst, uint8_t* end) {
  mdns_header header = {.qcount = 1};
  dst = mdns_build_header(&header, dst, end);
  if (!dst || dst >= end) return NULL;
  dst = mdns_build_domain(domain, dst, end);
  if (!dst || dst >= end) return NULL;
  if (end - dst < 4) return NULL;
  *(uint16_t*)(void*)dst = htons(MDNS_RTYPE_AAAA);
  dst += 2;
  *(uint16_t*)(void*)dst = htons(1 << 15 | 1); // Unicast=true QCLASS=1
  dst += 2;
  return dst;
}

uint8_t* mdns_build_response(const char* domain, uint8_t ip[16], uint32_t ttl,
                             uint8_t* dst, uint8_t* end) {
  mdns_header header = {.qcount = 1};
  dst = mdns_build_header(&header, dst, end);
  if (!dst || dst >= end) return NULL;
  dst = mdns_build_domain(domain, dst, end);
  if (!dst || dst >= end) return NULL;
  if (end - dst < 10 + 16) return NULL; // Fields plus 16 for the IP.
  *(uint16_t*)(void*)dst = htons(MDNS_RTYPE_AAAA);
  dst += 2;
  *(uint16_t*)(void*)dst = htons(1); // FLUSH = false, RRCLASS = 1
  dst += 2;
  *(uint32_t*)(void*)dst = htonl(ttl); // How long client should cache this.
  dst += 4;
  *(uint16_t*)(void*)dst = htons(16); // Size of IP address
  dst += 2;
  return dst;
}

static const uint8_t* mdns_parse_short(uint16_t* out, const uint8_t* src,
                                       const uint8_t* end) {
  if (end - src < 2) return NULL;
  *out = ntohs(*(uint16_t*)(void*)src);
  return src + 2;
}

static const uint8_t* mdns_parse_header(mdns_header* out, const uint8_t* src,
                                        const uint8_t* end) {
  if (end - src < 12) return NULL;
  src = mdns_parse_short(&out->id, src, end);
  src = mdns_parse_short(&out->flags, src, end);
  src = mdns_parse_short(&out->qcount, src, end);
  src = mdns_parse_short(&out->acount, src, end);
  src = mdns_parse_short(&out->nscount, src, end);
  src = mdns_parse_short(&out->arcount, src, end);
  return src;
}

static const uint8_t* mdns_parse_domain(char* dst, char* dend,
                                       const uint8_t* src, const uint8_t* end) {
  while (src < end && *src) {
    size_t len = *src++;
    const uint8_t* pend = src + len;
    while (src < pend) {
      if (dst == dend) return NULL;
      *dst++ = *src++;
    }
    // If we're not at the end of the domain add a '.'
    if (src < end && !*src) {
      if (dst == dend) return NULL;
      *dst++ = '.';
    }
  }
  // If we're at the end, don't write the null terminator to match what strncpy
  // does.
  if (dst < dend)
    *dst++ = 0;
  return src;
}

const uint8_t* mdns_parse_query(mdns_query* dst, const uint8_t* src,
                                const uint8_t* end) {
  src = mdns_parse_header(&dst->header, src, end);
  if (!src || src >= end | dst->header.qcount != 1) return NULL;
  src = mdns_parse_domain(dst->domain, dst->domain + MDNS_MAX_DOMAIN_LENGTH,
                          src, end);
  if (!src || end - src < 4) return NULL;
  uint16_t tmp;
  src = mdns_parse_short(&tmp, src, end);
  dst->rrclass = tmp; // TODO: Validate as supported?
  src = mdns_parse_short(&tmp, src, end);
  dst->unicast = tmp & 1; // TODO: read QCLASS too
  return src;
}

const uint8_t* mdns_parse_answer(mdns_answer* dst, const uint8_t* src,
                                 const uint8_t* end) {
  src = mdns_parse_header(&dst->header, src, end);
  if (!src || src >= end | dst->header.qcount != 1) return NULL;
  src = mdns_parse_domain(dst->domain, dst->domain + MDNS_MAX_DOMAIN_LENGTH,
                          src, end);
  if (!src || end - src < 11) return NULL;
  // I want to skip a bunch of stuff that is going to be ignored by us anyhow
  src += 8;
  
}
