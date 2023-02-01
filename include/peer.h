#pragma once
#include "neighbour.h"
#include "server.h"

int forward(peer *p, packet *pack);

int proxy_request(server *srv, int csocket, packet *p, peer *n);

int lookup_peer(uint16_t hash_id);

int handle_own_request(server* srv, client *c, packet *p);

int answer_lookup(packet *p, peer *n);

void answer_fingertable_init(packet* pack, peer *p);

int handle_packet_data(server *srv, client *c, packet *p);

int handle_packet_ctrl(server *srv, client *c, packet *p);

int handle_packet(server *srv, client *c, packet *p);
