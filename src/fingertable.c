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
