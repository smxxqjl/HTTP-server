/******************************************************************************
 * liso_server.c                                                               *
 *                                                                             *
 *                                                                             *
 * Authors: smxxqjl <smxxqjl@gmail.com>,                                       *
 *                                                                             *
 *                                                                             *
 *******************************************************************************/

#include <getopt.h>
#include <openssl/ssl.h>
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
#include <strings.h>
#include <netinet/tcp.h>
#include <netinet/in.h>

#define CRLF "\r\n"
#define MAXREQUEST 8192
#define MAXLINE 1024
#define MAXBUF 1024

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

#define MAXPATH 255
#define LISO_PORT 9999
#define BUF_SIZE 4096
#define PORT_LEN 6
// Maximum number of file descriptor that server can handle
static int use_ssl = 0;
static int use_log = 0;
static int use_daemon = 0;
static int liso_port = 9999;
//static int optional_argument = 2;
static char log_file[MAXPATH] = "log/access.log";
static char cert_file[MAXPATH] = "certs/myca.crt";
static char port_ascii[PORT_LEN] = "9999";
#define GETOPT_SSL 1
#define GETOPT_LOG 2
#define GETOPT_PORT 3
#define GETOPT_HELP 4
void usage() {
    printf("Usage: liso_exe_name [OPTIONS]...\n");
    printf("Running a lightweight HTTP Server.\n");
    printf("\n");
    printf("-h, -help, Looking for help?\n");
    printf("--ssl=[cert_file], run httlps(http over ssl) using specified"
           "certificate file.\n");
    printf("-p portnum, --port=portnum running server on specified port.\n");
    printf("--log=[logfile], generate a access log file as specified.\n");
}

ssize_t ssl_readfeedline(SSL *fd, void *vptr, size_t maxlen);
void ssl_select(struct sockaddr_in);
//TODO: Add arguments for request method
void setup_environ() {
    setenv("SERVER_NAME", "127.0.0.1", 1);
    setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
    setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
    setenv("SERVER_PORT", port_ascii, 1);
}

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
            "Content-Length: %d\r\n"
            "Content-Type: text/html\r\n"
            "\r\n %s"
            ,status_code, reason, (int)strlen(reason), reason);
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


