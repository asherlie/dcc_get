#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>

int establish_connection(char* host_name){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    /*struct in_addr host_addr;*/
    struct hostent* h = gethostbyname("irc.freenode.net");
    struct sockaddr_in addr;

    memcpy(&addr.sin_addr, h->h_addr_list[0], h->h_length);

    /*host_addr = */
    puts(inet_ntoa(addr.sin_addr));
    if(connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))){
        perror("connect");
        return -1;
    }

    return sock;
}

int main(){
}
