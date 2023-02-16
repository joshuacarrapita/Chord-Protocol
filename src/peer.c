#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "hash_table.h"
#include "neighbour.h"
#include "packet.h"
#include "requests.h"
#include "server.h"
#include "util.h"
#include "fingertable.h"

// actual underlying hash table
htable **ht = NULL;
rtable **rt = NULL;

// chord peers
peer *self = NULL;
peer *pred = NULL;
peer *succ = NULL;

char *hostPeer = NULL;
char *portPeer = NULL;

// fingertable
ftentry ftable [ft_len];
bool ftset = false;
bool ftinit = false;

#define debug 0

/**
 * @brief Forward a packet to a peer.
 *
 * @param peer The peer to forward the request to
 * @param pack The packet to forward
 * @return int The status of the sending procedure
 */
int forward(peer *p, packet *pack) {
    if (debug) {
        fprintf(stderr, "Forwarding packet: (%d; %d; %d) to (%d; %s; %d; %d).",
                pack->node_id, pack->node_ip, pack->node_port,
                p->node_id, succ->hostname, succ->port, succ->socket);
    }
    // check whether we can connect to the peer
    if (peer_connect(p) != 0) {
        fprintf(stderr, "Failed to connect to peer %s:%d\n", p->hostname,
                p->port);
        return -1;
    }

    size_t data_len;
    unsigned char *raw = packet_serialize(pack, &data_len);
    int status = sendall(p->socket, raw, data_len);
    free(raw);
    raw = NULL;

    peer_disconnect(p);
    return status;
}

/**
 * @brief Forward a request to the successor.
 *
 * @param srv The server
 * @param csocket The scokent of the client
 * @param p The packet to forward
 * @param n The peer to forward to
 * @return int The callback status
 */
int proxy_request(server *srv, int csocket, packet *p, peer *n) {
    // check whether we can connect to the peer
    if (peer_connect(n) != 0) {
        fprintf(stderr,
                "Could not connect to peer %s:%d to proxy request for client!",
                n->hostname, n->port);
        return CB_REMOVE_CLIENT;
    }

    size_t data_len;
    unsigned char *raw = packet_serialize(p, &data_len);
    sendall(n->socket, raw, data_len);
    free(raw);
    raw = NULL;

    size_t rsp_len = 0;
    unsigned char *rsp = recvall(n->socket, &rsp_len);

    // Just pipe everything through unfiltered. Yolo!
    sendall(csocket, rsp, rsp_len);
    free(rsp);

    peer_disconnect(n);
    return CB_REMOVE_CLIENT;
}

/**
 * @brief Lookup the peer responsible for a hash_id.
 *
 * @param hash_id The hash to lookup
 * @return int The callback status
 */
int lookup_peer(uint16_t hash_id) {
    // We could see whether or not we need to repeat the lookup

    // build a new packet for the lookup
    packet *lkp = packet_new();
    lkp->flags = PKT_FLAG_CTRL | PKT_FLAG_LKUP;
    lkp->hash_id = hash_id;
    lkp->node_id = self->node_id;
    lkp->node_port = self->port;

    lkp->node_ip = peer_get_ip(self);

    forward(succ, lkp);
    return 0;
}

/**
 * @brief Handle a client request we are resonspible for.
 *
 * @param c The client
 * @param p The packet
 * @return int The callback status
 */
