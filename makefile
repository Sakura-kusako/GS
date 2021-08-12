
all: clean client client_debug

client:
    gcc -o gs_client.exe gs_client_main.c gs_os_windows.c gs_socket_windows.c gs_app_protocol.c gs_console.c -O2 -Werror -Wall -lwsock32 -lws2_32 -DGS_PLATFORM_WINDOWS

server:
	gcc -o gs_server.exe gs_server_main.c gs_os_windows.c gs_socket_windows.c gs_app_protocol.c gs_console.c -O2 -Werror -Wall -lwsock32 -DGS_PLATFORM_WINDOWS

client_debug:
    gcc -o gs_client_bebug.exe gs_client_debug.c gs_os_windows.c gs_socket_windows.c gs_app_protocol.c gs_console.c -O2 -Werror -Wall -lwsock32 -lws2_32 -DGS_PLATFORM_WINDOWS

clean:
	
