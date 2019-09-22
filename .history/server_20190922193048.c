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
#include <signal.h>

// #define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
// #define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#define PENDING_CONNECTIONS_NUM 10
#define MAX_LEN 10000000
char *port;

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

void parse_args(int argc, char *argv[]) {
    int opt; 

    while((opt = getopt(argc, argv, "p:")) != -1) {
        switch(opt) {  
            case 'p':  
                printf("port: %s\n", optarg);  
                port = (char *) malloc((strlen(optarg) + 1) * sizeof(char));
                strcpy(port, optarg);
                break; 
            default:
                printf("Error. Check the command line arguments\n");
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

    if (getaddrinfo("127.0.0.1", port, &addr_info, &addr_info_list) != 0) {
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
                int flag = 1;
                // solving "Address already in use"
                setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof flag);
                // binding server to specific port
                if (bind(socket_fd, el->ai_addr, el->ai_addrlen) != -1) {
                    printf("bind successful!!\n");
                }
                if (listen(socket_fd, PENDING_CONNECTIONS_NUM) != -1) {
                    printf("listen successful!!\n");
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

uint16_t check_checksum(char* msg_buf, size_t length) {
  uint32_t sum = 0x0000;
  // Add every 2 byte chunk
  for (size_t i = 0; i + 1 < length; i += 2) {
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

//function for sending whole message
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

uint16_t get_checksum(char* msg_buf, size_t length) {
  uint32_t sum = 0x0000;
  // Add every 2 byte chunk
  for (size_t i = 0; i + 1 < length; i += 2) {
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

void syscall_handler(int s) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// for getting read of zombies
void attach_listener() {
    struct sigaction sig_act;
    sig_act.sa_handler = syscall_handler;
    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = 0;
    if (sigaction(SIGCHLD, &sig_act, NULL) == -1) {
        printf("Error from zombie killer");
        exit(1); 
    } 
}

int main(int argc, char *argv[]) {
    int socket_fd; 
    int client_socket_fd;
    struct sockaddr_storage client_sockaddr;
    socklen_t client_addr_size = sizeof(client_sockaddr);

    //for reaping zombies
    attach_listener();

    // parsing arguments
    parse_args(argc, argv);

    // setting up socket
    socket_fd = setup_socket();

    for(;;) {
        if ((client_socket_fd = accept(socket_fd, (struct sockaddr *) &client_sockaddr, &client_addr_size)) != -1) {
            // printf("accept successfull!");
        }
        int pid = fork();
        if (!pid) { // in the child    pid == 0
            close(socket_fd); // no need of listener for children

            //receiving message
            struct msg *msg_in = (struct msg*) malloc(sizeof(struct msg));
            memset(msg_in, 0, sizeof(struct msg));
            int size_received = 0;
            while ((size_received = recv(client_socket_fd, msg_in, sizeof(struct msg), 0)) > 0) {
                if (size_received == -1) {
                    printf("Error occured during receiveng\n");
                } else if (size_received == 0) {
                    printf("Connection lost when receiving\n");
                } else {
                    printf("Message received\n");
                }

                // checking integrity of message
                if (check_checksum((char *) msg_in, (int) ntohll(msg_in->length)) != 0xffff) { 
                    if ((int) ntohll(msg_in->length)) == 10000000) {
                        printf("10MB");
                    }
                    printf("incorrect checksum\n");
                    break;
                } else {
                    printf("checksum check passed\n");
                }

                //packing message
                struct msg *msg_out = (struct msg*) malloc(sizeof(struct msg));
                memset(msg_out, 0, sizeof(struct msg));
                msg_out->op = msg_in->op; 
                msg_out->length = msg_in->length; 
                strncpy(msg_out->keyword, msg_in->keyword, 4);
                if (ntohs(msg_in->op)) {
                    decode(msg_in->keyword, msg_in->data, msg_out->data);
                    printf("result of decoding %s", msg_out->data);
                } else {
                    encode(msg_in->keyword, msg_in->data, msg_out->data);
                    printf("result of encoding %s", msg_out->data);
                }
                msg_out->checksum = get_checksum((char *) msg_out, (int) (ntohll(msg_in->length)));

                //send message to client
                send_all(client_socket_fd, (char *) msg_out, ntohll(msg_out->length));

                //reset variables
                memset(msg_out, 0, sizeof(struct msg));
                memset(msg_in, 0, sizeof(struct msg));
            }
            close(client_socket_fd);
            free(msg_in);
            exit(0); 
        } else if (pid == -1) {
            printf("fork fail");
            continue;
        } else { // in the parent
            // wait(NULL); //for reaping zombie children
            while (waitpid(-1, NULL, WNOHANG) > 0);
            close(client_socket_fd);  // parent doesn't need this
        }
    }

    close(socket_fd);
    return 0; 
}