#define MAXARGS 100
void serve_dynamic(int connfd, char *exepath, char *header)
{
    int pid, i;
    char *argv[MAXARGS];

    /*
     * No need to produce the header in server code
     * CGI script will generate automatically
     * writen(connfd, header, strlen(header));
     */

    /* argument parsing */
    char *p = strchr(exepath, '?');

    i = 0;
    argv[i++] = exepath;
    if (p != NULL) {
        do {
            if (i >= MAXARGS - 1)
                fprintf(stderr, "Too many argv for the script\n");
            *p++ = '\0';  /* Chop out last string */
            argv[i++] = p;
        }while((p = strchr(p, '&')) != NULL);
    }
    argv[i] = NULL;

    if ((pid = fork()) == 0) {
        dup2(connfd, STDOUT_FILENO);
        setenv("SERVER_SOFTWARE", "Liso/1.0", 1);
        setenv("PATH_INFO", exepath, 1);
        setup_environ();
        execv("flaskr/wsgi_wrapper.py", argv);
        fprintf(stderr, "Execv error\n");
        abnormal_response(connfd, 500, "Internal Server Error");
    }
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

    char *query_str = strchr(path, '?');
    if (query_str != NULL) {
        setenv("QUERY_STRING", query_str, 1);
        printf("Set query string to %s\n", readbuf);
    }
    int contentlen;
    contentlen = 0;
    int re = 0;
    printf("Request Header:\n");
    unsetenv("HTTP_COOKIE");
    while(readfeedline(connfd, readbuf, MAXBUF) > 2) {
        printf("header is %s", readbuf);
        //strtolower(readbuf);
        if (startwith(readbuf, "content-length:")) {
            char *lenstr = strchr(readbuf, ':');
            *lenstr = '\0';
            lenstr++;
            char *p = strchr(lenstr, '\r');
            *p = '\0';
            lenstr = igspace(lenstr);
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
        } else if (startwith(readbuf, "cookie")) {
            char *constr = strchr(readbuf, ':');
            *constr = '\0';
            constr++;
            constr = igspace(constr);
            char *temp = strchr(constr, '\r');
            *temp = '\0';
            setenv("HTTP_COOKIE", constr, 1);
            printf("Set cookie constr %s\n", constr);
        }
    }

    if((re = readn(connfd, readbuf, contentlen)) != contentlen){
        perror("Connfd read error\n");
    }
    readbuf[contentlen] = '\0';
    if (re != 0) {
        setenv("QUERY_STRING", readbuf, 1);
        printf("Set query string to %s\n", readbuf);
    }
    printf("Body is %s \n", readbuf);

    /* check permission and write back response */
    reason = "Unauthorized";
    if (startwith(index_file, "cgi")) {
        serve_dynamic(connfd, index_file, header);
    } else if (access(index_file, F_OK) != 0) {
        abnormal_response(connfd, 404, reason);
    } else {
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
                    "Server: Lisoss/1.0\r\n"
                    "Trasfer-Encoding: chunked\r\n",
                    currenttime, modifiedtime);
            int headerlen;
            headerlen = strlen(header);
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
        else {
            abnormal_response(connfd, 404, reason);
        }
    }
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
    if (use_log) {
        fprintf(accesslog, "%s", line);
        fflush(accesslog);
    }
    i = 0;
    while(line[i] != ' ' && line[i] != '\0' && i < MAXMETHOD) {
        method[i] = line[i];
        i++;
    }
    if(i == MAXMETHOD)
        methodtype = UNKNOWN;
    else {
        method[i] = '\0';
        setenv("REQUEST_METHOD", method, 1);
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
            reason = "Unknown method";
            abnormal_response(connfd, 400, reason);
            break;
    }
    close(connfd);
    return 1;
}

int start_connect();
int main(int argc, char* argv[])
{
    int c;

    static struct option long_options[] = {
        { "ssl",  optional_argument, NULL, GETOPT_SSL},
        { "log",  optional_argument, NULL, GETOPT_LOG},
        { "port", optional_argument, NULL, GETOPT_PORT},
        { "help", no_argument,       NULL, GETOPT_HELP},
        {  NULL,                  0, NULL, 0}
    };

    while((c = getopt_long(argc, argv, "p:dh", long_options, NULL)) != -1) {
        switch (c) {
            case GETOPT_SSL:
                use_ssl = 1;
                if (optarg != NULL)
                    strncpy(cert_file, optarg, MAXPATH);
                break;
            case GETOPT_LOG:
                use_log = 1;
                if (optarg != NULL)
                    strncpy(log_file, optarg, MAXPATH);
                break;
            case GETOPT_PORT:
            case 'p':
                if (!isinteger(optarg)) {
                    fprintf(stderr, "Invalid port number optargs\n");
                    return 0;
                }
                strncpy(port_ascii, optarg, PORT_LEN);
                liso_port = atoi(optarg);
                break;
            case 'd':
                use_daemon = 1;
                break;
            case 'h':
            case GETOPT_HELP:
                usage();
                return 0;
            case '?':
            default:
                return 1;
        }
    }
    if (use_daemon)
        daemonize("lisolog");
    if (use_log) {
        accesslog = fopen(log_file, "w+");
        if (accesslog == NULL) {
            perror("Error while opening log_file no log will show.\n");
            use_log = 0;
        }
    }

    start_connect(argc, argv);
    return 0;
}

