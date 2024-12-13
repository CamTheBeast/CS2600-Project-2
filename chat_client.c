#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#define PortNumber 8080
#define ConversationLen 256
#define Host "127.0.0.1"   // Server address (localhost for testing)

// Function to display error and exit
void report(const char *message, int exit_code) {
   perror(message);     //print error message
   exit(exit_code);     //exit the program
}

//Thread funnction to receieve messages from the server
void *receive_messages(void *socket_desc) {
    int sockfd = *(int *)socket_desc;
    char buffer[ConversationLen];
    int bytes_read;

    while ((bytes_read = read(sockfd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s\n", buffer);
    }

    printf("Server has disconnected. Exiting...\n");
    close(sockfd);
    exit(EXIT_SUCCESS);
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
   saddr.sin_family = AF_INET;
   saddr.sin_addr.s_addr = ((struct in_addr*) htpr->h_addr_list[0])->s_addr;
   saddr.sin_port = htons(PortNumber);

   //Connect to server
   if (connect(sockfd, (struct sockaddr*) &saddr, sizeof(saddr)) < 0) report("Connect", 1);
   printf("Connected to server at %s:%d\n", Host, PortNumber);

   //Enter username
   char username[ConversationLen];
   printf("Enter your username: ");
   fgets(username, sizeof(username), stdin);
   username[strcspn(username, "\n")] = '\0'; // Remove newline character
   write(sockfd, username, strlen(username));

   //Start thread to recieve messages
   pthread_t receive_thread;
   if (pthread_create(&receive_thread, NULL, receive_messages, (void *)&sockfd) != 0) {
        report("Failed to create receive thread", 1);
   }

   //Chat loop : send message
   char message[ConversationLen];
    while (1) {
        fgets(message, sizeof(message), stdin);  // Read input from the user
        message[strcspn(message, "\n")] = '\0';  // Remove newline character

        if (strcmp(message, "exit") == 0) {
            write(sockfd, message, strlen(message));
            break;
        }

        write(sockfd, message, strlen(message)); // Send the message to the server
    }

   //Close connection
   close(sockfd);
   printf("Disconnected from the server.\n");
   return 0;
}
