/******************************************************************************
 * liso_server.c                                                               *
 *                                                                             *
 *                                                                             *
 * Authors: smxxqjl <smxxqjl@gmail.com>,                                       *
 *                                                                             *
 *                                                                             *
 *******************************************************************************/

#include <ctype.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "liso.h"

#define CRLF "\r\n"
#define MAXREQUEST 8192
#define MAXLINE 1024

/* The longest method name is 6 char hence include null terminator we make it 7 */
#define MAXMETHOD 8
#define OPTIONS 0
#define GET 1
#define HEAD 2
#define POST 3
#define PUT 4
#define DELETE 5
#define TRACE 6
#define CONNECT 7
#define UNKNOWN 8

#define LISO_PORT 9999
#define BUF_SIZE 4096
// Maximum number of file descriptor that server can handle

void not_found(int connfd)
{
    return;
}

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}


#define MAXERROR 2048
void abnormal_response(int connfd, int status_code, char *reason)
{
    char response[MAXERROR];

    snprintf(response, MAXERROR, "HTTP/1.1 %d %s\r\n"
            "\r\n"
            ,status_code, reason);
    writen(connfd, response, strlen(response));
}

void install_mime()
{
    int i;

    int extnum = 12;
    char *ext[] = {"javascript", "json", "pdf", "xml", "zip", "mpeg", "vorbis", "css", "html", "png", "jpeg", "gif"};
    char *retype[] = {"application/javascript", "application/json", "application/pdf", "application/xml",
        "application/zip", "audio/mpeg", "audio/vorbis", "text/css", "text/html", "image/png", "image/jpeg",
        "image/gif"
    };

    for (i = 0; i < extnum; i++) {
        if (install(ext[i], retype[i]) == NULL)
            fprintf(stderr, "error while install %s: %s\n", ext[i], retype[i]);
    }
}

void serve_dynamic(int connfd, char *exepath, char *header)
{
    int pid;

    writen(connfd, header, strlen(header));
    if ((pid = fork()) == 0) {
        dup2(connfd, STDOUT_FILENO);
        setenv("SERVER_SOFTWARE", "Liso/1.0", 1);
        execl(exepath, exepath, NULL);
        abnormal_response(connfd, 500, "Internal Server Error");
    }
    wait(NULL);
}

/*
 * printheader return a header character string, and end with a blank /r/n line
 * contentlen parameter used to fill Content-Length with the value. And if negative value
 * is given, no Content-Length header line will be produced.
 */
char *printheader(int contentlen)
{
    return NULL;
}

#define MAXPATH 255
#define MAXBUF 1024
#define MAXTIME 50
FILE *accesslog;
int method_handle(int connfd, char *requestline, int method)
{
    int i, j, content_len, wfilefd, endconnect;
    char path[MAXPATH], *index_file, *reason, readbuf[MAXBUF], header[MAXREQUEST], currenttime[MAXTIME];
    char modifiedtime[MAXTIME];
    i = j = endconnect = 0;

    printf("Request:%s\n", requestline);
    /* Ignore method and space */
    while(requestline[i++] != ' ');
    i++;

    while(requestline[i] != ' ') {
        path[j++] = requestline[i++];
    }
    path[j] = '\0';

    for(i = 0; i < j; i++) {
        if (path[i] != '/')
            break;
    }
    if (i == j)
        index_file = "index.html";
    else
        index_file = path+i;

    /* check permission and write back response */
    reason = "Unauthorized";
    if (access(index_file, F_OK) != 0)
        abnormal_response(connfd, 404, reason);
    else {
        struct stat buf;
        if (access(index_file, R_OK) == 0 && (wfilefd = open(index_file, O_RDONLY)) != -1) {
            time_t temptp;
            time(&temptp);
            fstat(wfilefd, &buf);
            strncpy(currenttime,asctime(gmtime(&temptp)), MAXTIME);
            currenttime[strlen(currenttime) - 1] = '\0';

            strncpy(modifiedtime,ctime(&(buf.st_mtim.tv_sec)), MAXTIME);
            modifiedtime[strlen(modifiedtime) - 1] = '\0';
            snprintf(header, MAXREQUEST, "HTTP/1.1 200 OK \r\n"
                    "MIME-Version: 1.0\r\n"
                    "Date: %s\r\n"
                    "Last-Modified: %s\r\n"
                    "Server: liso/1.0\r\n"
                    "Trasfer-Encoding: chunked\r\n",
                    currenttime, modifiedtime);
            int headerlen;
            headerlen = strlen(header);
            if (startwith(index_file, "cgi")) {
                if (method == GET || method == POST) 
                    serve_dynamic(connfd, index_file, header);
            } else {
                int readnum;
                content_len = buf.st_size;
                /* Get extension of file name to fill content-type header */
                for(i = strlen(index_file) - 1; i >= 0; i--)
                    if (index_file[i] == '.')
                        break;

                char *ext = index_file+i+1;
                struct nlist *type = lookup(ext);
                char mime_name[MAXLINE];
                if (type == NULL || i < 0) /* no extension or corresponding MIME-type name is found */
                    strncpy(mime_name, "application/octet-stream", MAXLINE);
                else
                    strncpy(mime_name, type->defn, MAXLINE);

                snprintf(header + headerlen, MAXREQUEST - headerlen,
                        "Content-Length: %d\r\n"
                        "Content-Type: %s\r\n"
                        "\r\n", content_len, mime_name);
                writen(connfd, header, strlen(header));
                if (method == GET || method == POST)
                    while ((readnum = read(wfilefd, &readbuf, MAXBUF)) != 0)
                        writen(connfd, readbuf, readnum);
            }
        }
        else {
            abnormal_response(connfd, 404, reason);
        }
    }

    int contentlen;
    contentlen = 0;
    while(readfeedline(connfd, readbuf, MAXBUF) > 2) {
        printf("header is %s\n", readbuf);
        strtolower(readbuf);
        if (startwith(readbuf, "content-length:")) {
            char *lenstr = strchr(readbuf, ':');
            *lenstr = '\0';
            lenstr++;
            lenstr = igspace(lenstr);
            char *p = strchr(lenstr, '\n');
            *p = '\0';
            if (strlen(lenstr) == 0 || !isinteger(lenstr)) {
                fprintf(stderr, "Bad Header\n %s\n", lenstr);
                continue;
            } else {
                contentlen = atoi(lenstr);
            } 
        } else if (startwith(readbuf, "close:")) {
            char *constr = strchr(readbuf, ':');
            *constr = '\0';
            constr++;
            constr = igspace(constr);
            constr = strtolower(constr);
            endconnect = (strcmp(constr, "close") == 0);
        }
    }
    if (readn(connfd, readbuf, contentlen) != contentlen)
        perror("Connfd read error\n");

    if (endconnect)
        close(connfd);
    return endconnect;
}