int handle_own_request(server* srv, client *c, packet *p) {
    // build a new packet for the request
    packet *rsp = packet_new();

    if (p->flags & PKT_FLAG_GET) {
        // this is a GET request
        htable *entry = htable_get(ht, p->key, p->key_len);
        if (entry != NULL) {
            rsp->flags = PKT_FLAG_GET | PKT_FLAG_ACK;

            rsp->key = (unsigned char *)malloc(entry->key_len);
            rsp->key_len = entry->key_len;
            memcpy(rsp->key, entry->key, entry->key_len);

            rsp->value = (unsigned char *)malloc(entry->value_len);
            rsp->value_len = entry->value_len;
            memcpy(rsp->value, entry->value, entry->value_len);
        } else {
            rsp->flags = PKT_FLAG_GET;
            rsp->key = (unsigned char *)malloc(p->key_len);
            rsp->key_len = p->key_len;
            memcpy(rsp->key, p->key, p->key_len);
        }
    } else if (p->flags & PKT_FLAG_SET) {
        // this is a SET request
        rsp->flags = PKT_FLAG_SET | PKT_FLAG_ACK;
        htable_set(ht, p->key, p->key_len, p->value, p->value_len);
    } else if (p->flags & PKT_FLAG_DEL) {
        // this is a DELETE request
        int status = htable_delete(ht, p->key, p->key_len);

        if (status == 0) {
            rsp->flags = PKT_FLAG_DEL | PKT_FLAG_ACK;
        } else {
            rsp->flags = PKT_FLAG_DEL;
        }
    } else {
        // send some default data
        rsp->flags = p->flags | PKT_FLAG_ACK;
        rsp->key = (unsigned char *)strdup("Rick Astley");
        rsp->key_len = strlen((char *)rsp->key);
        rsp->value = (unsigned char *)strdup("Never Gonna Give You Up!\n");
        rsp->value_len = strlen((char *)rsp->value);
    }

    size_t data_len;
    unsigned char *raw = packet_serialize(rsp, &data_len);
    packet_free(rsp);
    sendall(c->socket, raw, data_len);
    free(raw);
    raw = NULL;

    return CB_REMOVE_CLIENT;
}

/**
 * @brief Answer a lookup request from a peer.
 *
 * @param p The packet
 * @param n The peer
 * @return int The callback status
 */
int answer_lookup(packet *p, peer *n) {
    peer *questioner = peer_from_packet(p);

    // check whether we can connect to the peer
    if (peer_connect(questioner) != 0) {
        fprintf(stderr, "Could not connect to questioner of lookup at %s:%d\n!",
                questioner->hostname, questioner->port);
        peer_free(questioner);
        return CB_REMOVE_CLIENT;
    }

    // build a new packet for the response
    packet *rsp = packet_new();
    rsp->flags = PKT_FLAG_CTRL | PKT_FLAG_RPLY;
    rsp->hash_id = p->hash_id;
    rsp->node_id = n->node_id;
    rsp->node_port = n->port;
    rsp->node_ip = peer_get_ip(n);

    size_t data_len;
    unsigned char *raw = packet_serialize(rsp, &data_len);
    packet_free(rsp);
    sendall(questioner->socket, raw, data_len);
    free(raw);
    raw = NULL;
    peer_disconnect(questioner);
    peer_free(questioner);
    return CB_REMOVE_CLIENT;
}

/**
 * @brief Handle a key request request from a client.
 *
 * @param srv The server
 * @param c The client
 * @param p The packet
 * @return int The callback status
 */
int handle_packet_data(server *srv, client *c, packet *p) {
    // Hash the key of the <key, value> pair to use for the hash table
    uint16_t hash_id = pseudo_hash(p->key, p->key_len);
    fprintf(stderr, "Hash id: %d\n", hash_id);

    // Forward the packet to the correct peer
    if (peer_is_responsible(pred->node_id, self->node_id, hash_id)) {
        // We are responsible for this key
        fprintf(stderr, "We are responsible.\n");
        return handle_own_request(srv, c, p);
    } else if (peer_is_responsible(self->node_id, succ->node_id, hash_id)) {
        // Our successor is responsible for this key
        fprintf(stderr, "Successor's business.\n");
        return proxy_request(srv, c->socket, p, succ);
    } else {
        // We need to find the peer responsible for this key
        fprintf(stderr, "No idea! Just looking it up!.\n");
        add_request(rt, hash_id, c->socket, p);
        lookup_peer(hash_id);
        return CB_OK;
    }
}

/**
 * @brief Handle a control packet from another peer.
 * Lookup vs. Proxy Reply
 *
 * @param srv The server
 * @param c The client
 * @param p The packet
 * @return int The callback status
 */
