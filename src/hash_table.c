#include "hash_table.h"

void htable_set(htable **ht, const unsigned char *key, size_t key_len,
                const unsigned char *value, size_t value_len) {
    htable_delete(ht, key, key_len);
    // tmp struct
    htable *s = (struct htable *) calloc(1, sizeof(htable));
    // write key and key_len
    s->key = calloc(key_len, sizeof (unsigned char));
    memcpy(s->key, key, key_len);
    s->key_len = key_len;
    // write value and value_len
    s->value = calloc(value_len, sizeof (unsigned char));
    memcpy(s->value, value, value_len);
    s->value_len = value_len;
    // add s to hash table
    HASH_ADD_KEYPTR(hh, *ht, s->key, key_len, s);
}

htable *htable_get(htable **ht, const unsigned char *key, size_t key_len) {
    struct htable *s;
    HASH_FIND(hh, *ht, key, key_len, s);
    return s;
}

int htable_delete(htable **ht, const unsigned char *key, size_t key_len) {
    struct htable *s;
    HASH_FIND(hh, *ht, key, key_len, s);
    if(s == NULL) {
        return -1;
    }
    free(s->key);
    free(s->value);
    HASH_DELETE(hh, *ht, s);
    free(s);
    return 0;
}
