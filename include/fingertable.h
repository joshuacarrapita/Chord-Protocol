#include "neighbour.h"

#define ft_len 16

typedef struct _ftentry{
    int start;
    peer* successor;
} ftentry;

int ft_forward_closest_pred(ftentry* fngrtable, int fngrtable_len, packet* pack);

void ft_clear_fingertable(ftentry* fngrtable, int fngrtable_len);

void ftentry_free(ftentry entry);

bool ft_update_entries(ftentry* fngrtable, int fngrtable_len, peer* s);

