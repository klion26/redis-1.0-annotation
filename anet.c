/* anet.c -- Basic TCP socket stuff made a bit less boring
 *
 * Copyright (c) 2006-2009, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "fmacros.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include "anet.h"

/**
 * local function for setting error messages
 * @param err, space to hold the error message.
 */
static void anetSetError(char *err, const char *fmt, ...)
{
    va_list ap;

    if (!err) return;
    va_start(ap, fmt);
    vsnprintf(err, ANET_ERR_LEN, fmt, ap);
    va_end(ap);
}

/**
 * set fd to nonblocking by using fcntl(2)
 */
int anetNonBlock(char *err, int fd)
{
    int flags;

    /* Set the socket nonblocking.
     * Note that fcntl(2) for F_GETFL and F_SETFL can't be
     * interrupted by a signal. */
    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        anetSetError(err, "fcntl(F_GETFL): %s\n", strerror(errno));
        return ANET_ERR;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        anetSetError(err, "fcntl(F_SETFL,O_NONBLOCK): %s\n", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

/**
 * set socket nodelay
 */
int anetTcpNoDelay(char *err, int fd)
{
    int yes = 1;
    /**
     * int setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len);
     * IPPROTO_TCP has been defined in in.h on Mac with value of 6
     * TCP_NODELAY has been defined in tcp.h on Mac with value of 0x01
     */
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) == -1)
    {
        anetSetError(err, "setsockopt TCP_NODELAY: %s\n", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

/**
 * set the socket send buff
 */
int anetSetSendBuffer(char *err, int fd, int buffsize)
{
    /**
     * SOL_SOCKET has been defined in socket.h on Mac with value of 0xffff
     * SO_SNDBUF has been defined in socket.h on Mac with value of 0x1001
     * the setsockopt function will set the send buff size to buffsize
     */
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffsize, sizeof(buffsize)) == -1)
    {
        anetSetError(err, "setsockopt SO_SNDBUF: %s\n", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

/**
 * set the tcp to keep alive
 */
int anetTcpKeepAlive(char *err, int fd)
{
    int yes = 1;
    /**
     * SOL_SOCKET has been defined in socket.h on Mac with value of 0xffff
     * SO_KEEPALIVE has been defined in socket.h on Mac with value of 0x0008
     * the following function will set tcp to keep alive.
     */
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes)) == -1) {
        anetSetError(err, "setsockopt SO_KEEPALIVE: %s\n", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

/**
 * translate host to ip address
 * @param err
 * @param host: NUL-terminated ip/hostname
 * @param ipbuf: the ip address will be stored. Users have to allocate the space for ipbuf
 */
int anetResolve(char *err, char *host, char *ipbuf)
{
    struct sockaddr_in sa;

    sa.sin_family = AF_INET;
    /**
     * treats host as ip address
     * if inet_aton fails (return 0 means fail), return 1 means success
     */
    if (inet_aton(host, &sa.sin_addr) == 0) {
        struct hostent *he;
        /**
         *  treats host as hostname
         */
        he = gethostbyname(host);
        if (he == NULL) {
            anetSetError(err, "can't resolve: %s\n", host);
            return ANET_ERR;
        }
        memcpy(&sa.sin_addr, he->h_addr, sizeof(struct in_addr));
    }
    /**
     * copy the address to ipbuf
     */
    strcpy(ipbuf,inet_ntoa(sa.sin_addr));
    return ANET_OK;
}

#define ANET_CONNECT_NONE 0
#define ANET_CONNECT_NONBLOCK 1
/**
 * return a socket defined by (addr, port, flags)
 * @param addr: address
 * @param port: port
 * @param flags: ANET_CONECT_NON/ANET_CONNECT_NONBLOCK to specify block or nonblock type
 */
static int anetTcpGenericConnect(char *err, char *addr, int port, int flags)
{
    int s, on = 1;
    struct sockaddr_in sa;
    /**
     * create a tcp socket
     */
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        anetSetError(err, "creating socket: %s\n", strerror(errno));
        return ANET_ERR;
    }
    /* Make sure connection-intensive things like the redis benckmark
     * will be able to close/open sockets a zillion of times */
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    /**
     * set up sa using addr and port
     */
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    if (inet_aton(addr, &sa.sin_addr) == 0) {
        struct hostent *he;

        he = gethostbyname(addr);
        if (he == NULL) {
            anetSetError(err, "can't resolve: %s\n", addr);
            close(s);
            return ANET_ERR;
        }
        memcpy(&sa.sin_addr, he->h_addr, sizeof(struct in_addr));
    }
    /**
     * set nonblock flag
     */
    if (flags & ANET_CONNECT_NONBLOCK) {
        if (anetNonBlock(err,s) != ANET_OK)
            return ANET_ERR;
    }
    /**
     * connect to sa{addr:port}
     */
    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        /**
         * nonblock type and we are inprogress, return the current socket
         */
        if (errno == EINPROGRESS &&
            flags & ANET_CONNECT_NONBLOCK)
            return s;

        anetSetError(err, "connect: %s\n", strerror(errno));
        close(s);
        return ANET_ERR;
    }
    return s;
}
/**
 * connect to {addr:port} with block type
 */
int anetTcpConnect(char *err, char *addr, int port)
{
    return anetTcpGenericConnect(err,addr,port,ANET_CONNECT_NONE);
}

/**
 * connect to {addr:port} with nonblock type
 */
int anetTcpNonBlockConnect(char *err, char *addr, int port)
{
    return anetTcpGenericConnect(err,addr,port,ANET_CONNECT_NONBLOCK);
}

/* Like read(2) but make sure 'count' is read before to return
 * (unless error or EOF condition is encountered) */
int anetRead(int fd, char *buf, int count)
{
    int nread, totlen = 0;
    /**
     * read at most count characters from fd to buf
     */
    while(totlen != count) {
        nread = read(fd,buf,count-totlen);
        /**
         * reach the end of fd
         */
        if (nread == 0) return totlen;
        /**
         * error
         */
        if (nread == -1) return -1;
        totlen += nread;
        buf += nread;
    }
    /**
     * return the length we read from fd
     */
    return totlen;
}

/* Like write(2) but make sure 'count' is read before to return
 * (unless error is encountered) */
/**
 * @param fd: file descriptor
 * @param buf: the content we will write to fd
 * @param count : the number of characters we will write to fd
 */
int anetWrite(int fd, char *buf, int count)
{
    int nwritten, totlen = 0;
    while(totlen != count) {
        nwritten = write(fd,buf,count-totlen);
        if (nwritten == 0) return totlen;
        if (nwritten == -1) return -1;
        totlen += nwritten;
        buf += nwritten;
    }
    return totlen;
}

int anetTcpServer(char *err, int port, char *bindaddr)
{
    int s, on = 1;
    struct sockaddr_in sa;
    
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        anetSetError(err, "socket: %s\n", strerror(errno));
        return ANET_ERR;
    }
    /**
     * set address reuseable, so we can open/close zillian times
     */
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
        anetSetError(err, "setsockopt SO_REUSEADDR: %s\n", strerror(errno));
        close(s);
        return ANET_ERR;
    }
    memset(&sa,0,sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    /**
     * if bindaddr isn't NULL, bind the address to sa
     */
    if (bindaddr) {
        if (inet_aton(bindaddr, &sa.sin_addr) == 0) {
            anetSetError(err, "Invalid bind address\n");
            close(s);
            return ANET_ERR;
        }
    }
    /**
     * bind the server address to socket s
     */
    if (bind(s, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        anetSetError(err, "bind: %s\n", strerror(errno));
        close(s);
        return ANET_ERR;
    }
    /**
     * listen at most 64 sockets to connect
     */
    if (listen(s, 64) == -1) {
        anetSetError(err, "listen: %s\n", strerror(errno));
        close(s);
        return ANET_ERR;
    }
    return s;
}

/**
 * accept a connect, save the client's ip and port
 * to the paramenter @ip and @port
 * @param serversock: server fd
 * @param ip: if it is not null, the client ip will save to it
 * @param port: the space to hold the connect client port
 */
int anetAccept(char *err, int serversock, char *ip, int *port)
{
    int fd;
    struct sockaddr_in sa;
    unsigned int saLen;

    /**
     * accept a connect
     */
    while(1) {
        saLen = sizeof(sa);
        fd = accept(serversock, (struct sockaddr*)&sa, &saLen);
        if (fd == -1) {
            if (errno == EINTR)
                continue;
            else {
                anetSetError(err, "accept: %s\n", strerror(errno));
                return ANET_ERR;
            }
        }
        break;
    }
    if (ip) strcpy(ip,inet_ntoa(sa.sin_addr));
    if (port) *port = ntohs(sa.sin_port);
    return fd;
}
