#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int establish_connection(char* host_name){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    /*struct in_addr host_addr;*/
    struct hostent* h = gethostbyname(host_name);
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(6660)},
                 local_addr = {.sin_family = AF_INET, .sin_port = 0};// .sin_addr = INADDR_ANY};

    local_addr.sin_addr.s_addr = INADDR_ANY;
    /*
     * local_addr.sin_family = AF_INET;
     * local_addr.sin_port = 0;
     * local_addr.sin_addr = INADDR_ANY;
    */

    if(bind(sock, (struct sockaddr*)&local_addr, sizeof(struct sockaddr_in)) == -1)perror("bind");
    /*listen(sock, 0);*/

    memcpy(&addr.sin_addr, h->h_addr_list[0], h->h_length);

    puts(inet_ntoa(addr.sin_addr));
    if(connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))){
        perror("connect");
        return -1;
    }

    return sock;
}

int main(){
    int sock = establish_connection("irc.freenode.net"), llen;
    char* ln = NULL;
    size_t sz;
    FILE* fp;

    if(sock == -1)return 0;

    fp = fdopen(sock, "rw");

    while((llen = getline(&ln, &sz, fp)) != -1){
        if(ln[llen-1] == '\n')ln[llen-1] = 0;
        puts(ln);
    }
}
