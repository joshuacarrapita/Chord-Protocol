#pragma once

#include <time.h>
#include <unistd.h>

#include "neighbour.h"

#define ft_len 16

typedef struct _ftentry{
    int start;
    peer* successor;
} ftentry;

int ft_forward_closest_pred(ftentry* fngrtable, int fngrtable_len, packet* pack);

void ft_clear_fingertable(ftentry* fngrtable, int fngrtable_len);

void ftentry_free(ftentry entry);

void ft_update_entries(ftentry* fngrtable, int fngrtable_len, peer* s);

bool ft_is_set(ftentry* fngrtable, int fngrtable_len);

void ft_set_successor(ftentry* fngrtable, int fngrtable_len, packet* s);

