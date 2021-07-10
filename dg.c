#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>

#include "spool/sp.h"

#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_RESET   "\x1b[0m"

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

_Bool parse_dcc_str(char* msg, char** fn, unsigned long* ip, unsigned short* port, int* len){
    /*puts(msg);*/
    char* cursor = strrchr(msg, ' '), * st_cursor,
        * ip_str, * port_str, * len_str;

    if(!cursor)return 0;

    for(char* i = cursor+1; *i; ++i){
        if(!isdigit(*i)){
            *i = 0;
            break;
        }
    }

    *cursor = 0;
    len_str = cursor+1;


    cursor = strrchr(msg, ' ');

    if(!cursor)return 0;

    port_str = cursor+1;
    /**len = cursor+1;*/

    *cursor = 0;

    cursor = strrchr(msg, ' ');

    if(!cursor)return 0;

    ip_str = cursor+1;
    /**port = cursor-1;*/

    *cursor = 0;
    /**ip = cursor-1;*/

    /*printf("finding :DCC SEND in \"%s\"\n", msg);*/
    st_cursor = strchr(msg, '\001');
    if(!st_cursor)st_cursor = msg;
    else ++st_cursor;

    /*st_cursor = strstr(st_cursor, ":DCC SEND");*/
    st_cursor = strstr(st_cursor, "SEND");
    /*printf("found: \"%s\"\n", st_cursor);*/

    if(!st_cursor)return 0;

    /*st_cursor += 10;*/
    st_cursor += 5;

    *fn = st_cursor;

    #if !1
                                                                                                          |
    ":ub.156.185.IP PRIVMSG MALLOC :DCC SEND \"Albert Camus - Exile and the Kingdom (retail) (mobi).mobi\" 3114053547 34035 20280";
    #endif
    *len = atoi(len_str);
    *port = ntohs((unsigned short)atoi(port_str));
    *ip = ntohl(strtoul(ip_str, NULL, 10));
    /*printf("%s %li %i %i\n", *fn, *ip, *port, *len);*/

    return 1;
}

void handle_dcc(char* msg, struct irc_conn* ic){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int fsz;
    struct sockaddr_in sender = {.sin_family = AF_INET}, local;
    char* fn; 
    unsigned short port;
    unsigned long ip;

    /** port, * len, * fn;*/

    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = 0;

    if(bind(sock, (struct sockaddr*)&local, sizeof(struct sockaddr_in)))perror("dcc bind");

    if(!parse_dcc_str(msg, &fn, &ip, &port, &fsz))goto CLEANUP;

    puts("received DCC SEND offer, readying port");

    printf("file \"%s\" from \"%li@%i\" \"%i\"\n", fn, ip, port, fsz);
    fflush(stdout);

    char accept_str[200] = {0};
    sprintf(accept_str, "DCC ACCEPT %s %i 1\n", fn, port);
    /* TODO: THIS WILL BE USEFUL FOR DEBUGGING FILES WITH WHITESPACE */
    /*printf("accept string: %s\n", accept_str);*/
    send_irc(ic, accept_str);
    sender.sin_port = port;
    sender.sin_addr.s_addr = ip;

    if(connect(sock, (struct sockaddr*)&sender, sizeof(struct sockaddr_in)))perror("connect_dcc");

    /*FILE* sfp = fdopen(sock, "r");*/

    /* +1 for weirdness with extra byte */
    char* buf = malloc(fsz+1);
    /*fread(buf, 1, fsz, sfp);*/
    /* each time no more are sent, we need to send the total
     * number of bytes read in network order to get more
     */
    int b_read = 0, tmp;
    uint32_t n_int;
    /*while(b_read != fsz){*/
    while(1){
        /*while((tmp = read(sock, buf+b_read, 4))){*/
        /*
         * while((tmp = read(sock, buf+b_read, fsz))){
         *     b_read += tmp;
         *     printf("read %i/%s bytes\n", b_read, len);
         * }
        */

        tmp = read(sock, buf+b_read, fsz);
        b_read += tmp;
        printf("read %i/%i bytes\n", b_read, fsz);

        /*if(b_read == fsz)break;*/
        /* TODO: ??? */
        /* with `peapod` book provider, 1 extra byte is sometimes sent */
        if(b_read >= fsz)break;

        n_int = htonl(b_read);
        send(sock, &n_int, sizeof(uint32_t), 0);

        /*
         * while(fread(buf, 1, 1, sfp)){
         *     printf("read a char: %c\n", *buf);
         * }
        */
    }

    if(*fn == '"'){
        char* l_quote;

        if((l_quote = strrchr(fn+1, '"'))){
            ++fn;
            *l_quote = 0;
        }
    }

    FILE* fp = fopen(fn, "w");
    puts("writing to file");
    fwrite(buf, 1, fsz, fp);
    free(buf);
    fclose(fp);
    printf("wrote %i bytes to %s\n", fsz, fn);

    /*
     * for(int i = 0; i < fsz; ++i){
     *     printf("'%c',", buf[i]);
     * }
    */
    CLEANUP:
    close(sock);
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
        /*puts("PONG");*/
    }

    if(!arg->ic->on_server && (strstr(arg->msg, "Welcome") || strstr(arg->msg, "WELCOME"))){
        arg->ic->on_server = 1;
        /*puts("in_room = 1");*/
        /*send_irc(arg->ic, "JOIN #freenode\n");*/
        /*send_irc(arg->ic, "JOIN #ebooks\n");*/
        send_irc(arg->ic, arg->ic->join_str);
        printf("%sroom has been joined%s with command %s%s%s", ANSI_GREEN, ANSI_RESET, ANSI_RED, arg->ic->join_str, ANSI_RESET);

        fflush(stdout);
    }

    if(strstr(arg->msg, "DCC")){
        /*puts("handling dcc msg");*/
        /*printf("dcc msg: \"%s\"\n", arg->msg);*/
        handle_dcc(arg->msg, arg->ic);
    }

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