int handle_packet_ctrl(server *srv, client *c, packet *p) {
    if (p->flags & PKT_FLAG_LKUP) {
        fprintf(stderr, "Handling control packet...\n");

        // we received a lookup request
        if (peer_is_responsible(pred->node_id, self->node_id, p->hash_id)) {
            // Our business
            fprintf(stderr, "Lol! This should not happen!\n");
            return answer_lookup(p, self);
        } else if (peer_is_responsible(self->node_id, succ->node_id,
                                       p->hash_id)) {
            return answer_lookup(p, succ);
        } else {
            // Great! Somebody else's job!
            if(ftset){
                // If there is a fingertable, let's do a more efficient lookup
                ft_forward_closest_pred(ftable, ft_len, p);
            } else {
                forward(succ, p);
            }
        }
    } else if (p->flags & PKT_FLAG_RPLY) {
        fprintf(stderr, "Handling control packet...\n");

        // Set successor on fingertable if necessary
        if(ftinit && !ftset){
            ft_set_successor(ftable, ft_len, p);
            ftset = ft_is_set(ftable, ft_len);
        }

        // Look for open requests and proxy them
        peer *n = peer_from_packet(p);
        for (request *r = get_requests(rt, p->hash_id); r != NULL;
             r = r->next) {
            proxy_request(srv, r->socket, r->packet, n);
            server_close_socket(srv, r->socket);
        }
        clear_requests(rt, p->hash_id);
    } else {
        /**
         * TODO:
         * Extend handled control messages.
         * For the first task, this means that join-, stabilize-, and notify-messages should be understood.
         * For the second task, finger- and f-ack-messages need to be used as well.
         **/

        // Send Join Msg to node in chord ring
        if (p->flags & PKT_FLAG_JOIN && p->flags & PKT_FLAG_NTFY) {
            if (hostPeer != NULL && portPeer != NULL) {
                peer *node = peer_init(
                        0, hostPeer,
                        portPeer);

                // check whether we can connect to the peer
                if (peer_connect(node) != 0) {
                    fprintf(stderr, "Could not connect to peer of JOIN&NTFY at %s:%d\n!",
                            node->hostname, node->port);
                    peer_free(node);
                    return CB_REMOVE_CLIENT;
                }

                // build a new packet including own id ip port
                packet *rsp = packet_new();
                rsp->flags = PKT_FLAG_CTRL | PKT_FLAG_JOIN;
                rsp->node_id = self->node_id;
                rsp->node_port = self->port;
                rsp->node_ip = peer_get_ip(self);

                size_t data_len;
                unsigned char *raw = packet_serialize(rsp, &data_len);
                packet_free(rsp);
                sendall(node->socket, raw, data_len);
                free(raw);
                raw = NULL;
                peer_disconnect(node);
                peer_free(node);
            }
        }
            // Send Stabilize Msg to successor
        else if (p->flags & PKT_FLAG_STAB && p->flags & PKT_FLAG_NTFY) {
            if (succ != NULL) {
                if (debug) fprintf(stderr, "Entering STAB&NTFY. \'succ\' is %d with %s:%d and sock %d. \'self\' is %d with %s:%d and sock %d.\n", succ->node_id, succ->hostname, succ->port, succ->socket, self->node_id, self->hostname, self->port, self->socket);
                // check whether we can connect to the peer
                if (peer_connect(succ) != 0) {
                    fprintf(stderr, "Could not connect to peer of STAB&NTFY at %s:%d\n!",
                            succ->hostname, succ->port);
                    return CB_REMOVE_CLIENT;
                }

                // build a new packet including own id ip port
                packet *rsp = packet_new();
                rsp->flags = PKT_FLAG_CTRL | PKT_FLAG_STAB;
                rsp->node_id = self->node_id;
                rsp->node_port = self->port;
                rsp->node_ip = peer_get_ip(self);

                size_t data_len;
                unsigned char *raw = packet_serialize(rsp, &data_len);
                packet_free(rsp);
                sendall(succ->socket, raw, data_len);
                free(raw);
                raw = NULL;

                client *new_client = (client *)malloc(sizeof(client));
                new_client->socket = succ->socket;
                new_client->addr = succ->addr;
                new_client->addr_len = succ->addr_len;

                new_client->state = IDLE;
                new_client->header_buf = rb_new(PKT_HEADER_LEN);
                new_client->pkt_buf = NULL;
                new_client->pack = NULL;

                // Append to front
                new_client->next = srv->clients;
                srv->clients = new_client;
                srv->n_clients++;
            }
        } else {
            fprintf(stderr, "Handling control packet...\n");

            // Normal incoming CTRL Packages
            if (p->flags & PKT_FLAG_JOIN) {
                if (pred == NULL || succ == NULL) {
                    peer *connection = peer_from_packet(p);
                    peer *new_node1 = peer_from_packet(p);
                    peer *new_node2 = peer_from_packet(p);

                    if (!succ) succ = new_node1;
                    if (!pred) pred = new_node2;

                    // check whether we can connect to the peer
                    if (peer_connect(connection) != 0) {
                        fprintf(stderr, "Could not connect to questioner of join at %s:%d\n!",
                                connection->hostname, connection->port);
                        peer_free(connection);
                        return CB_REMOVE_CLIENT;
                    }

                    // build a new packet for the response including own id ip port
                    packet *rsp = packet_new();
                    rsp->flags = PKT_FLAG_CTRL | PKT_FLAG_NTFY;
                    rsp->node_id = self->node_id;
                    rsp->node_port = self->port;
                    rsp->node_ip = peer_get_ip(self);

                    size_t data_len;
                    unsigned char *raw = packet_serialize(rsp, &data_len);
                    packet_free(rsp);
                    sendall(connection->socket, raw, data_len);
                    free(raw);
                    raw = NULL;
                    peer_disconnect(connection);
                    peer_free(connection);
                } else if (peer_is_responsible(pred->node_id, self->node_id, p->node_id)) {
                    // current peer is successor of new peer: updates its own predecessor
                    // and answers with a Notify and its own data
                    if(debug) {
                        fprintf(stderr, "I (%d; %s; %d; %d) am responsible for joining node (%d; %d; %d). Preparing JOIN...",
                                self->node_id, self->hostname, self->port, self->socket,
                                p->node_id, p->node_ip, p->node_port);
                    }

                    peer *connection = peer_from_packet(p);
                    peer *new_node1 = peer_from_packet(p);

                    if(debug) {
                        fprintf(stderr, "Attempting connection to (%d; %s, %d; %d). Preparing JOIN...",
                                connection->node_id, connection->hostname, connection->port, connection->socket);
                    }
                    // check whether we can connect to the peer
                    if (peer_connect(connection) != 0) {
                        fprintf(stderr, "Could not connect to questioner of lookup at %s:%d\n!",
                                connection->hostname, connection->port);
                        peer_free(connection);
                        return CB_REMOVE_CLIENT;
                    }

                    // add new_node as own predecessor
                    if (pred != NULL) {
                        peer_free(pred);
                    }
                    pred = new_node1;

                    // build a new packet for the response including own id ip port
                    packet *rsp = packet_new();
                    rsp->flags = PKT_FLAG_CTRL | PKT_FLAG_NTFY;
                    rsp->node_id = self->node_id;
                    rsp->node_port = self->port;
                    rsp->node_ip = peer_get_ip(self);

                    size_t data_len;
                    unsigned char *raw = packet_serialize(rsp, &data_len);
                    packet_free(rsp);
                    sendall(connection->socket, raw, data_len);
                    free(raw);
                    raw = NULL;
                    peer_disconnect(connection);
                    peer_free(connection);
                } else if (peer_is_responsible(self->node_id, succ->node_id, p->node_id)) {
                    // successor is responsible -> forward
                    if(debug) {
                        fprintf(stderr, "I (%d; %s; %d; %d) am not responsible for joining node, "
                                              "(%d; %d; %d) forwarding to successor (%d; %s; %d; %d).",
                                              self->node_id, self->hostname, self->port, self->socket,
                                              p->node_id, p->node_ip, p->node_port,
                                              succ->node_id, succ->hostname, succ->port, succ->socket);
                    }
                    forward(succ, p);
                } else {
                   if(ftset){
                       ft_forward_closest_pred(ftable,ft_len,p);
                   } else {
                       forward(succ,p);
                   }
                }
            } else if (p->flags & PKT_FLAG_NTFY) {
                // set successor to peer in packet
                peer *p_from_packet = peer_from_packet(p);
                if (p_from_packet->node_id != self->node_id || p_from_packet->port != self->port) { // TODO: Port Vergleich entfernen?
                    if (succ != NULL) { // this check has to be done because succ is NULL for joining node in the beginning
                        peer_free(succ);
                    }
                    succ = p_from_packet;
                }
            } else if (p->flags & PKT_FLAG_STAB) {
                if (pred == NULL) {
                    peer *p_from_packet = peer_from_packet(p);
                    if (p_from_packet->node_id != self->node_id || p_from_packet->port != self->port) {
                        pred = p_from_packet;
                    }
                }
                if (pred != NULL) {
                    // current peer is not successor of origin anymore and sends new successor back

                    /*peer *node = peer_from_packet(p);

                    // check whether we can connect to the peer
                    if (peer_connect(node) != 0) {
                        fprintf(stderr, "Could not connect to questioner of lookup at %s:%d\n!",
                                node->hostname, node->port);
                        peer_free(node);
                        return CB_REMOVE_CLIENT;
                    }*/

                    // build a new packet for the response including id ip port of predecessor
                    packet *rsp = packet_new();
                    rsp->flags = PKT_FLAG_CTRL | PKT_FLAG_NTFY;
                    rsp->node_id = pred->node_id;
                    rsp->node_port = pred->port;
                    rsp->node_ip = peer_get_ip(pred);

                    size_t data_len;
                    unsigned char *raw = packet_serialize(rsp, &data_len);
                    packet_free(rsp);
                    sendall(c->socket, raw, data_len);
                    free(raw);
                    raw = NULL;
                    //peer_disconnect(node);
                    //peer_free(node);
                }
                // else do nothing. everything is ok
            } else if (p->flags & PKT_FLAG_FNGR) { // initialize finger table
                ftinit = false;
                ftset = false;
                if(self->node_id){
                    // Clear old fingertable and free memory
                    ft_clear_fingertable(ftable, ft_len);

                    // Update entries
                    ft_update_entries(ftable, ft_len, pred, self, succ);

                    // Send acknowledgment
                    answer_fingertable_init(c->socket);

                    ftinit = true;
                }
            }
        }
    }
    return CB_REMOVE_CLIENT;
}

