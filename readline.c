#include <unistd.h>
#include <errno.h>
#include <openssl/ssl.h>

#define MAXLINE 2048
static int read_cnt;
static char *read_ptr;
static char read_buf[MAXLINE];

ssize_t my_read(int fd, char *ptr)
{
    if(read_cnt <= 0) {
again:
        if ((read_cnt=read(fd, read_buf, sizeof(read_buf))) < 0) {
            /* set when interrupted by a singal */
            if (errno == EINTR)
                goto again;
            return -1;
        }
        else if (read_cnt == 0)
            return 0;
        read_ptr = read_buf;
    }

    read_cnt--;
    *ptr = *read_ptr++;
    return 1;
}

ssize_t readn(int fd, void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nread;
    char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0;      /* and call read() again */
            else
                return (-1);
        } else if (nread == 0)
            break;

        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft);
}

ssize_t writen(int fd, void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;
            else
                return (-1);
        }

        nleft -= nwritten;
        ptr += nwritten;
    }
    return (n);
}

ssize_t readaline(int fd, void *vptr, size_t  maxlen)
{
    ssize_t n, rc;
    char c, *ptr;

    ptr = vptr;
    for(n = 1; n < maxlen; n++) {
        if((rc = read(fd, &c, 1)) == 1) {
            *ptr++ = c;
            if(c == '\n')
                break;
        }
        else if(rc == 0) {
            *ptr = 0;
            return n-1;
        }
        else
            return -1;
    }

    *ptr = 0;
    return n;
}

ssize_t readfeedline(int fd, void *vptr, size_t maxlen)
{
    ssize_t n, rc;
    char c, *ptr;
    int carryflag;

    ptr = vptr;
    for(n = 1; n < maxlen; n++) {
        if((rc = read(fd, &c, 1)) == 1) {
            *ptr++ = c;
            if(c == '\r') {
                carryflag = 1;
                continue;
            }
            else if (carryflag && c == '\n')
                break;
            carryflag = 0;
        }
        else if(rc == 0) {
            *ptr = 0;
            return n-1;
        }
        else
            return -1;
    }

    *ptr = 0;
    return n-1;
}

ssize_t ssl_readfeedline(SSL *fd, void *vptr, size_t maxlen)
{
    ssize_t n, rc;
    char c, *ptr;
    int carryflag;

    ptr = vptr;
    for(n = 1; n < maxlen; n++) {
        if((rc = SSL_read(fd, &c, 1)) == 1) {
            *ptr++ = c;
            if(c == '\r') {
                carryflag = 1;
                continue;
            }
            else if (carryflag && c == '\n')
                break;
            carryflag = 0;
        }
        else if(rc == 0) {
            *ptr = 0;
            return n-1;
        }
        else
            return -1;
    }

    *ptr = 0;
    return n-1;
}

ssize_t readlinebuf(void **vptrptr)
{
    if(read_cnt)
        *vptrptr = read_ptr;
    return read_cnt;
}
