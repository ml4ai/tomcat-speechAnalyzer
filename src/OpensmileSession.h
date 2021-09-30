#include <iostream>
#include <thread>
#include <vector>

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>

#include <mutex>

#include "JsonBuilder.h"
#include <smileapi/SMILEapi.h>

class OpensmileClient{
	public:
		OpensmileClient(int socket_port, JsonBuilder *builder);
		void send_chunk(std::vector<float> float_chunk);
		void send_eoi();
		void loop();
	private:
		JsonBuilder *builder;
		
		bool looping;
		std::thread loop_thread;

		int sock=0, valread;
		struct sockaddr_in serv_addr;
		int socket_port;
};

class OpensmileServer{
	public:
		OpensmileServer(int socket_port);

	private:
		smileobj_t* handle;
		std::thread opensmile_thread;

		int server_fd, new_socket, valread;
		struct sockaddr_in address;
		int opt = 1;
		int addrlen = sizeof(address);
		int socket_port;
};

class OpensmileSession{
	public:
		OpensmileSession(int socket_port, JsonBuilder *builder);

		void send_chunk(std::vector<float> float_chunk);
		void send_eoi();
	private:
		int mode = -1;
		OpensmileClient *client;
		OpensmileServer *server;	
};


