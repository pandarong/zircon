#include <mdns/mdns.h>
#include <inet6.h>

// Sends an mDNS message (Class: QR, Type: Standard) to the ipv6 address
// ff02::fb, expecting a response from the Fuchsia bootserver.
static void mdns_advertise(void) {
    // const char hostname[19] = "fxbootserver.local";
    // int port = MDNS_PORT;
    // const ip6_addr bootserver_address = {
    //     .x = {0xFF, 0x02, 0xfb},
    // };

    // mdns_message q;
    // memset(&q, 0, sizeof(q));

    // // Header
    // memset(&q.header, 0, sizeof(header));
    // q.header = header;
    // q.header.id = 07734;
    // q.header.flags = (uint16_t)0x8000; // QR, Standard query
    
    // // Question
    // mdns_question question;
    // memset(&question, 0, sizeof(question));
    // question.qtype = RR_A;
    // question.qclass = QCLASS_IN;
    // question.domain = &hostname;
    // q.questions = &question;

    // // Pack message
    // uint8_t msg[512];
    // memset(&msg, 0, 512);
    // uint8_t* end = pack_message(msg, &q.header, NULL, q.answers, NULL, NULL);

    // // Send it!
    // udp6_send(msg, (char*)end - (char*)msg, &bootserver_address, 5350, 5353);
}

int mdns_poll() {
    mdns_advertise();
    return 0;
}

int mdns_close() {
    return 0;
}