#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PortNumber      8080
#define BuffSize        2048
#define MaxConnects     5
#define ConversationLen 256
#define Host            "127.0.0.1"
#define LogFile         "chat_history"

//Define client struct
typedef struct Client {
   int socket;
   char username[ConversationLen];
   struct Client *next;
} Client;

FILE *log_file;
Client *clients = NULL;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_shutdown = 0;

//Get timestamp
void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "[%Y-%m-%d %H:%M:%S]", tm_info);
}

// Log messages to file
void log_message(const char *message) {
    pthread_mutex_lock(&log_mutex);
    fprintf(log_file, "%s\n", message);
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);
}

//Broadcast message to all clients
void broadcast_message(const char *message) {
    pthread_mutex_lock(&clients_mutex);
    Client *client = clients;
    while (client) {
        write(client->socket, message, strlen(message));
        client = client->next;
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Notify clients of server shutdown
void notify_shutdown() {
    char shutdown_message[] = "Server is shutting down. Disconnecting...\n";
    pthread_mutex_lock(&clients_mutex);
    Client *client = clients;
    while (client) {
        write(client->socket, shutdown_message, strlen(shutdown_message));
        close(client->socket);
        Client *temp = client;
        client = client->next;
        free(temp);
    }
    clients = NULL;
    pthread_mutex_unlock(&clients_mutex);
}

//Remove client from list
void remove_client(Client *client) {
   pthread_mutex_lock(&clients_mutex);
   if (clients == client) {
      clients = clients->next;
   } else {
      Client *temp = clients;
      while (temp && temp->next != client) {
            temp = temp->next;
      }
      if (temp) {
         temp->next = client->next;
      }
   }
   pthread_mutex_unlock(&clients_mutex);
}

//Handle comms with client
void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[BuffSize];
    char timestamp[64];
    int bytes_read;

    // Receive username
    bytes_read = read(client->socket, client->username, sizeof(client->username) - 1);
    if (bytes_read <= 0) {
        close(client->socket);
        free(client);
        pthread_exit(NULL);
    }
    client->username[bytes_read] = '\0';

    // Notify other of new client
    get_timestamp(timestamp, sizeof(timestamp));
    char join_message[BuffSize];
    snprintf(join_message, sizeof(join_message), "%s %s has joined the chat.", timestamp, client->username);
    printf("%s\n", join_message);
    log_message(join_message);
    broadcast_message(join_message);

    // Chat Loop
    while ((bytes_read = read(client->socket, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        if (strcmp(buffer, "exit") == 0) break;

        // Send message with timestamp
        get_timestamp(timestamp, sizeof(timestamp));

        // Ensure message fits within the buffer
        char message[BuffSize];
        int len = snprintf(message, sizeof(message), "%s %s: %s", timestamp, client->username, buffer);

        if (len >= sizeof(message)) {
            // Truncate the message or handle error
            message[BuffSize - 1] = '\0';  // Ensure null-termination
        }

        printf("%s\n", message);
        log_message(message);
        broadcast_message(message);
    }

    // Notify others of disconnection
    get_timestamp(timestamp, sizeof(timestamp));
    snprintf(join_message, sizeof(join_message), "%s %s has left the chat.", timestamp, client->username);
    printf("%s\n", join_message);
    log_message(join_message);
    broadcast_message(join_message);

    // Cleanup
    close(client->socket);
    remove_client(client);
    free(client);
    pthread_exit(NULL);
}

void *server_command_listener(void *arg) {
    char command[BuffSize];
    while (1) {
        // Read input from the server's terminal
        if (fgets(command, sizeof(command), stdin) != NULL) {
            // Remove newline character
            command[strcspn(command, "\n")] = '\0';

            if (strcmp(command, "exit") == 0 || strcmp(command, "close") == 0) {
                printf("Shutting down server...\n");
                server_shutdown = 1;
                break;
            } else {
                printf("Unknown command: %s\n", command);
            }
        }
    }
    return NULL;
}

int main() {

  //Open log file
  log_file = fopen(LogFile, "w");
  if (!log_file) {
     perror("Unable to open log file");
     exit(EXIT_FAILURE);
  }

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
  saddr.sin_port = htons(PortNumber);           // Use the specified port

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

  // Start server command to close server
  pthread_t command_thread;
  if (pthread_create(&command_thread, NULL, server_command_listener, NULL) != 0) {
        perror("Failed to create command listener thread");
        close(server_socket);
        fclose(log_file);
        exit(EXIT_FAILURE);
    }

  //Accept clients
  fd_set read_fds;
  struct timeval timeout;

  while (!server_shutdown) {
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        timeout.tv_sec = 1; // 1-second timeout
        timeout.tv_usec = 0;

        int activity = select(server_socket + 1, &read_fds, NULL, NULL, &timeout);
        if (activity < 0) {
            perror("Select failed");
            break;
        } else if (activity == 0) {
            // Timeout, check for shutdown flag again
            continue;
        }

        if (FD_ISSET(server_socket, &read_fds)) {
            Client *client = malloc(sizeof(Client));
            if (!client) {
                perror("Failed to allocate memory for client");
                continue;
            }

        socklen_t addr_len = sizeof(struct sockaddr_in);
        client->socket = accept(server_socket, (struct sockaddr *)&saddr, &addr_len);
        if (client->socket < 0) {
            perror("Accept failed");
            free(client);
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        client->next = clients;
        clients = client;
        pthread_mutex_unlock(&clients_mutex);

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, (void *)client) != 0) {
            perror("Thread creation failed");
            close(client->socket);
            free(client);
        }
        pthread_detach(thread); // Auto-reclaim resources on thread exit
    }
  }
  // Wait for command listener to finish
  pthread_join(command_thread, NULL);

  // Shutdown procedure after "exit" command
  printf("Server is shutting down...\n");
  notify_shutdown();

  close(server_socket);
  fclose(log_file);
  return 0;
}
