#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>

#include "spool/sp.h"

struct irc_conn{
    struct spool_t msg_handlers;

    /*pthread_cond_t first_msg_recvd;*/
    pthread_mutex_t nick_set_lck;

    _Atomic _Bool nick_set;
    int wait_for_n;
    _Atomic int n_initial_msgs;
    /*_Atomic _Bool active;*/
    /*_Atomic _Bool in_channel;*/
    int sock;
    FILE* fp_in, * fp_out;
};

/* returns success */
_Bool establish_connection(struct irc_conn* ic, char* host_name){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct hostent* h = gethostbyname(host_name);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(6660)},
                 local_addr = {.sin_family = AF_INET, .sin_port = 0};

    ic->wait_for_n = 3;
    ic->n_initial_msgs = 0;
    ic->sock = -1;
    /*ic->active = 1;*/
    ic->nick_set = 0;
    pthread_mutex_init(&ic->nick_set_lck, NULL);

    /*pthread_cond_init(&ic->first_msg_recvd, NULL);*/
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
    ic->fp_out = fdopen(ic->sock, "w");
    ic->fp_in = fdopen(ic->sock, "r");
    return 1;
}

_Bool send_irc(struct irc_conn* ic, char* msg){
    _Bool ret = fprintf(ic->fp_out, "%s", msg) == (int)strlen(msg);
    fflush(ic->fp_out);
    return ret;
}

struct msg_arg{
    char* msg;
    struct irc_conn* ic;
};

int set_nick(struct irc_conn* ic){
    int ret = 1;
/*
 *     puts("set_nick()");
 *     pthread_mutex_t lck;
 *     pthread_mutex_init(&lck, NULL);
 *     pthread_mutex_lock(&lck);
 * 
 *     puts("waiting :)");
 *     while(ic->n_initial_msgs != ic->wait_for_n){
 *         pthread_cond_wait(&ic->first_msg_recvd, &lck);
 *         pthread_mutex_lock(&lck);
 *         puts("OOPS");
 *     }
 *     [>usleep(4000000);<]
*/

    puts("sending!");

    ret &= send_irc(ic, "USER malloc 0 * :malloc\n");
    printf("ret: %i\n", ret);
    /*usleep(100000);*/
    ret &= send_irc(ic, "NICK freeman\n");
    printf("ret: %i\n", ret);
    /*usleep(100000);*/
    ret &= send_irc(ic, "JOIN #freenode\n");
    printf("ret: %i\n", ret);

    puts("sent!");
    /*pthread_mutex_destroy(&lck);*/

    return ret;
}


/* msg_handler will attempt to set nickname
 * with each new msg until nickname has been
 * succesfully set
 */
void* msg_handler(void* v_arg){
    struct msg_arg* arg = v_arg;

    printf("msg: \"%s\"", arg->msg);
    if(!arg->ic->nick_set){
        puts("acq lock");
        pthread_mutex_lock(&arg->ic->nick_set_lck);

        if(++arg->ic->n_initial_msgs == arg->ic->wait_for_n){
            arg->ic->nick_set = 1;
            arg->ic->nick_set = set_nick(arg->ic);
            arg->ic->nick_set = 1;
            /*if(!arg->ic->nick_set)arg->ic->n_initial_msgs = 0;*/
        }

        pthread_mutex_unlock(&arg->ic->nick_set_lck);
        puts("gave up");
        /* TODO: try atomic_compare_exchange_strong() method again with separate file pointers */
        /*arg->ic->wait_for_n;*/
        /* if we're one away from our target, wake up nick setter */
        /*
         * if(atomic_compare_exchange_strong(&arg->ic->n_initial_msgs, &arg->ic->wait_for_n-1, arg->ic->wait_for_n)){
         *     printf("signalling after %i messages\n", arg->ic->wait_for_n);
         *     pthread_cond_signal(&arg->ic->first_msg_recvd);
         * }
         * else ++arg->ic->n_initial_msgs;
        */
        /*arg->ic->nick_set = 1;*/
    }
    /*printf("msg: \"%s\"", arg->msg);*/

    free(arg);

    return NULL;
}

void* read_th(void* v_ic){
    struct irc_conn* ic = v_ic;
    struct msg_arg* ma;
    char* ln = NULL;
    size_t sz;
    int llen;

    /*while(ic->active){*/
    /*while((llen = getline(&ln, &sz, ic->fp)) != -1){*/
    while((llen = getdelim(&ln, &sz, '\r', ic->fp_in)) != -1){
        ma = malloc(sizeof(struct msg_arg));
        ma->ic = ic;
        ma->msg = ln;
        exec_routine(&ic->msg_handlers, msg_handler, ma, 0);
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
    pthread_t read_pth;

    establish_connection(&ic, "irc.freenode.net");

    if(ic.sock == -1)return 0;

    read_pth = spawn_read_th(&ic);

/*
 *     while(!set_nick(&ic)){
 *         puts("failed to send nick");
 *     }
 * 
 *     ic.nick_set = 1;
 *     puts("succesfully set nick");
*/
    /*
     * printf("sn: %i\n", set_nick(&ic));
     * ic.nick_set = 1;
    */

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
