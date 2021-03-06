#define _GNU_SOURCE

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

struct s_term{
    char* term;
    struct s_term* next;
};

struct s_term_l{
    struct s_term* first, * last;
    pthread_mutex_t s_term_lck;
};

struct s_term_l* init_s_term_l(){
    struct s_term_l* stl = malloc(sizeof(struct s_term_l));
    stl->first = stl->last = NULL;
    pthread_mutex_init(&stl->s_term_lck, NULL);

    return stl;
}

void insert_s_term_l(struct s_term_l* stl, char* term){
    pthread_mutex_lock(&stl->s_term_lck);
    if(!stl->last){
        stl->first = stl->last = malloc(sizeof(struct s_term));
        stl->first->next = NULL;
        stl->first->term = term;
    }
    pthread_mutex_unlock(&stl->s_term_lck);
}

char* pop_s_term_l(struct s_term_l* stl){
    char* ret = NULL;
    pthread_mutex_lock(&stl->s_term_lck);
    if(stl->first){
        ret = stl->first->term;
        stl->first = stl->first->next;
    }
    pthread_mutex_unlock(&stl->s_term_lck);

    return ret;
}

/*
 * compare each word in our term to the target as a whole
 * sum the matches
 * return sum
*/
/*write a test*/
/* must be called with lock acquired */
float st_confidence(struct s_term* st, char* target){
    char* term = st->term;
    char* cursor = term, * prev = term;
    int n_found = 0, n_subterms = 1;
    while((cursor = strchr(cursor, ' '))){
        ++n_subterms;
        *cursor = 0;
        if(strcasestr(target, prev))++n_found;
        *cursor = ' ';
        ++cursor;
        prev = cursor;
        /*if(deleter = strchr(cursor+1))*/
    }
    n_found += (_Bool)strcasestr(target, prev);

     /*
      * printf("%i %i\n", n_found, n_subterms);
      * printf("target %s with confidence %f\n", target, (float)n_found/(float)n_subterms);
     */

    return (float)n_found/(float)n_subterms;
}

struct irc_conn{
    struct spool_t msg_handlers;

    /*pthread_cond_t first_msg_recvd;*/
    pthread_mutex_t nick_set_lck, send_lck;

    pthread_t read_th;

    struct s_term_l* stl;

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

    ic->stl = init_s_term_l();

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

char* find_file_str(struct s_term_l* stl, FILE* option_lst){
    char* ln = NULL;
    size_t sz = 0;
    int len;

    float max_conf = 0, tmp;
    char* best_match = NULL;

    /* we must acquire lock before selecting a file */
    pthread_mutex_lock(&stl->s_term_lck);
    while((len = getline(&ln, &sz, option_lst)) != EOF){
        if(*ln != '!')continue;
        #if !1
        st_confidence is a float - matches/total_subterms - 1 is absolute best
        if 1, we can short circuit!
        hmm this code is weird... we need to be finding confidence for each ln
        BUT check each line against every term in the stl
        need another nested loop!
        nvm swap these loops, this should be outer bc we are selecting the proper ln
        need to see which line works best with a given 

            wtf... scrap it all hmm
            given a list of terms and a list of files
            need to find the one file that a term matches the best
    
            go thru all terms, each time there is a hit of a subterm, add up that term candidate
            whichever term has a bigger score 
            idk man

            once we select one, keep track of it in the corresponding s_term
            and mark that s_term as awaiting_file

            when a non-zip file comes in with the identical filename, we can perform an action with it
            such as emailing perhaps
            #endif

        for(struct s_term* st = stl->first; st; st = st->next){
            if((tmp = st_confidence(st, ln)) > max_conf){
                max_conf = tmp;
                if(best_match)free(best_match);
                best_match = ln;

                if(max_conf == 1.0)break;
            }
            /*else free(ln);*/
        }
        if(best_match == ln)ln = NULL;
    }
    pthread_mutex_unlock(&stl->s_term_lck);

    return best_match;
}

/* s_term and recipient can be coupled together on insertion */
/*void file_dl_handler(struct s_term_l* stl, char* fn){*/
void file_dl_handler(struct irc_conn* ic, char* fn){
    char* ext = strrchr(fn, '.');
    struct s_term_l* stl = ic->stl;

    if(!ext)return;

    if(!strcmp(ext+1, "zip")){
        char buf[500] = {0}, rawfn[200] = {0}, * find_fs;
        *ext = 0;
        /*sprintf(rawfn, "\".%s.raw\"", fn);*/
        sprintf(rawfn, ".%s.raw", fn);
        *ext = '.';
        sprintf(buf, "/usr/bin/unzip -p %s > %s", fn, rawfn);
        system(buf);
        /*printf("rawfn: %s\n", rawfn);*/
        /*printf("run cmd: |%s|\n", buf);*/
        /*usleep(400000);*/

        FILE* fp = fopen(rawfn, "r");
        
        /*while(!(fp = fopen(rawfn, "w")))usleep(1000);*/

        find_fs = find_file_str(stl, fp);
        if(find_fs){
            char fdl_cmd[1000] = {0};
            sprintf(fdl_cmd, "PRIVMSG #ebooks :%s\n", find_fs);

            printf("%sREQUESTING FILE: \"%s\"%s\n", ANSI_RED, find_fs, ANSI_RESET);
            fflush(stdout);
            /*printf("sending cmd: %s\n", fdl_cmd);*/
            send_irc(ic, fdl_cmd);
        }
        /*printf("using search term: %s\n", find_file_str(stl, fp));*/

        fclose(fp);
    }
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
        printf("\rread %i/%i bytes", b_read, fsz);
        fflush(stdout);

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
    /*puts("");*/

    if(*fn == '"'){
        char* l_quote;

        if((l_quote = strrchr(fn+1, '"'))){
            ++fn;
            *l_quote = 0;
        }
    }

    for(char* i = fn; *i; ++i){
        if(*i == ' ')*i = '_';
    }

    FILE* fp = fopen(fn, "w");
    puts("writing to file");
    fwrite(buf, 1, fsz, fp);
    free(buf);
    fflush(fp);
    fclose(fp);
    printf("wrote %i bytes to %s\n", fsz, fn);

    file_dl_handler(ic, fn);

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

void test_find_file_str(char* s_term, char* fn){
    FILE* fp = fopen(fn, "r");
    struct s_term_l* stl = init_s_term_l();
    insert_s_term_l(stl, strdup(s_term));

    printf("ret: %s\n", find_file_str(stl, fp));

    fclose(fp);
}

#if !1
search term must be recorded in order to search through them
immediately after a download of a zip file, we call a file_dl_handler()
which handles diff file types
for ".zip", look in struct for a matching term

maybe keep track of port, ip, sterm, 

need to link .search command with dcc download

we can just store the entire search term
then when we get a dcc file, is downloaded, we go through each line
and if any of th eelements of the search term separated by ' ' match a line
keep track of it, line_no, num_matches
once all match use that
otherwise use most matched
#endif

int main(int a, char** b){
    /*exit(0);*/
    (void)a;
    (void)b;
    /*
     * (void)a;
     * test_find_file_str(b[1], b[2]);
     * exit(0);
    */
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
            /*puts("sent term");*/
            insert_s_term_l(ic->stl, ln+1);
        }
        else send_irc(ic, ln);
    }

    await_irc(ic);
}
