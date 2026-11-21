#pragma once
#include "link.h"
namespace rift {
struct Envelope { uint8_t mac[6]; Packet data; };
namespace radio {
bool begin();
void end();
bool bind(const uint8_t *address);
bool fromPeer(const Envelope &in);
bool next(Envelope &in);
void send(const Packet &p, bool advertise=false);
}
}