/**
 * @brief Handle a received packet.
 * This can be a key request received from a client or a control packet from
 * another peer.
 *
 * @param srv The server instance
 * @param c The client instance
 * @param p The packet instance
 * @return int The callback status
 */
int handle_packet(server *srv, client *c, packet *p) {
    if (p->flags & PKT_FLAG_CTRL) {
        return handle_packet_ctrl(srv, c, p);
    } else {
        return handle_packet_data(srv, c, p);
    }
}

/**
 * @brief Main entry for a peer of the chord ring.
 *
 * TODO:
 * Modify usage of peer. Accept:
 * 1. Own IP and port;
 * 2. Own ID (optional, zero if not passed);
 * 3. IP and port of Node in existing DHT. This is optional: If not passed, establish new DHT, otherwise join existing.
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @return int The exit code
 */
int main(int argc, char **argv) {

    if (argc < 3) {
        fprintf(stderr, "Not enough args! I need IP PORT [ID] [Peer-IP Peer-Port]\n");
    }

    // Read arguments for self
    char *hostSelf = argv[1];
    char *portSelf = argv[2];

    // check if less than 3 arguments
    uint16_t idSelf;
    if(argc > 3) {
        // check if arg3 is Number -> id is set
        bool idIsSet = true;
        for (int i = 0; argv[3][i] != 0; i++) {
            if (!isdigit(argv[3][i]))
                idIsSet = false;
        }

        int peerArgNum;
        if (idIsSet) {
            idSelf = strtoul(argv[3], NULL, 10);
            peerArgNum = 4;
        } else {
            idSelf = 0;
            peerArgNum = 3;
        }

        // Read arguments for existing peer (optional parameter)
        if ((peerArgNum == 4 && argc >= 6) || (peerArgNum == 3 && argc >= 5)) {
            hostPeer = argv[peerArgNum];
            portPeer = argv[peerArgNum + 1];
        }
    }
    else {
        idSelf = 0;
    }

    // Initialize all chord peers
    self = peer_init(
        idSelf, hostSelf,
        portSelf); //  Not really necessary but convenient to store us as a peer

    // Initialize outer server for communication with clients
    server *srv = server_setup(portSelf);
    if (srv == NULL) {
        fprintf(stderr, "Server setup failed!\n");
        return -1;
    }
    // Initialize hash table
    ht = (htable **)malloc(sizeof(htable *));
    // Initiale reuqest table
    rt = (rtable **)malloc(sizeof(rtable *));
    *ht = NULL;
    *rt = NULL;

    srv->packet_cb = handle_packet;
    server_run(srv);
    close(srv->socket);
}