int start_connect()
{
    int sock, client_sock, i, maxi, maxfd, nready, flag;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    fd_set rset, allset;

    install_mime();


    addr.sin_family = AF_INET;
    addr.sin_port = htons(liso_port);
    addr.sin_addr.s_addr = INADDR_ANY;
    int client[FD_SETSIZE];
    for (i = 0; i < FD_SETSIZE; i++)
        client[i] = -1;

    if (use_ssl)
        ssl_select(addr);

    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }
    if (setsockopt(sock, SOL_SOCKET,SO_REUSEADDR, (char *)&flag, sizeof(int)) == -1)
    {                                                                               
        perror("reuse error\n");
    } 
    flag = 1;
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) == -1) {
        printf("Nodelay error\n");
    }

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
            if (i == FD_SETSIZE) { fprintf(stderr, "too many clients.\n"); return EXIT_FAILURE; } 
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
void ssl_abnormal_response(SSL* connfd, int status_code, char *reason)
{
    char response[MAXERROR];

    snprintf(response, MAXERROR, "HTTP/1.1 %d %s\r\n"
            "Content-Length: %d\r\n"
            "Content-Type: text/html\r\n"
            "\r\n %s"
            ,status_code, reason, (int)strlen(reason), reason);
    SSL_write(connfd, response, strlen(response));
}

int ssl_method_handle(SSL* cli_context, char *requestline)
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

    printf("SSL index file is %s\n", index_file);
    char *query_str = strchr(path, '?');
    if (query_str != NULL) {
        setenv("QUERY_STRING", query_str, 1);
        printf("Set query string to %s\n", readbuf);
    }

    /* check permission and write back response */
    if (access(index_file, F_OK) != 0) {
        reason = "Not Found";
        ssl_abnormal_response(cli_context, 404, reason);
    } else {
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
                    "Server: Lisoss/1.0\r\n"
                    "Trasfer-Encoding: chunked\r\n",
                    currenttime, modifiedtime);
            int headerlen;
            headerlen = strlen(header);
            int readnum;
            content_len = buf.st_size;
            /* Get extension of file name to fill content-type header */
            for(i = strlen(index_file) - 1; i >= 0; i--)
                if (index_file[i] == '.')
                    break;

            char *ext = index_file+i+1;
            printf("ext is %s\n", ext);
            struct nlist *type = lookup(ext);
            char mime_name[MAXLINE];
            if (type == NULL || i < 0) /* no extension or corresponding MIME-type name is found */
                strncpy(mime_name, "application/octet-stream", MAXLINE);
            else
                strncpy(mime_name, type->defn, MAXLINE);
            printf("type is %s\n", mime_name);

            snprintf(header + headerlen, MAXREQUEST - headerlen,
                    "Content-Length: %d\r\n"
                    "Content-Type: %s\r\n"
                    "\r\n", content_len, mime_name);
            SSL_write(cli_context, header, strlen(header));
            while ((readnum = read(wfilefd, &readbuf, MAXBUF)) != 0)
                SSL_write(cli_context, readbuf, readnum);
        }
        else {
            reason = "xx";
            ssl_abnormal_response(cli_context, 404, reason);
        }
    }
    return 0;
}

int ssl_request_handle(SSL *client_context) {
    char buf[BUF_SIZE], line[BUF_SIZE];
    int first = 1;

    bzero(line, BUF_SIZE);
    while (ssl_readfeedline(client_context, buf, BUF_SIZE) > 2) {
        if (first || strlen(line) == 0) {
            first = 0;
            strcpy(line, buf);
        }
        printf("Wow Using ssl read something %s\n", buf);
        fflush(stdout);
        bzero(buf, MAXBUF);
    }
    if (strlen(line) != 0) {
        ssl_method_handle(client_context, line);
    }
    return 1;
}

