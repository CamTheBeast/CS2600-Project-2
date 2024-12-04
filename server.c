#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PortNumber      8080
#define BuffSize        1024
#define MaxConnects     5
#define ConversationLen 256
#define Host            "127.0.0.1"

//Define client struct
typedef struct Client {
   int socket;                  //client's socket
   struct sockaddr_in addr;     //client's addr info
   socklen_t addr_len;          //length of addr
} Client;

//Handles comms with client
void *handle_client(void *arg) {
   Client *client = (Client *)arg;
   int client_socket = client->socket;
   char buffer[ConversationLen];        //Holds messages
   int bytes_read;

   //Reads data until it stops receiving
   while ((bytes_read = read(client_socket, buffer, ConversationLen)) > 0) {
        buffer[bytes_read] = '\0';
        printf("Received from client (%s): %s\n", inet_ntoa(client->addr.sin_addr), buffer);
        write(client_socket, buffer, bytes_read);
   }

   //Close connection and free memory when finished
   close(client_socket);
   printf("Client disconnected: %s\n", inet_ntoa(client->addr.sin_addr));
   free(client);
   return NULL;
}

int main() {

  //Create socket for any new connections
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) {
     perror("Socket connection failed");
     exit(EXIT_FAILURE);
  }

  //Bind server address in memory
  struct sockaddr_in saddr;
  memset(&saddr, 0, sizeof(saddr));             // Clears the bytes
  saddr.sin_family = AF_INET;                   // Use IPv4
  saddr.sin_addr.s_addr = inet_addr(Host);      // Bind to the specified host
  saddr.sin_port = htons(PortNumber);                   // Use the specified port

  if (bind(server_socket, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
  }

  //Listen for connections, up to Maxconnects
  if (listen(server_socket, MaxConnects) < 0) { //Listens for clients, up to MaxConnects
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
  }

  printf("Server listening on %s:%d...\n", Host, PortNumber);

  //Allocate memory for new client connection
  while (1) {
        Client *client = malloc(sizeof(Client));
        if (!client) {
           perror("Failed to allocate memory for client");
           continue;
        }

  //Accept new incoming connection and fill the client structure
  client->addr_len = sizeof(client->addr);
  client->socket = accept(server_socket, (struct sockaddr *) &client->addr, &client->addr_len);
  if (client->socket < 0) {
        perror("Accept failed");
        free(client); //Free memory for the client if accept fails
        continue;
  }

  printf("Client connected: %s\n", inet_ntoa(client->addr.sin_addr));

  //Create a new thread to handle client comm's
  pthread_t thread_id;
  if (pthread_create(&thread_id, NULL, handle_client, (void *)client) < 0) {
        perror("Could not create thread");
        close(client->socket);
        free(client);
        }
  }

  close(server_socket);
  return 0;
}
