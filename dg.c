#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>

int main(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct in_addr host_addr;
    struct hostent* h = gethostbyname("irc.freenode.net");

    memcpy(&host_addr, h->h_addr_list[0], h->h_length);
    /*host_addr = */
    puts(inet_ntoa(host_addr));
}
