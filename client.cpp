#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#define str_ std::string
#define vec_int_ std::vector<int>
#define	map_fd_response_ std::map<int, str_>
#define LOCAL_HOST 2130706433 //127.0.0.1
#define debug(str) std::cout << str << std::endl

void	putError(str_ error_str)
{
	std::cerr << error_str << ": " << strerror(errno) << std::endl;
}

void	exitWithPutError(str_ error_str)
{
	putError(error_str);
	std::exit(EXIT_FAILURE);
}

int	x_socket(int domain, int type, int protocol)
{
	int	clnt_socket = socket(domain, type, protocol);
	if (clnt_socket == -1)
	{
		exitWithPutError("socket() failed");
	}
	return clnt_socket;
}

void	setSockaddr_in(int port, struct sockaddr_in *addr)
{
	memset(addr, 0, sizeof(*addr));
	addr->sin_addr.s_addr		= htonl(LOCAL_HOST);
	addr->sin_family			= AF_INET;
	addr->sin_port				= htons(port);
}

void	x_connect(int clnt_socket, struct sockaddr_in serv_addr)
{
	if (connect(clnt_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
	{
		exitWithPutError("connect() failed");
	}
}

int	createClntSocket(int serv_port)
{
	struct sockaddr_in serv_addr;

	int clnt_socket = x_socket(AF_INET, SOCK_STREAM, 0);
	setSockaddr_in(serv_port, &serv_addr);
	x_connect(clnt_socket, serv_addr);
	return clnt_socket;
}

#define BUFF_SIZE 10240

void	recvAll(int clnt_socket)
{
	char 	buff[BUFF_SIZE + 1];
	ssize_t len = 1;
	str_	recved_str;

	while (true)
	{
		len = recv(clnt_socket, buff, BUFF_SIZE, MSG_DONTWAIT);
		if (len == -1)
		{
			if (errno != EWOULDBLOCK)
				putError("recv() faile");
		}
		else
		{
			buff[len] = '\0';
			recved_str = buff;
			std::cout << recved_str << std::endl;
			break ;
		}
	}
}

void	sendAll(int clnt_socket, char *buff)
{
	ssize_t	len = 1;

	for (size_t	i = 0; len && i < BUFF_SIZE; i++)
	{
		len = send(clnt_socket, &buff[i], 1, 0);
		if (len == -1)
			exitWithPutError("send() faile");
	}
}

void	sendDevRandom(int clnt_socket)
{
	ssize_t len = 1;
	char	buff[BUFF_SIZE + 1];
	int 	fd  = open("/dev/random", O_RDONLY);
	if (fd == -1)
		exitWithPutError("open faile");
	while (len != 0)
	{
		len = read(fd, buff, BUFF_SIZE);
		if (len == -1)
			exitWithPutError("read() faile");
		sendAll(clnt_socket, buff);
	}
}

str_	makeRequestMessage()
{
	str_	request_message = "GET / HTTP/1.1\r\n";

	request_message += "Host: localhost:443\r\n";
	request_message += "Connection: keep-alive\r\n\r\n";
	//request_message += "Connection: close\r\n\r\n";
	return request_message;
}

void	IOLoop(int clnt_socket)
{
	str_	request_message = makeRequestMessage();
	ssize_t	len;

	// while(true)
	// {
	// 	//sendDevRandom(clnt_socket);
	// 	//inputed_str += "\r\n";
	// 	len = send(clnt_socket, request_message.c_str(), request_message.size(), 0);
	// 	if (len == -1)
	// 		putError("send() faile");
	// 	debug(len);
	// 	recvAll(clnt_socket);
		
	// }
	len = send(clnt_socket, request_message.c_str(), request_message.size(), MSG_DONTWAIT);
	if (len == -1)
		putError("send() faile");
	recvAll(clnt_socket);
	request_message = "HTTP/1.1\r\n\r\n";
	len = send(clnt_socket, request_message.c_str(), request_message.size(), MSG_DONTWAIT);
	if (len == -1)
		putError("send() faile");
	recvAll(clnt_socket);
	
}

int	main(int argc, char **argv)
{
	if (argc != 2)
		exitWithPutError("Only a argument");
	int	clnt_socket = createClntSocket(atoi(argv[1]));
	IOLoop(clnt_socket);
}

//c++ -Wall -Wextra -Werror client.cpp -o client && ./client 8080