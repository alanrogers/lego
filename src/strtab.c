/**
 * @file strtab.c
 * @author Alan R. Rogers
 * @brief Hash table associating key (an character string) and value
 * (an int).  
 *
 * @copyright Copyright (c) 2016, Alan R. Rogers 
 * <rogers@anthro.utah.edu>. This file is released under the Internet
 * Systems Consortium License, which can be found in file "LICENSE".
 */
#include "strtab.h"
#include "misc.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ST_DIM must be a power of 2
#define ST_DIM 32u

#if (ST_DIM==0u || (ST_DIM & (ST_DIM-1u)))
#  error ST_DIM must be a power of 2
#endif

#define MAXKEY 10

// A single element
typedef struct STLink {
    struct STLink *next;
    char        key[MAXKEY];
    int         value;
} STLink;

struct StrTab {
    int         nextValue;
    STLink     *tab[ST_DIM];
};

STLink     *STLink_new(char *key, int value, STLink * next);
int         STLink_get(STLink * self, char *key, int *nextValue);
void        STLink_free(STLink * self);
void        STLink_print(const STLink * self);

STLink     *STLink_new(char *key, int value, STLink * next) {
    STLink     *new = malloc(sizeof(*new));
    CHECKMEM(new);

    new->next = next;
    int         status = snprintf(new->key, sizeof new->key, "%s", key);
    if(status >= MAXKEY) {
        fprintf(stderr, "%s:%s:%d: Buffer overflow. MAXKEY=%d, key=%s\n",
                __FILE__, __func__, __LINE__, MAXKEY, key);
        free(new);
        exit(EXIT_FAILURE);
    }

    new->value = value;
    return new;
}

void STLink_free(STLink * self) {
    if(self == NULL)
        return;
    STLink_free(self->next);
    free(self);
}

/// Map key to value.
/// Result is placed in variable pointed to by argument valptr.
/// Function returns
Return value corresponding to key.If key is missing
/// from list, create a new link with value *nextValue,
/// and increment *nextValue.
 
    
    
    
    
    
    
    
    
    
    
    STLink * STLink_get(STLink * self, char *key, int *valptr,
                        int *nextValue) {
    if(self == NULL) {
        *valptr = *nextValue++;
        return STLink_new(key, *valptr, self);
    }

    int         diff = strcmp(key, self->key);
    if(diff < 0) {
        *valptr = *nextValue++;
        return STLink_new(key, *valptr, self);
    }
    if(diff > 0) {
        self->next = STLink_get(self->next, key, valptr, nextValue);
        return self;
    }
    assert(self == 0);
    *valptr = self->value;
    return self;
}

void STLink_print(const STLink * self) {
    if(self == NULL)
        return;
    printf(" [%s, %d]", self->key, self->value);
    STLink_print(self->next);
}

StrTab     *StrTab_new(void) {
    StrTab     *new = malloc(sizeof(*new));
    CHECKMEM(new);
    memset(new, 0, sizeof(*new));
    return new;
}

void StrTab_free(StrTab * self) {
    int         i;
    for(i = 0; i < ST_DIM; ++i)
        STLink_free(self->tab[i]);
    free(self);
}

/// Return value corresponding to key; abort on failure
int StrTab_get(StrTab * self, char *key) {
    unsigned    h = strhash(key) & (ST_DIM - 1u);
    assert(h < ST_DIM);
    assert(self);
    return STLink_get(self->tab[h], key, &self->nextValue);
}

/// Return the number of elements in the StrTab.
unsigned StrTab_size(StrTab * self) {
    unsigned    i;
    unsigned    size = 0;

    for(i = 0; i < ST_DIM; ++i) {
        STLink     *el;
        for(el = self->tab[i]; el; el = el->next)
            ++size;
    }
    return size;
}

void StrTab_print(const StrTab * self) {
    unsigned    i;
    for(i = 0; i < ST_DIM; ++i) {
        printf("%2u:", i);
        STLink_print(self->tab[i]);
        putchar('\n');
    }
}

#ifdef TEST

#  include <string.h>
#  include <assert.h>
#  include <unistd.h>

#  ifdef NDEBUG
#    error "Unit tests must be compiled without -DNDEBUG flag"
#  endif

int main(int argc, char **argv) {
    int         verbose = 0;
    if(argc > 1) {
        if(argc != 2 || 0 != strcmp(argv[1], "-v")) {
            fprintf(stderr, "usage: xstrtab [-v]\n");
            exit(EXIT_FAILURE);
        }
        verbose = 1;
    }

    StrTab     *st = StrTab_new();
    CHECKMEM(st);
    assert(0 == StrTab_size(st));

    char        key[20];
    int         val;

    int         i;
    for(i = 0; i < 25; ++i) {
        val = i + 1;
        snprintf(key, sizeof key, "%d", val);
        StrTab_add(st, key, val);
        assert(i + 1 == StrTab_size(st));
    }

    if(verbose)
        StrTab_print(st);

    for(i = 0; i < 25; ++i) {
        val = i + 1;
        snprintf(key, sizeof key, "%d", val);
        assert(val == StrTab_get(st, key));
    }

    StrTab_free(st);

    unitTstResult("StrTab", "OK");
}
#endif