void ssl_select(struct sockaddr_in addr) 
{
    int clients[FD_SETSIZE];
    SSL_CTX *ssl_context;
    SSL *client_context_set[FD_SETSIZE], *client_context;
    fd_set rset, allset;
    int maxfd, maxi, i, client_sock, acceptfd;
    struct sockaddr_in cli_addr;
    socklen_t cli_size;

    for (i = 0; i < FD_SETSIZE; i++) {
        client_context_set[i] = NULL;
        clients[i] = -1;
    }
    maxi = -1;


    SSL_load_error_strings();
    SSL_library_init();

    /* we want to use TLSv1 only */
    if ((ssl_context = SSL_CTX_new(TLSv1_server_method())) == NULL)
    {
        fprintf(stderr, "Error creating SSL context.\n");
        exit(1);
    }

    /* register private key */
    if (SSL_CTX_use_PrivateKey_file(ssl_context, "private/myca.key",
                SSL_FILETYPE_PEM) == 0)
    {
        SSL_CTX_free(ssl_context);
        fprintf(stderr, "Error associating private key.\n");
        exit(1);
    }

    /* register public key (certificate) */
    if (SSL_CTX_use_certificate_file(ssl_context, "certs/myca.crt",
                SSL_FILETYPE_PEM) == 0)
    {
        SSL_CTX_free(ssl_context);
        fprintf(stderr, "Error associating certificate.\n");
        exit(1);
    }
    /************ END SSL INIT ************/

    if ((acceptfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        SSL_CTX_free(ssl_context);
        fprintf(stderr, "Failed creating socket.\n");
        exit(1);
    }
    FD_ZERO(&allset);
    FD_SET(acceptfd, &allset);
    maxfd = acceptfd;

    if (bind(acceptfd, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(acceptfd);
        SSL_CTX_free(ssl_context);
        fprintf(stderr, "Failed binding socket.\n");
        exit(1);
    }

    if (listen(acceptfd, 5))
    {
        close_socket(acceptfd);
        SSL_CTX_free(ssl_context);
        fprintf(stderr, "Error listening on socket.\n");
        exit(1);
    }
    /************ SERVER SOCKET SETUP ************/
    while(1) {
        rset = allset;
        int nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(acceptfd, &rset)) {
            cli_size = sizeof(cli_addr);
            if ((client_sock = accept(acceptfd, (struct sockaddr *) &cli_addr,
                            &cli_size)) == -1) {
                close(acceptfd);
                SSL_CTX_free(ssl_context);
                fprintf(stderr, "Error accepting connection.\n");
                exit(1);
            }
            for (i = 0; i < FD_SETSIZE; i++) {
                if (clients[i] < 0) {
                    clients[i] = client_sock;
                    break;
                }
            }
            if (i == FD_SETSIZE) {
                fprintf(stderr, "too many clients.\n");
                exit(1);
            }

            if ((client_context = SSL_new(ssl_context)) == NULL)
            {
                close(acceptfd);
                SSL_CTX_free(ssl_context);
                fprintf(stderr, "Error creating client SSL context.\n");
                exit(1);
            }
            if (SSL_set_fd(client_context, client_sock) == 0)
            {
                close(acceptfd);
                SSL_free(client_context);
                SSL_CTX_free(ssl_context);
                fprintf(stderr, "Error creating client SSL context.\n");
                exit(1);
            }  

            if (SSL_accept(client_context) <= 0)
            {
                close(acceptfd);
                SSL_free(client_context);
                SSL_CTX_free(ssl_context);
                fprintf(stderr, "Error accepting (handshake) client SSL context.\n");
                exit(1);
            }
            client_context_set[i] = client_context;
            FD_SET(clients[i], &allset);

            if (client_sock > maxfd) {
                maxfd = client_sock;
            }
            if (i > maxi) {
                maxi = i;
            }
            if (--nready <= 0)
                continue;
        }

        for (i = 0; i <= maxi; i++) {
            if (clients[i] < 0) {
                continue;
            }
            if (FD_ISSET(clients[i], &rset)) {
                if (ssl_request_handle(client_context_set[i]) == 1) {
                    SSL_free(client_context_set[i]);
                    client_context_set[i] = NULL;
                    close(clients[i]);
                    clients[i] = -1;
                }
                if (--nready <= 0)
                    break;
            }
        }
    }
}
