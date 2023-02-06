//
#include "peer.h"
#include <stdbool.h>
#include <stdio.h>

#include "packet.h"
#include "math.h"
#include "fingertable.h"
#include "util.h"

/**
 * @brief Forward a packet to the closest predecessor to given hash_id in packet that is saved in the passed fingertable.
 *
 * @param peer The fingertable to search
 * @param pack The packet to forward
 * @return int The status of the sending procedure
 */
int ft_forward_closest_pred(ftentry* fngrtable, int fngrtable_len, packet* pack){
    for (int i = fngrtable_len-1; i >= 0; i--) {
         if (fngrtable[i].start < pack->hash_id) {
             return forward(fngrtable[i].successor, pack);
         }
    }
    return -1;
}

/**
 * @brief Free the memory allocated for the entries in fingertable, set all values to 0, set all pointers to NULL.
 *
 * @param fngrtable  The fingertable to be cleared
 * @param fngrtable_len The length of the given fingertable
 */
void ft_clear_fingertable(ftentry* fngrtable, int fngrtable_len){
    for (int i = 0; i < fngrtable_len; i++){
        ftentry_free(fngrtable[i]);
        fngrtable[i].start = 0;
    }
}

/**
 * @brief Free the memory allocated for an ftentry
 *
 * @param entry The fingertable entry that's to be freed
 */
void ftentry_free(ftentry entry){
    if(entry.successor != NULL) {
        peer_free(entry.successor);
        entry.successor = NULL;
    }
}

/**
 * @brief Initialises the fingertables start values and sends lookups for successors
 *
 * @param fngrtable The fingertable to be initialised or updated
 * @param fngrtable_len  The length of said fingertable
 * @param s Self
 * @return true if the fingertable was initialised sucessfully, otherwise false
 */
void ft_update_entries(ftentry* fngrtable, int fngrtable_len, peer* s){
    for(int i = 0; i < fngrtable_len; i++){
            // calculate start value
            fngrtable[i].start = (int)(s->node_id + pow(2, i)) % (int) pow(2, fngrtable_len);

            // find successor for given start value
            lookup_peer(fngrtable[i].start); // look up responsible peer for hash ID 'new_entry.start'
    }
}

/**
 * @brief Checks if there is a successor that is not yet set in the fingertable
 *
 * @param fngrtable The fingertable to check
 * @param fngrtable_len The lenght of said fingertable
 * @return true if all sucessors are set, else false
 */
bool ft_is_set(ftentry* fngrtable, int fngrtable_len){
    for(int i = 0; i < fngrtable_len; i++){
        if(!fngrtable[i].successor){
            return false;
        }
    }
    return true;
}

/**
 * @brief Sets the sucessor to a given value if this value exists in the fingertable
 *
 * @param fngrtable The fingertable in which the successor should be set
 * @param fngrtable_len The length of said fingertable
 * @param s The packet containing the data about the value and the successor
 */
void ft_set_successor(ftentry* fngrtable, int fngrtable_len, packet* s){
    for(int i = 0; i < fngrtable_len; i++){
        if(fngrtable[i].start == s->hash_id){
            fngrtable[i].successor = peer_from_packet(s);
            return;
        }
    }
}

/**
 * @brief Answer a fingertable initialisation request with an acknowledgement
 *
 * @param csocket The socket of the client to send acknowledgement to
 * @return int The callback status
 */
void answer_fingertable_init(int csocket){

    // build a new packet for the response
    packet *rsp = packet_new();
    rsp->flags = PKT_FLAG_FACK;

    // send response
    size_t data_len;
    unsigned char *raw = packet_serialize(rsp, &data_len);
    free(rsp);
    sendall(csocket, raw, data_len);
    free(raw);
    raw = NULL;
}
