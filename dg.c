#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "spool/sp.h"

struct irc_conn{
    struct spool_t msg_handlers;

    _Atomic _Bool nick_set;
    /*_Atomic _Bool active;*/
    /*_Atomic _Bool in_channel;*/
    int sock;
    FILE* fp;
};

/* returns success */
_Bool establish_connection(struct irc_conn* ic, char* host_name){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct hostent* h = gethostbyname(host_name);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(6660)},
                 local_addr = {.sin_family = AF_INET, .sin_port = 0};

    ic->sock = -1;
    /*ic->active = 1;*/
    ic->nick_set = 0;

    init_spool_t(&ic->msg_handlers, 10);

    local_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(sock, (struct sockaddr*)&local_addr, sizeof(struct sockaddr_in)) == -1){
        perror("bind");
        return 0;
    }

    memcpy(&addr.sin_addr, h->h_addr_list[0], h->h_length);

    puts(inet_ntoa(addr.sin_addr));
    if(connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))){
        perror("connect");
        return 0;
    }

    ic->sock = sock;
    ic->fp = fdopen(ic->sock, "rw");
    return 1;
}

void* msg_handler(void* v_msg){
    char* msg = v_msg;

    printf("msg: \"%s\"", msg);
    return NULL;
}

void* read_th(void* v_ic){
    struct irc_conn* ic = v_ic;
    char* ln = NULL;
    size_t sz;
    int llen;

    (void)sz;
    (void)llen;

    /*while(ic->active){*/
    while((llen = getline(&ln, &sz, ic->fp)) != -1){
        exec_routine(&ic->msg_handlers, msg_handler, ln, 0);
        ln = NULL;
        /* nick should be set by a separate function
         * this other function should be waiting on a
         * cond_t until we've received our first msg
         * 
         * this thread will ONLY receive input and handle
         * special cases, like ping AND signal our cond_t
         *
         * could have a cool system where each time
         * a message is received, we signal the cond_t
         * and increment an atomic counter
         *
         * then any time we need to wait on a msg, we can
         * just sleep until then easily
         *
         * after set_nick() returns, we can join_room()
         */
/*
 *         if(!ic->nick_set){
 * 
 *             ic->nick_set = 1;
 *         }
*/
    }

    return NULL;
}

pthread_t spawn_read_th(struct irc_conn* ic){
    pthread_t pth;
    pthread_create(&pth, NULL, read_th, (void*)ic);

    return pth;
}

/* returns a FILE* associated with sock */
/*
 * FILE* set_nick(int sock){
 *     
 * }
*/

#if 0
TODO: 
add a set of functionality that enables a list of startup commands that will be run in order,
once a connection has been established

once we have received bytes, send the following:
    USER malloc 0 * :malloc
    nick freeman

then, join a channel

have a separate thread to handle pongs
use spool

need a thread to read from sock constantly, optionally print,
handle pings

PING :JmqXrjDYIj
PONG JmqXrjDYIj     
#endif
int main(){
    struct irc_conn ic;
    int llen;
    char* ln = NULL;
    size_t sz;
    pthread_t read_pth;

    establish_connection(&ic, "irc.freenode.net");

    if(ic.sock == -1)return 0;

    read_pth = spawn_read_th(&ic);

    pthread_join(read_pth, NULL);
    /*usleep(4000000);*/
    /*
     * fprintf(fp, "USER Goo 0 * :Goo\n");
     * fprintf(fp, "NICK freeman\n");
    */

    /*
     * while((llen = getline(&ln, &sz, ic.fp)) != -1){
     *     if(ln[llen-1] == '\n')ln[llen-1] = 0;
     *     puts(ln);
     * }
    */
}