#define MAXPATH 255
#define MAXBUF 1024
#define MAXLINE 1024
int request_handle(int connfd)
{	    
    char line[MAXLINE], method[MAXMETHOD], *reason;
    int i, methodtype;
    reason = NULL;

    if (readfeedline(connfd, line, MAXLINE) < 2)
        return 0; /* In case some null read */
    fprintf(accesslog, "%s", line);
    fflush(accesslog);
    i = 0;
    while(line[i] != ' ' && line[i] != '\0' && i < MAXMETHOD) {
        method[i] = line[i];
        i++;
    }
    if(i == MAXMETHOD)
        methodtype = UNKNOWN;
    else {
        method[i] = '\0';
        if (strcasecmp(method, "OPTIONS") == 0)
            methodtype = OPTIONS;
        else if (strcasecmp(method, "GET") == 0)
            methodtype = GET;
        else if (strcasecmp(method, "HEAD") == 0)
            methodtype = HEAD;
        else if (strcasecmp(method, "POST") == 0)
            methodtype = POST;
        else if (strcasecmp(method, "PUT") == 0)
            methodtype = PUT;
        else if (strcasecmp(method, "DELETE") == 0)
            methodtype = DELETE;
        else if (strcasecmp(method, "TRACE") == 0)
            methodtype = TRACE;
        else if (strcasecmp(method, "CONNECT") == 0)
            methodtype = CONNECT;
        else
            methodtype = UNKNOWN;
    }
    switch(methodtype) {
        case GET:
            return method_handle(connfd, line, GET);
            break;
        case HEAD:
            return method_handle(connfd, line, HEAD);
            break;
        case POST:
            return method_handle(connfd, line, POST);
            break;
        case OPTIONS:
        case PUT:
        case DELETE:
        case TRACE:
        case CONNECT:
            reason = "Not-Implement";
            abnormal_response(connfd, 501, reason);
            break;
        case UNKNOWN:
        default:
            abnormal_response(connfd, 400, reason);
            break;
    }
    close(connfd);
    return 1;
}

int main(int argc, char* argv[])
{
    int sock, client_sock, i, maxi, maxfd, nready, lport, reuse;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    fd_set rset, allset;

#ifndef DEBUG
    daemonize("lisolog");
#endif

    accesslog = fopen("access.log", "w+");
    if (accesslog == NULL) {
        perror("open log fille access.log error");
    }
    lport = LISO_PORT;
    if (argc >= 2) {
        int len = strlen(argv[1]);
        for (i = 0; i < len; i++) {
            if (!isdigit(argv[1][i])) {
                fprintf(stderr, "Unavailable port argv %s Using default port %d",
                        argv[1], lport);
            }
        }
        if (i == len)
            lport = atoi(argv[1]);
    }

    install_mime();
    int client[FD_SETSIZE];
    for (i = 0; i < FD_SETSIZE; i++)
        client[i] = -1;

    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }
    if (setsockopt(sock, SOL_SOCKET,SO_REUSEADDR, (char *)&reuse, sizeof(int)) == -1)
    {                                                                               
        perror("resuse error\n");
    } 
    addr.sin_family = AF_INET;
    addr.sin_port = htons(lport);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

    maxi = -1;
    maxfd = sock;

    if (listen(sock, 5))
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    FD_ZERO(&allset);
    FD_SET(sock, &allset);

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        /* structure assignment, is this ok? */
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(sock, &rset)) { /* new client connection */
            cli_size = sizeof(cli_addr);
            if ((client_sock = accept(sock, (struct sockaddr *) &cli_addr,
                            &cli_size)) == -1) {
                close(sock);
                fprintf(stderr, "Error accepting connection.\n");
                return EXIT_FAILURE;
            }
            for (i = 0; i < FD_SETSIZE; i++) {
                if (client[i] < 0) {
                    client[i] = client_sock;
                    break;
                }
            }
            if (i == FD_SETSIZE) {
                fprintf(stderr, "too many clients.\n");
                return EXIT_FAILURE;
            }

            FD_SET(client_sock, &allset);

            if (client_sock > maxfd)
                maxfd = client_sock;
            if (i > maxi)
                maxi = i;

            if (--nready <= 0)
                continue;
        }

        for (i = 0; i <= maxi; i++) {  /* check all clients for data */
            if ((client_sock = client[i]) < 0)
                continue;
            if (FD_ISSET(client_sock, &rset)) {
                if (request_handle(client_sock) != 0)
                    client[i] = -1;
                if (--nready <= 0)
                    break;  /* no more ready socket */
            }
        }
    }
    return EXIT_SUCCESS;
}
