#include "hash_table.h"

void htable_set(htable **ht, const unsigned char *key, size_t key_len,
                const unsigned char *value, size_t value_len) {
    /* TODO IMPLEMENT */

    // tmp struct
    struct htable *s = (struct htable *) malloc(sizeof *s);
    // write key and key_len
    memcpy(s->key, key, key_len);
    s->key_len = key_len;
    // write value and value_len
    memcpy(s->value, value, value_len);
    s->value_len = value_len;
    // add s to hash table
    HASH_ADD_KEYPTR(hh, *ht, s->key, key_len, s);
}

htable *htable_get(htable **ht, const unsigned char *key, size_t key_len) {
    /* TODO IMPLEMENT */

    struct htable *s;
    HASH_FIND(hh, *ht, key, key_len, s);
    return s;
}

int htable_delete(htable **ht, const unsigned char *key, size_t key_len) {
    /* TODO IMPLEMENT */

    struct htable *s;
    HASH_FIND(hh, *ht, key, key_len, s);
    // TODO: Free?
    HASH_DELETE(hh, *ht, s);
    return 0;
}
