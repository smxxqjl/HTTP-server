#ifndef LISO_H
struct nlist {      /* table entry */
    struct nlist *next; /* next entry in the chain */
    char *name;         /* defined name */
    char *defn;         /* replacement text */
};

struct nlist *lookup(char *s);
struct nlist *install(char *name, char *defn);
ssize_t readaline(int, void *, void *ptr, ssize_t maxlen);
ssize_t readfeedline(int ,void *, size_t);
ssize_t writen(int fd, void *vptr, size_t writelen);
ssize_t readn(int fd, void *vptr, size_t n);
int startwith(char *, char *);
void *strtolower(char *);
char *igspace(char *);
int isinteger(char *);
int daemonize(char* lock_file);
#endif
