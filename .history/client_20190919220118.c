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
                    printf("connected!!\n");
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
                printf("Error. Check the command line arguments\n");
        }  
    }
} 

uint16_t test_checksum(char* vdata,size_t length) {
    
  //Initialise the accumulator.
  uint32_t acc=0x0000;

  // Handle complete 16-bit blocks.
  for (size_t i=0;i+1<length;i+=2) {
      uint16_t word;
      memcpy(&word, vdata+i,2);
      acc+=word;
      if (acc>0xffff) {
          acc-=0xffff;
      }
  }

  // Handle any partial block at the end of the data.
  if (length&1) {
      uint16_t word=0;
      memcpy(&word,vdata+length-1,1);
      acc+=word;
      if (acc>0xffff) {
          acc-=0xffff;
      }
  }

  return (uint16_t)~acc;
};

uint16_t get_checksum(char* msg_buf, size_t length) {
  uint32_t sum = 0x0000;
  // Add every 2 byte chunk
  for (size_t i = 0; i + 1 <= length; i += 2) {
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
  // Add every 2 byte chunk
  for (size_t i = 0; i + 1 <= length; i += 2) {
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
      return (uint16_t) sum;
  }
};

struct msg *pack_message(char *text) {
    struct msg *msg_out = (struct msg*) malloc(sizeof(struct msg));
    memset(msg_out, 0, sizeof(struct msg));
	msg_out->op = htons(operation); //convert short from host to network
    int text_len = strlen(text) - 1;
    strncpy(msg_out->keyword, keyword, 4);
    msg_out->length = htonll(text_len + (uint64_t)16); // 64 bit num in host byte order to network byte
    strncpy(msg_out->data, text, text_len);
    msg_out->checksum = get_checksum((char *) msg_out, text_len + 16);
    uint16_t checksum_test = test_checksum((char *) msg_out, text_len + 16);
    printf("my function: %u\n", (unsigned int) msg_out->checksum);
    printf("their function: %u\n", (unsigned int) checksum_test);
    return msg_out;
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
        //packing message
        struct msg *msg_out = (struct msg*) malloc(sizeof(struct msg));
        memset(msg_out, 0, sizeof(struct msg));
        msg_out = pack_message(stdInput);

        // // send message
        // ssize_t sent_size = send(socket_fd, (char *) msg_out, ntohll(msg_out->length), 0);
        // if (sent_size == -1) {
        //     printf("Error occured during sending\n");
        // } else if (sent_size == 0) {
        //     printf("Connection lost\n");
        // } else {
        //     printf("Message sent\n");
        // }

        // //receiving message
        // char *buffer = (char *) malloc(MAX_LEN * sizeof(char));
        // uint64_t received_size = recv(socket_fd, buffer, MAX_LEN + 16, 0);
        // if (received_size < 0) {
        //     printf("Error occured during receiveng\n");
        // } else if (received_size == 0) {
        //     printf("Connection lost when receiving\n");
        // } else {
        //     printf("Message received\n");
        // }

        // //validating checksum
        // if (check_checksum(buffer, (int) ntohll(received_size)) != 0xffff) { 
		// 	printf("incorrect checksum\n");
		// } else {
        //     printf("checksum check passed\n");
        // }

        // //print the received text from server
        // printf("%s\n", buffer + 16);
        }

    close(socket_fd);
    return 0; 
}
