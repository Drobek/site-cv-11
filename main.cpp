#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <stdlib.h>

#define BUFSIZE 1500
using namespace std;
int finish = 0;

/* Funkce checksum je opsaná z příslušných RFC dokumentů. */
unsigned short int checksum(unsigned short int *addr, unsigned count) {

    unsigned int sum = 0;
    while (count > 1) {
        sum += *(addr++);
        count -= 2;
    }

    if (count > 0) {
        sum += *(unsigned char *) addr;
    }

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return ~sum; // truncate to 16 bits
}

void sig_handler(int sig) {
    if (sig == SIGINT)
        finish = 1;
}

int main(int argc, char *argv[]) {
    hostent *host;

    struct sigaction act;
    struct icmphdr my_icmp_header, icmp_header;
    int sock;
    size_t size;
    unsigned int ttl = 255;
    struct iphdr *ip_header;
    struct timeval start, end, my_tv, timeout;
    struct sockaddr_in my_sockaddr_in, received_address;
    fd_set myset;
    char buffer[BUFSIZE];

    memset(&act, 0, sizeof (act));
    act.sa_handler = sig_handler;
    host = gethostbyname(argv[1]);
    sigaction(SIGINT, &act, NULL);

    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    /* vytvorime raw socket */

    //vytvorim icmp paket
    my_icmp_header.type = ICMP_ECHO;
    my_icmp_header.code = 0;
    my_icmp_header.un.echo.id = getpid();

    //naplneni sockaddr_in
    my_sockaddr_in.sin_family = AF_INET;
    my_sockaddr_in.sin_port = 0;
    memcpy(&my_sockaddr_in.sin_addr, host->h_addr, host->h_length);

    int count = 0;
    char* s, *chyba;
    s = argv[2];
    count = (int) strtol(s, &chyba, 10);

    for (int i = 0; i < count; i++) {
        my_icmp_header.checksum = 0;
        my_icmp_header.un.echo.sequence = i;
        int sequence = my_icmp_header.un.echo.sequence;

        sock = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);

        if (sock == -1) {
            perror("socket error");
            return -1;
        }

        setsockopt(sock, IPPROTO_IP, IP_TTL,
                (const char *) & ttl, sizeof (ttl));

        //Nastavte časový interval pro příjem odpovídající odpovědi na 5 sekund (pomocí volby socketu SO_RCVTIMEO)
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*) & timeout, sizeof (timeout));


        my_icmp_header.checksum = checksum((unsigned short *) & my_icmp_header, sizeof (my_icmp_header));
        //pri checksumu s vyplnenym checksumem ma vyjit 0
        if (checksum((unsigned short *) & my_icmp_header, sizeof (my_icmp_header)) != 0) {
            printf("OMFG, spatny checksum");
            return -1;
        }

        struct in_addr ping_adress;
        //        printf("My address: %s\n", inet_ntoa(ping_adress));

        gettimeofday(&start, NULL);
        sendto(sock, (char *) & my_icmp_header, sizeof (icmphdr), 0, (sockaddr *) & my_sockaddr_in, sizeof (sockaddr));
        //        cout << "ping to " << host->h_name << ": " << "ip: " << inet_ntoa(*((struct in_addr *)host->h_addr_list[0])) << endl;
        //paket odeslan, cekame na odpoved

        do {
            gettimeofday(&end, NULL);

            size = sizeof (sockaddr_in);

            if (recvfrom(sock, buffer, BUFSIZE, 0, (sockaddr *) & received_address, &size) == -1) {
                printf("recvfrom error, neboli timeout;\n");
                return -1;
            }

            //ted chytame ale vsechno, takze potreba vypsat jen kdyz to je nas hledany paket, popripade cekat dokud neprijde ten pravy

            ip_header = (iphdr *) buffer;
            icmp_header = *(struct icmphdr*) (buffer + sizeof (iphdr));

            cout << " bytes from " << host->h_name << " (" << inet_ntoa(*((struct in_addr *) host->h_addr_list[0])) << ") icmp_req=" << (i + 1) << " ttl=" << ip_header->ttl;

            //            cout << "typ (0 je fajn) : " << ntohs(icmp_header.type) << endl;
            //            cout << "kod (0 je fajn) : " << ntohs(icmp_header.code) << endl;
            //            cout << "id (" << getpid() << " je fajn)" << icmp_header.un.echo.id << endl;
            //            cout << "sequence (" << sequence << " je fajn)" << icmp_header.un.echo.sequence << endl << endl;
        } while (!((icmp_header.type == ICMP_ECHOREPLY)
                && (icmp_header.code == 0)
                && (icmp_header.un.echo.id == getpid())
                && (icmp_header.un.echo.sequence == sequence)));
        cout << " time=" << 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) << endl;

        close(sock);

    }
    /* zrusime socket */
    close(sock);

    return 0;
}