#ifndef READLINE
ssize_t readaline(int, void *, void *ptr, ssize_t maxlen);
ssize_t readfeedline(int ,void *, size_t);
ssize_t writen(int fd, void *vptr, size_t writelen);
#endif
#define READLINE
