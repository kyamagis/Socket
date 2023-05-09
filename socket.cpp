
#include <iostream>
#include <string>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
 #include <fcntl.h>

#define str_ std::string
#define vec_int_ std::vector<int>
#define LOCAL_HOST 2130706433 //127.0.0.1
#define MAX_QUEQUE 5

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
	int	serv_socket = socket(domain, type, protocol);
	if (serv_socket == -1)
	{
		exitWithPutError("socket() failed");
	}
	return serv_socket;
}

void	x_close(int serv_socket)
{
	int error_flg = close(serv_socket);
	if (error_flg == -1)
		exitWithPutError("close() failed");
}

int	x_bind(int serv_socket, struct sockaddr_in addr, socklen_t addr_len)
{
	if (bind(serv_socket, (struct sockaddr *)&addr, addr_len) == -1)
	{
		x_close(serv_socket);
		return -1;
	}
	return 0;
}

void	x_listen(int serv_socket, int backlog)
{
	if (listen(serv_socket, backlog) == -1)
	{
		exitWithPutError("listen() failed");
	}
}

void	setSockaddr_in(int port, struct sockaddr_in *addr)
{
	memset(addr, 0, sizeof(*addr));
	addr->sin_addr.s_addr		= htonl(LOCAL_HOST);
	addr->sin_family			= AF_INET;
	addr->sin_port				= htons(port);
}

void	x_setsockopt(int serv_socket)
{
	int	option_value = 1;

	if (setsockopt(serv_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&option_value, sizeof(option_value)) == -1)
	{
		exitWithPutError("setsockopt() failed");
	}
}

int	createServSocket(int port)
{
	int	serv_socket = x_socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in	addr;

	setSockaddr_in(port, &addr);
	x_setsockopt(serv_socket);
	int	error_flg = x_bind(serv_socket, addr, sizeof(addr));
	if (error_flg == -1)
	{
		return -1;
	}
	fcntl(serv_socket, F_SETFL, O_NONBLOCK);
	x_listen(serv_socket, MAX_QUEQUE);
	return serv_socket;
}

vec_int_	createVecServSocket(int num_of_ports, char **argv)
{
	vec_int_	vec_serv_socket;
	int			serv_socket;

	for (int i = 0; i < num_of_ports; i++)
	{
		serv_socket = createServSocket(atoi(argv[i]));
		std::cout << "port: " << atoi(argv[i]);
		if (serv_socket == -1)
		{
			std::cout << ", bind() failed: " << strerror(errno) << std::endl;
		}
		else if (serv_socket != -1)
		{
			vec_serv_socket.push_back(serv_socket);
			std::cout << ", fd: " << serv_socket << std::endl;
		}
	}
	return vec_serv_socket;
}

int initMasterReadfds(vec_int_ vec_serv_socket, fd_set *master_readfds)
{
	int	max_descripotor = 0;

	FD_ZERO(master_readfds);
	for (size_t i = 0; i < vec_serv_socket.size(); i++)
	{
		FD_SET(vec_serv_socket[i], master_readfds);
		if (max_descripotor < vec_serv_socket[i])
			max_descripotor = vec_serv_socket[i];
	}
	return max_descripotor;
}

bool	containsListeningSocket(int fd, vec_int_ vec_serv_socket)
{
	for (size_t i = 0; i < vec_serv_socket.size(); i++)
		if (vec_serv_socket[i] == fd)
			return true;
	return false;
}

void	createClntSocket(int sorv_socket, fd_set *master_readfds, int *max_descripotor)
{
	int	clnt_socket;
	while(true)
	{
		clnt_socket = accept(sorv_socket, NULL, NULL);
		if (clnt_socket == -1)
		{
			if (errno != EWOULDBLOCK)
			{
				exitWithPutError("accept() failed");
			}
			return ;
		}
		FD_SET(clnt_socket, master_readfds);
		if (*max_descripotor < clnt_socket)
			*max_descripotor = clnt_socket;
	}
}

void	sendResponse(int clnt_socket, fd_set *master_writefds, str_ &entity_body)
{
	str_	request_message = "HTTP/1.1 200 OK\r\n";
	request_message += "Connection: close\r\n";
	request_message += "Content-Type: text/plane\r\n";
	request_message += "Content-Length: " + std::to_string(entity_body.size()) + "\r\n\r\n";
	request_message += entity_body;
	ssize_t	sent_len = send(clnt_socket, request_message.c_str(), request_message.size(), MSG_DONTWAIT);
	if (sent_len == -1)
	{
		if (errno != EWOULDBLOCK)
		{
			exitWithPutError("send() failed");
		}
	}
	FD_CLR(clnt_socket, master_writefds);
	x_close(clnt_socket);
	entity_body.clear();
}

void	recvRequest(int clnt_socket, fd_set *master_readfds, fd_set *master_writefds, str_ &entity_body)
{
	ssize_t recved_len = 1;
	char	buffer[1024];

	memset(buffer, '\0', sizeof(buffer));
	recved_len = recv(clnt_socket, buffer, sizeof(buffer), MSG_DONTWAIT);
	if (recved_len == -1)
	{
		if (errno != EWOULDBLOCK)
		{
			exitWithPutError("accept() failed");
		}
		return ;
	}
	if (buffer[1023] == '\0')
	{
		FD_CLR(clnt_socket, master_readfds);
		FD_SET(clnt_socket, master_writefds);
		buffer[recved_len] = '\0';
	}
	entity_body += buffer;
}

void	IOMultiplexingLoop(vec_int_ vec_serv_socket)
{
	fd_set	master_readfds;
	fd_set	master_writefds;
	fd_set	readfds;
	fd_set	writefds;
	int		max_descripotor = initMasterReadfds(vec_serv_socket, &master_readfds);
	int		ready;
	str_	entity_body;
	struct timeval	timeout;

	FD_ZERO(&master_writefds);
	while(true)
	{
		memcpy(&writefds, &master_writefds, sizeof(master_writefds));
		memcpy(&readfds, &master_readfds, sizeof(master_readfds));
		timeout.tv_sec  = 0;
		timeout.tv_usec = 0;
		ready = select(max_descripotor + 1, &readfds, &writefds, NULL, &timeout);
		if (ready == 0)
		{
			continue ;
		}
		else if (ready == -1)
			exitWithPutError("select() failed");
		else
		{
			for (int fd = 0; fd < max_descripotor + 1 && 0 < ready; fd++)
			{
				if (FD_ISSET(fd, &writefds))
				{
					--ready;
					sendResponse(fd, &master_writefds, entity_body);
				}
				else if (FD_ISSET(fd, &readfds))
				{
					--ready;
					if (containsListeningSocket(fd, vec_serv_socket))
					{
						createClntSocket(fd, &master_readfds, &max_descripotor);
					}
					else
					{
						std::cout << "clnt_socket: " <<  fd << ", max_descripotor: " << max_descripotor << std::endl;
						recvRequest(fd, &master_readfds, &master_writefds, entity_body);
					}
				}
			}
		}
	}
}

int	main(int argc, char **argv)
{
	if (argc < 2)
		exitWithPutError("Too few argument");

	int			num_of_ports = argc - 1;
	vec_int_	vec_serv_socket = createVecServSocket(num_of_ports, &argv[1]);
	std::cout << "サーバー起動!!" << std::endl;
	signal(SIGPIPE, SIG_IGN);
	IOMultiplexingLoop(vec_serv_socket);
}

// c++ -Wall -Wextra -Werror socket.cpp && ./a.out 8080 8081 8082
// http://localhost:8080