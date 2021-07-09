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
    pthread_mutex_t nick_set_lck, send_lck;

    pthread_t read_th;

    char* join_str;
    _Atomic _Bool nick_set, on_server;
    int wait_for_n;
    _Atomic int n_initial_msgs;
    /*_Atomic _Bool active;*/
    /*_Atomic _Bool in_channel;*/
    int sock;
    FILE* fp_in, * fp_out;
};

/* returns success */
_Bool establish_connection(struct irc_conn* ic, char* host_name, char* join_str){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct hostent* h = gethostbyname(host_name);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(6660)},
                 local_addr = {.sin_family = AF_INET, .sin_port = 0};

    /*ic->wait_for_n = 3;*/
    ic->wait_for_n = 2;
    ic->n_initial_msgs = 0;
    ic->sock = -1;
    /*ic->active = 1;*/
    ic->on_server = ic->nick_set = 0;
    ic->join_str = strdup(join_str);

    pthread_mutex_init(&ic->nick_set_lck, NULL);
    pthread_mutex_init(&ic->send_lck, NULL);

    /*pthread_cond_init(&ic->first_msg_recvd, NULL);*/
    init_spool_t(&ic->msg_handlers, 12);

    local_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(sock, (struct sockaddr*)&local_addr, sizeof(struct sockaddr_in)) == -1){
        perror("bind");
        return 0;
    }

    memcpy(&addr.sin_addr, h->h_addr_list[0], h->h_length);

    /*puts(inet_ntoa(addr.sin_addr));*/
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

    pthread_mutex_lock(&ic->send_lck);
    fflush(ic->fp_out);
    pthread_mutex_unlock(&ic->send_lck);
    return ret;
}

struct msg_arg{
    char* msg;
    struct irc_conn* ic;
};

_Bool set_nick(struct irc_conn* ic){
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

    /*puts("sending!");*/

    /*usleep(100000);*/
    ret &= send_irc(ic, "USER malloc 0 * :malloc\n");
    /*printf("ret: %i\n", ret);*/
    /*usleep(100000);*/
    ret &= send_irc(ic, "NICK MALLOC\n");
    /*printf("ret: %i\n", ret);*/
    /*usleep(100000);*/
    /*while(!ic->in_room){*/
        /*usleep(1000000);*/
        /*ret &= send_irc(ic, "JOIN #freenode\n");*/
    /*}*/
    /*
     * while(!ic->in_room){
     * usleep(100);
     * }
    */
    /*ret &= send_irc(ic, "JOIN #freenode\n");*/

    /*printf("ret: %i\n", ret);*/

    /*puts("sent!");*/
    /*pthread_mutex_destroy(&lck);*/

    return ret;
}


/* msg_handler will attempt to set nickname
 * with each new msg until nickname has been
 * succesfully set
 */
void* msg_handler(void* v_arg){
    struct msg_arg* arg = v_arg;

    /*printf("msg: \"%s\"", arg->msg);*/

    if(!arg->ic->nick_set){
    /*if(!arg->ic->in_room){*/
        pthread_mutex_lock(&arg->ic->nick_set_lck);

        if(++arg->ic->n_initial_msgs == arg->ic->wait_for_n){
            /*arg->ic->nick_set = set_nick(arg->ic);*/
            /*usleep(1000000);*/
            set_nick(arg->ic);
            arg->ic->nick_set = 1;
            /*if(!arg->ic->nick_set)arg->ic->n_initial_msgs = 0;*/
        }

        pthread_mutex_unlock(&arg->ic->nick_set_lck);
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
    if(*arg->msg == 'P'){
        char* c = strchr(arg->msg, ':')+1, buf[200] = {0};
        sprintf(buf, "PONG %s\n", c);
        send_irc(arg->ic, buf);
        puts("PONG");
    }

    if(strstr(arg->msg, "Welcome") || strstr(arg->msg, "WELCOME")){
        arg->ic->on_server = 1;
        /*puts("in_room = 1");*/
        /*send_irc(arg->ic, "JOIN #freenode\n");*/
        /*send_irc(arg->ic, "JOIN #ebooks\n");*/
        send_irc(arg->ic, arg->ic->join_str);
    }

    if(strstr(arg->msg, "DCC")){
        puts("handling dcc msg");
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in sender = {.sin_family = AF_INET};
        char* parse = strstr(arg->msg, "SEND"), * ip, * port, * len;

        if(!parse)goto CLEANUP;

        parse = strchr(parse, ' ');
        if(!parse)goto CLEANUP;
        parse = strchr(parse+1, ' ');
        if(!parse)goto CLEANUP;
        ip = parse+1;
        /*
         * printf("IP: %s\n", parse);
         * printf("IP: %s\n", parse+1);
        */
        parse = strchr(parse+1, ' ');
        if(!parse)goto CLEANUP;
        *parse = 0;
        port = parse+1;
        /*
         * printf("port: %s\n", parse);
         * printf("port: %s\n", parse+1);
        */
        parse = strchr(parse+1, ' ');
        if(!parse)goto CLEANUP;
        *parse = 0;
        len = parse+1;
        /*
         * printf("len: %s\n", parse);
         * printf("len: %s\n", parse+1);
        */
        printf("\"%s@%s\" \"%s\"", ip, port, len);
    }

    CLEANUP:

    free(arg);

    return NULL;
}

void* read_th(void* v_ic){
    struct irc_conn* ic = v_ic;
    struct msg_arg* ma;
    char* ln = NULL;
    size_t sz = 0;
    int llen;

    /*while(ic->active){*/
    /*while((llen = getline(&ln, &sz, ic->fp)) != -1){*/
    while((llen = getdelim(&ln, &sz, '\n', ic->fp_in)) != -1){
        ma = malloc(sizeof(struct msg_arg));
        ma->ic = ic;
        ma->msg = ln;
        exec_routine(&ic->msg_handlers, msg_handler, ma, 0);
        sz = 0;
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

struct irc_conn* irc_connect(char* server, char* room){
    struct irc_conn* ic = malloc(sizeof(struct irc_conn));
    char join_str[200] = {0};

    sprintf(join_str, "JOIN #%s\n", room);

    establish_connection(ic, server, join_str);
    if(ic->sock == -1){
        free(ic);
        return NULL;
    }

    ic->read_th = spawn_read_th(ic);

    return ic;
}

void await_irc(struct irc_conn* ic){
    pthread_join(ic->read_th, NULL);
}

/*
 * now i just need to figure out dcc, update msg handler to always accept DCC
 * dcc request comes in, interpret IP directly as unsigned long and port as unsigned short
 *
 * this should be done async, use spooler and have a function called dcc_recv
 * that creates a socket and recvs a file given the DCC string
 * 
 * exec_routine(_, dcc_recv, arg->msg) will be called from handler each time
 * strstr(dcc)
*/

int main(){
    struct irc_conn* ic = irc_connect("irc.irchighway.net", "ebooks");
    char* ln = NULL;
    size_t sz = 0;

    while(getline(&ln, &sz, stdin) != -1){
        send_irc(ic, ln);
    }

    await_irc(ic);
}

int smain(){
    struct irc_conn ic;
    char* ln = NULL;
    size_t sz = 0;
    pthread_t read_pth;

    /*establish_connection(&ic, "irc.freenode.net");*/
    establish_connection(&ic, "irc.irchighway.net", "ebooks");

    if(ic.sock == -1)return 0;

    read_pth = spawn_read_th(&ic);

    while(getline(&ln, &sz, stdin) != -1){
        send_irc(&ic, ln);
    }

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
    return 0;
}
