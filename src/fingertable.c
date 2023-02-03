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
 *
 * @param fngrtable The fingertable to be initialised or updated
 * @param fngrtable_len  The length of said fingertable
 * @param s The peer to receive info about responsible peers from, usually self
 * @return true if the fingertable was initialised sucessfully, otherwise false
 */
bool ft_update_entries(ftentry* fngrtable, int fngrtable_len, peer* s){
    for(int i = 0; i < fngrtable_len; i++){
            ftentry new_entry;

            // calculate start value
            new_entry.start = (int)(s->node_id + pow(2, i)) % (int) pow(2, fngrtable_len);

            // find successor for given start value
            lookup_peer(new_entry.start); // look up responsible peer for hash ID 'new_entry.start'

            size_t rsp_len = 0;
            unsigned char* rsp = recvall(s->socket, &rsp_len); // receive responsible peer info
            packet* new_succ_pack = packet_decode(rsp, rsp_len);
            if(!new_succ_pack){
                fprintf(stderr, "Fingertable init failed. Did not receive responsible peer info.");
                return false;
            }

            new_entry.successor = peer_from_packet(new_succ_pack); // once there is a result save it as the successor

            fngrtable[i] = new_entry; // all done, add entry to fingertable!
    }
    return true;
}
