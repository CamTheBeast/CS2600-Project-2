# Compiler and flags
CC = gcc
CFLAGS = -Wall -pthread

# Server source files and output binary
SERVER_SRC = chat_server.c
SERVER_BIN = server

# Client source files and output binary
CLIENT_SRC = chat_client.c
CLIENT_BIN = client

# Default target: build nothing, make specific targets instead
.PHONY: all server client clean

# build only the server
$(SERVER_BIN): $(SERVER_SRC)
        $(CC) $(CFLAGS) -o $(SERVER_BIN) $(SERVER_SRC)

# build only the client
$(CLIENT_BIN): $(CLIENT_SRC)
        $(CC) $(CFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC)

# Clean rule to remove compiled files
clean:
        rm -f $(SERVER_BIN) $(CLIENT_BIN)

# Rebuild rule
rebuild: clean server client
