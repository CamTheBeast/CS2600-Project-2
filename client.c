#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PortNumber 8080
#define ConversationLen 256
#define Host "127.0.0.1"   // Server address (localhost for testing)

void report(const char *message, int exit_code) {
   perror(message);     //print error message
   exit(exit_code);     //exit the program
}

int main() {

   //Create a socket to connect to the server
   int sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0) report ("socket", 1);

   //Get address of host
   struct hostent* htpr = gethostbyname(Host);
   if (!htpr) report("gethostbyname", 1);
   if (htpr->h_addrtype != AF_INET) report("bad address family", 1);

   //Set up server addr struct
   struct sockaddr_in saddr;
   memset(&saddr, 0, sizeof(saddr));
   saddr.sin_family = AF_INET;                          // Use IPv4
   saddr.sin_addr.s_addr = ((struct in_addr*) htpr->h_addr_list[0])->s_addr;
   saddr.sin_port = htons(PortNumber);

   //Connect to server
   if (connect(sockfd, (struct sockaddr*) &saddr, sizeof(saddr)) < 0) report("Connect", 1);
   printf("Connected to server at %s:%d\n", Host, PortNumber);

   //Talk with the server
   char buffer[ConversationLen];
   int bytes_read;

   while (1) {
        printf("Enter message: ");
        fgets(buffer, sizeof(buffer), stdin);  // Read input from stdin
        buffer[strcspn(buffer, "\n")] = '\0';  // Remove newline character

        if (strcmp(buffer, "exit") == 0) {
            break;                              //Exit Loop
        }

   //Send message to server
   write(sockfd, buffer, strlen(buffer));

   //Read message from server
   bytes_read = read(sockfd, buffer, sizeof(buffer));
       if (bytes_read < 0) {
           report("read", 1);
       }
       buffer[bytes_read] = '\0';

   //Display message
   printf("Server echoed: %s\n", buffer);
   }

   //Close connection
   close(sockfd);
   printf("Connection closed.\n");
   return 0;
}