void dctest(){
    /*char wspc[] = ":Horla!Horla@ihw-lof.1ub.156.185.IP PRIVMSG MALLOC :DCC SEND \"Albert Camus - Exile and the Kingdom (retail) (mobi).mobi\" 3114053547 34035 20280";*/
    /*char wspc[] = ":Horla!Horla@ihw-lof.1ub.156.185.IP PRIVMSG MALLOC :DCC SEND \"Albert Camus - Exile and the Kingdom (retail) (mobi).mobi\" 3114053547 34035 20280";*/

    /*char nwspc[] = "DCC SEND txt.zip 2907702291\4611\13093";*/
    /*char nwspc[] = ":Search!Search@ihw-vu07ko.dyn.suddenlink.net PRIVMSG MALLOC :DCC SEND SearchBot_results_for__kafka.txt.zip 2907702291 4611 13093";*/

    /*char sss[] = ":Search!Search@ihw-vu07ko.dyn.suddenlink.net PRIVMSG MALLOC :DCC SEND SearchBot_results_for__kafka.txt.zip 22 3 2";*/
    char sss[] = ":Search!Search@ihw-vu07ko.dyn.suddenlink.net PRIVMSG MALLOC :DCC SEND SearchBot_results_for__kafka.txt.zip 2907702291 4431 12955";

    char* fn; 

    int len;
    unsigned long ip;
    unsigned short port;

    if(parse_dcc_str(sss, &fn, &ip, &port, &len))
        printf("reported: %s %li %i %i\n", fn, ip, port, len);
    else puts("failed");

    /*
     * parse_dcc_str(nwspc, &fn, &ip, &port, &len);
     * printf("reported: %s %li %i %i\n", fn, ip, port, len);
    */
}


int main(){
	/*dctest();*/
	/*exit(0);*/
    struct irc_conn* ic = irc_connect("irc.irchighway.net", "ebooks");
    char* ln = NULL;
    size_t sz = 0;

    while(getline(&ln, &sz, stdin) != -1){
        if(*ln == '.'){
            char buf[200] = {0};
            sprintf(buf, "PRIVMSG #ebooks :@search %s", ln+1);
            send_irc(ic, buf);
        }
        else send_irc(ic, ln);
    }

    await_irc(ic);
}
