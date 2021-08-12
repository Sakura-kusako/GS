CC := gcc

SERVER_OUTPUT := gs_server.out
CLIENT_OUTPUT := gs_client.out
CLIENT_DEBUG_OUTPUT := gs_client_bebug.out

BASE_SRCS := gs_app_protocol.o gs_console.o gs_os_linux.o gs_socket_linux.o
SERVER_SRCS := gs_client_main.o
CLIENT_SRCS := gs_server_main.o
CLIENT_DEBUG_SRCS := gs_client_debug.o

CFLAGS := -O2 -Werror -Wall -Wno-unused-result -DGS_PLATFORM_LINUX -pthread

all: clean client client_debug server
	echo "\033[1;32m\n    make all success\n\033[0m"

client: $(BASE_SRCS) $(SERVER_SRCS)
	$(CC) $^ -o $(SERVER_OUTPUT) $(CFLAGS)

server: $(BASE_SRCS) $(CLIENT_SRCS)
	$(CC) $^ -o $(SERVER_OUTPUT) $(CFLAGS)

client_debug: $(BASE_SRCS) $(CLIENT_DEBUG_SRCS)
	$(CC) $^ -o $(CLIENT_DEBUG_OUTPUT) $(CFLAGS)

clean:
	rm -f *.o *.out

%.o:%.c
	$(CC) -c $< -o $@ $(CFLAGS)
