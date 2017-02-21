#ifndef HASH_H
struct nlist {      /* table entry */
    struct nlist *next; /* next entry in the chain */
    char *name;         /* defined name */
    char *defn;         /* replacement text */
};

struct nlist *lookup(char *s);
struct nlist *install(char *name, char *defn);
#endif
