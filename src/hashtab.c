#include "hashtab.h"
#include "misc.h"
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define KEYSIZE 20

// HASHDIM must be a power of 2
#define HASHDIM 32u

// Make sure HASHDIM is a power of 2
#if (HASHDIM==0u || (HASHDIM & (HASHDIM-1u)))
# error HASHDIM must be a power of 2
#endif

// A single element in a sparse matrix.
struct El {
    struct El  *next;
    char        key[KEYSIZE];
    void       *value;
};

struct HashTab {
    El         *tab[HASHDIM];
};

struct HashTabSeq {
    struct HashTab *ht;
    int ndx;
    El *el;
};

unsigned    strhash(const char *ss);
El         *El_new(const char *key);
void        El_free(El * e);
El         *El_find(El * self, El ** found, const char *key);
void        HashTabSeq_nextBucket(HashTabSeq *self);

/// Hash a character string
unsigned strhash(const char *ss) {
    unsigned long hashval;
    int c;
    const unsigned char *s = (const unsigned char *) ss;
#if 0
    // Kernighan and Richie, 2nd edn.
    for(hashval = 0; *s != '\0'; ++s)
        hashval += c + 31 * hashval;
#elif 1
    // djb2
    hashval = 5381;
    while((c = *s++))
        hashval = ((hashval << 5) + hashval) +  c;
#else
    // sdbm
    hashval = 0;
    while((c = *s++))
        hashval = c + (hashval << 6) + (hashval << 16) - hashval;
#endif

    // Same as hash % HASHDIM but faster. Requires
    // that HASHDIM be a power of 2.
    return hashval  & (HASHDIM-1u);
}

El         *El_new(const char *key) {
    El         *new = malloc(sizeof(*new));
    CHECKMEM(new);

    new->next = NULL;
    snprintf(new->key, sizeof(new->key), "%s", key);
    assert(0 == strncmp(new->key, key, sizeof(new->key)));
    new->value = NULL;
    return new;
}

void El_free(El * e) {
    if(e == NULL)
        return;
    El_free(e->next);
    free(e);
}

void * El_get(El * self) {
    assert(self);
    return self->value;
}

void El_set(El * self, void *value) {
    assert(self);
    self->value = value;
}

El *El_find(El * self, El ** found, const char *key) {
    int comparison;
    if(self == NULL
       || 0 > (comparison = strncmp(key, self->key, sizeof(self->key)))) {
        *found = El_new(key);
        (*found)->next = self;
        return *found;
    } else if(comparison > 0) {
        self->next = El_find(self->next, found, key);
        return self;
    }
    assert(0 == strncmp(key, self->key, sizeof(self->key)));
    *found = self;
    return self;
}

void El_printShallow(El * self) {
    if(self == NULL)
        return;
    printf(" [%s, %p]", self->key, self->value);
}

void El_print(El * self) {
    if(self == NULL)
        return;
    El_printShallow(self);
    El_print(self->next);
}

HashTab    *HashTab_new(void) {
    HashTab    *new = malloc(sizeof(*new));
    CHECKMEM(new);
    memset(new->tab, 0, sizeof(new->tab));
    return new;
}

void HashTab_free(HashTab * self) {
    int i;
    for(i=0; i < BT_DIM; ++i)
        El_free(self->tab[i]);
    free(self);
}

El *HashTab_get(HashTab * self, const char *key) {

    unsigned h = strhash(key);
    assert(h < HASHDIM);

    assert(self);

    El *el = NULL;
    self->tab[h] = El_find(self->tab[h], &el, key);
    assert(el);

    return el;
}

/// Return the number of elements in the HashTab.
unsigned long HashTab_size(HashTab * self) {
    unsigned    i;
    unsigned long size = 0;

    for(i = 0; i < HASHDIM; ++i) {
        El *el;
        for(el = self->tab[i]; el; el = el->next)
            ++size;
    }
    return size;
}

void HashTab_print(HashTab *self) {
    unsigned i;
    for(i=0; i < HASHDIM; ++i) {
        printf("%2u:", i);
        El_print(self->tab[i]);
        putchar('\n');
    }
}

// This object is for stepping through the entries of a hash table.
HashTabSeq *HashTabSeq_new(HashTab *ht) {
    HashTabSeq *self = malloc(sizeof(*self));
    CHECKMEM(self);

    self->ht = ht;
    self->ndx = -1;
    self->el = NULL;
    return self;
}

// Find the next bucket, ignoring empty buckets. If all remaining buckets
// are empty, set self->ndx equal to HASHDIM.
void HashTabSeq_nextBucket(HashTabSeq *self) {
    ++self->ndx;
    while(self->ndx < HASHDIM && self->ht->tab[self->ndx]==NULL)
        ++self->ndx;
}

// Return the next element in table.
El *HashTabSeq_next(HashTabSeq *self) {
    if(self->ndx < 0) {
        HashTabSeq_nextBucket(self);
        if(self->ndx == HASHDIM) {
            self->el = NULL;
            return NULL;
        }
        self->el = self->ht->tab[self->ndx];
        return self->el;
    }
    if(self->el == NULL) {
        if(self->ndx != HASHDIM) {
            fprintf(stderr,"%s:%s:%d: this shouldn't happen.",
                    __FILE__, __func__, __LINE__);
            exit(EXIT_FAILURE);
        }
        return NULL;
    }
    self->el = self->el->next;
    if(self->el == NULL) {
        HashTabSeq_nextBucket(self);
        if(self->ndx == HASHDIM)
            return NULL;
        self->el = self->ht->tab[self->ndx];
    }
    return self->el;
}

void HashTabSeq_free(HashTabSeq *self) {
    free(self);
}
