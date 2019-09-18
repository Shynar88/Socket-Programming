#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <ctype.h>

#define MAX_LEN 10000000
char *host;
char *port;
int operation;
char keyword[5];

struct msg {
	uint16_t op;
	uint16_t checksum;
	char keyword[4];
	uint64_t length;
	char data[MAX_LEN - 16];
};

static int is_char(char c) {
	return (c >= 97 && c <= 122);
}

// encryption function
void encode(char *keyword, char *text, char *result){
    int i;
    int k_i = 0;

    for (i = 0; i < strlen(text); i++) { 
        int ch_lower = tolower(text[i]);
        if (is_char(ch_lower)) {
            int ch_new = ch_lower - 97 + keyword[k_i%4];
            if (ch_new > 122)  // wrapping around the alphabet
                memset(result + i, ch_new - 26, 1);
            else
                memset(result + i, ch_new, 1);
            k_i++;
        }
        else {  // not character
            memset(result+i, ch_lower, 1);
        }
    }
}

//decription function
void decode(char *keyword, char *text, char *result){
    int i;
    int k_i = 0;

    for (i = 0; i < strlen(text); i++) { 
        int ch_lower = tolower(text[i]);
        if (is_char(ch_lower)) {
            int ch_new = ch_lower + 97 - keyword[k_i%4];
            if (ch_new < 97)  // wrapping around the alphabet
                memset(result + i, ch_new + 26, 1);
            else
                memset(result + i, ch_new, 1);
            k_i++;
        }
        else {  // not character
            memset(result+i, ch_lower, 1);
        }
    }
}

// Setting up the socket
int setup_socket(){
    struct addrinfo *addr_info_list;
    struct addrinfo *el;
    struct addrinfo addr_info;
    int socket_fd;
    char ip[INET_ADDRSTRLEN];
    memset(&addr_info, 0, sizeof(struct addrinfo));

    addr_info.ai_socktype = SOCK_STREAM; // stream socket
    addr_info.ai_family = AF_INET;     // IPv4

    if (getaddrinfo("143.248.53.25", "7878", &addr_info, &addr_info_list) != 0) {
        printf("error from getaddrinfo");
    }

    //looping through the list
    for(el = addr_info_list; el != NULL; el = el->ai_next) {
        void *address;
        // get the pointer to the address itself,
        // get the fields
        if (el->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4_sockaddr = (struct sockaddr_in *)el->ai_addr;
            address = &(ipv4_sockaddr->sin_addr); //get the Internet address
            if ((socket_fd = socket(el->ai_family, el->ai_socktype, el->ai_protocol)) == -1) {
                continue;
            } else {
                if (connect(socket_fd, el->ai_addr, el->ai_addrlen) != -1) {
                    printf("connected!!");
                    break;
                }
            }
        } else { 
            printf("not IPv4");
        }
        // convert IP from network to presentation:
        inet_ntop(el->ai_family, address, ip, sizeof ip);
    }

    freeaddrinfo(addr_info_list); //cleaning up

    // return socket file descriptor
    return socket_fd;
}

void parse_args(int argc, char *argv[]) {
    int opt; 

    while((opt = getopt(argc, argv, "h:p:o:k:")) != -1) {
        switch(opt) {  
            case 'h':  
                printf("host: %s\n", optarg); 
                host = (char *) malloc((strlen(optarg) + 1) * sizeof(char));
                strcpy(host, optarg); 
                break; 
            case 'p':  
                printf("port: %s\n", optarg);  
                port = (char *) malloc((strlen(optarg) + 1) * sizeof(char));
                strcpy(port, optarg);
                break; 
            case 'o':  
                printf("operation: %s\n", optarg); 
                operation = atoi(optarg);
                break;  
            case 'k':  
                printf("keyword: %s\n", optarg);  
                strcpy(keyword, optarg);
                break;
            default:
                printf("Error. Check the command line arguments");
        }  
    }
} 

int main(int argc, char *argv[]) {
    int socket_fd; 

    // // parsing arguments
    // parse_args(argc, argv);

    // // setting up socket
    // socket_fd = setup_socket();

    // // keep getting stdin 
    // char *stdInput = malloc(MAX_LEN - 16);
    // while (fgets(stdInput, sizeof(stdInput), stdin) != NULL) {
    //     }

    // close(socket_fd);
    return 0; 
}
