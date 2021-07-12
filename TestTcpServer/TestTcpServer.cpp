

#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h>

#include "TestTcpServer.h"

#define PORT 8888 

int main(int argc, char const *argv[]) 
{ 
    int server_fd, new_socket, valread; 
    struct sockaddr_in address; 
    int opt = 1; 
    int addrlen = sizeof(address); 
    char buffer[1024] = {0}; 
    const char rMsg[] = {"ok\n"}; 
       
    // Creating socket file descriptor 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
    { 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    } 
       
    // Forcefully attaching socket to the port  
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, 
                                                  &opt, sizeof(opt))) 
    { 
        perror("setsockopt"); 
        exit(EXIT_FAILURE); 
    } 
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons( PORT ); 
       
    // Forcefully attaching socket to the port  
    if (bind(server_fd, (struct sockaddr *)&address,  
                                 sizeof(address))<0) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
    if (listen(server_fd, 3) < 0) 
    { 
        perror("listen"); 
        exit(EXIT_FAILURE); 
    } 
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,  
                       (socklen_t*)&addrlen))<0) 
    { 
        perror("accept"); 
        exit(EXIT_FAILURE); 
    } 
    
    while(1) {
        char c;
        int idx = 0;
        memset(buffer, 0, sizeof(buffer));
        do {
    	    valread = read( new_socket , &c, 1); 
	    if(valread <= 0) {
		printf("+++ disconnected\n");
		return -1;
	    }
	  buffer[idx++] = c;
	} while (c != 0x0d && c != 0x0a && idx < sizeof(buffer)-1);
	 buffer[idx] = 0;

	printf("+++ got %d bytes\n", idx);
	for(int i=0; i<idx; i++) {
	   printf("%2.2x ", (unsigned char)buffer[i]);
	}
	printf(" - ");
	for(int i=0; i<idx; i++) {
	   printf("%c", buffer[i]);
	}

	printf("\n");

	::usleep(10000);

	if(strncmp(buffer, "M115", 4) == 0) {
	    printf("Processing M115\n");
	    sendM115(new_socket);
	}

	int rc = 0;
    	if((rc = send(new_socket , rMsg , sizeof(rMsg)-1, 0 )) < 0) { 
	   printf("+++ cannot send\n");
	   return -1;
	}
    	printf("rc: %d, sent data: %s\n", rc, rMsg); 
    }
    return 0; 
}

