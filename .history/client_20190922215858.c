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

// #define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
// #define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
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

    if (getaddrinfo(host, port, &addr_info, &addr_info_list) != 0) {
        printf("error from getaddrinfo\n");
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
                    // printf("connected!!\n");
                    break;
                }
            }
        } else { 
            printf("not IPv4\n");
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
                // printf("host: %s\n", optarg); 
                host = (char *) malloc((strlen(optarg) + 1) * sizeof(char));
                strcpy(host, optarg); 
                break; 
            case 'p':  
                // printf("port: %s\n", optarg);  
                port = (char *) malloc((strlen(optarg) + 1) * sizeof(char));
                strcpy(port, optarg);
                break; 
            case 'o':  
                // printf("operation: %s\n", optarg); 
                operation = atoi(optarg);
                break;  
            case 'k':  
                // printf("keyword: %s\n", optarg);  
                strcpy(keyword, optarg);
                break;
            default:
                printf("Error. Check the command line arguments\n");
        }  
    }
} 

uint16_t get_checksum(char* msg_buf, size_t length) {
  uint32_t sum = 0x0000;
  size_t i;
  // Add every 2 byte chunk
  for (i = 0; i + 1 <= length; i += 2) {
      uint16_t chunk;
      memcpy(&chunk, msg_buf + i, 2);
      sum += chunk;
      if (sum > 0xffff) {
          sum -= 0xffff;
      }
  }
  // If length is odd, add the left over chunk
  if (length % 2 == 0) {
      return (uint16_t) ~sum;
  } else {
      uint16_t chunk = 0;
      memcpy(&chunk, msg_buf + length - 1, 1);
      sum += chunk;
      if (sum > 0xffff) {
          sum -= 0xffff;
      }
      return (uint16_t) ~sum;
  }
};

uint16_t check_checksum(char* msg_buf, size_t length) {
  uint32_t sum = 0x0000;
  size_t i;
  // Add every 2 byte chunk
  for (i = 0; i + 1 < length; i += 2) {
      uint16_t chunk;
      memcpy(&chunk, msg_buf + i, 2);
      sum += chunk;
      if (sum > 0xffff) {
          sum -= 0xffff;
      }
  }
  // If length is odd, add the left over chunk
  if (length % 2 == 0) {
      return (uint16_t) sum;
  } else {
      uint16_t chunk = 0;
      memcpy(&chunk, msg_buf + length - 1, 1);
      sum += chunk;
      if (sum > 0xffff) {
          sum -= 0xffff;
      }
      return (uint16_t) sum;
  }
};

struct msg *pack_message(char *text) {
    struct msg *msg_out = (struct msg*) malloc(sizeof(struct msg));
    memset(msg_out, 0, sizeof(struct msg));
	msg_out->op = htons(operation); //convert short from host to network
    int text_len = strlen(text);
    strncpy(msg_out->keyword, keyword, 4);
    msg_out->length = htonll(text_len + (uint64_t)16); // 64 bit num in host byte order to network byte
    strncpy(msg_out->data, text, text_len);
    msg_out->checksum = get_checksum((char *) msg_out, text_len + 16);
    return msg_out;
}

ssize_t send_all(int socket_fd, char* msg_buf, size_t msg_length) {
    int i = 0;
	ssize_t size_acc = 0;
	while (msg_length > 0) {
        ssize_t sent_size = send(socket_fd, msg_buf + i, msg_length, 0);
		if (sent_size == -1) {
			printf("Error occured during sending.\n");
            continue;
		} else if (sent_size == 0) {
			printf("Connection lost.\n");
		} else {
            size_acc += sent_size;
            msg_length -= sent_size;
            i += sent_size;
        }
	}
	return size_acc;
}

int main(int argc, char *argv[]) {
    int socket_fd; 

    // parsing arguments
    parse_args(argc, argv);

    // setting up socket
    socket_fd = setup_socket();

    // keep getting stdin 
    char *stdInput = (char*) malloc((MAX_LEN - 16) * sizeof(char)); 
    char *result = (char*) malloc((MAX_LEN - 16) * sizeof(char));
    memset(stdInput, 0, sizeof(char));
    memset(result, 0, sizeof(char));
    while (fgets(stdInput, (MAX_LEN - 16) * sizeof(char), stdin) != NULL) {
        //pack message
        struct msg *msg_out = (struct msg*) malloc(sizeof(struct msg));
        memset(msg_out, 0, sizeof(struct msg));
        msg_out = pack_message(stdInput);

        // send message
        ssize_t sent_size = send_all(socket_fd, (char *) msg_out, ntohll(msg_out->length));

        //receiving message
        int size_acc = 0;
        char *buffer = (char *) malloc(MAX_LEN * sizeof(char));
        while (size_acc < ntohll(msg_out->length)) {
            size_acc += recv(socket_fd, buffer + size_acc, MAX_LEN - size_acc, 0);
        }
        if (size_acc == -1) {
            printf("Error occured during receiveng\n");
        } else if (size_acc == 0) {
            printf("Connection lost when receiving\n");
        } else {
            // printf("Message received\n");
        }

        // checking integrit of message
        uint64_t chunk;
        memcpy(&chunk, buffer + 8, 8);
        if (check_checksum(buffer, (int) ntohll(chunk)) != 0xffff) { //length of msg itself might be better
			printf("incorrect checksum\n");
		} else {
            // printf("checksum check passed\n");
        }

        //printing payload
        printf("%s", buffer + 16);
        memset(stdInput, 0, sizeof(char));
        memset(result, 0, sizeof(char));
        }

    close(socket_fd);
    return 0; 
}
