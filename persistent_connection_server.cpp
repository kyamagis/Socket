
#include <iostream>
#include <string>
#include <vector>
#include <map>

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
#define	map_fd_data_ std::map<int, t_data>
#define map_ite_ std::map<int, t_data>::iterator
#define LOCAL_HOST 2130706433 //127.0.0.1
#define debug(str) std::cout << str << std::endl

typedef struct s_data
{
	str_	response_message;
}	t_data;

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
		putError("socket() failed");
		return -1;
	}
	return serv_socket;
}

void	x_close(int serv_socket)
{
	int error_flg = close(serv_socket);
	if (error_flg == -1)
		exitWithPutError("close() failed");
}

void	setSockaddr_in(int port, struct sockaddr_in *addr)
{
	memset(addr, 0, sizeof(*addr));
	addr->sin_addr.s_addr		= htonl(LOCAL_HOST);
	addr->sin_family			= AF_INET;
	addr->sin_port				= htons(port);
}

int	x_setsockopt(int serv_socket, int level, int optname)
{
	int	option_value = 1;

	if (setsockopt(serv_socket, level, optname, (const char *)&option_value, sizeof(option_value)) == -1)
	{
		putError("setsockopt() failed");
		x_close(serv_socket);
		return -1;
	}
	return 0;
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

int	x_fcntl(int fd, int cmd, int flg)
{
	if (fcntl(fd, cmd, flg) == -1)
	{
		putError("fcntl() failed");
		x_close(fd);
		return -1;
	}
	return 0;
}

int	x_listen(int serv_socket, int backlog)
{
	if (listen(serv_socket, backlog) == -1)
	{
		putError("listen() failed");
		x_close(serv_socket);
		return -1;
	}
	return 0;
}

int	createServSocket(int port)
{
	struct sockaddr_in	serv_addr;

	int serv_socket = x_socket(AF_INET, SOCK_STREAM, 0);
	if (serv_socket == -1)
		return -1;
	setSockaddr_in(port, &serv_addr);
	int	error_flg = x_setsockopt(serv_socket, SOL_SOCKET, SO_REUSEADDR);
	if (error_flg == -1)
		return -1;
	error_flg = x_bind(serv_socket, serv_addr, sizeof(serv_addr));
	if (error_flg == -1)
		return -1;
	error_flg = x_fcntl(serv_socket, F_SETFL, O_NONBLOCK);
	if (error_flg == -1)
		return -1;
	error_flg = x_listen(serv_socket, SOMAXCONN);
	if (error_flg == -1)
		return -1;
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

#define RESPONSE_MESSAGE fd_data[clnt_socket].response_message

void	sendResponse(int clnt_socket, fd_set *master_readfds, fd_set *master_writefds, int *max_descripotor, map_fd_data_ &fd_data)
{
	ssize_t	sent_len = send(clnt_socket, RESPONSE_MESSAGE.c_str(), RESPONSE_MESSAGE.size(), MSG_DONTWAIT);

	if (sent_len == -1)
	{
		debug("recv == -1");
	}
	if (sent_len == -1 && errno != EWOULDBLOCK)
	{
		exitWithPutError("send() failed");
	}
	if (sent_len < 1 || RESPONSE_MESSAGE.size() == (size_t)sent_len)
	{
		FD_CLR(clnt_socket, master_writefds);
		if (clnt_socket == *max_descripotor)
		{	
			while (!FD_ISSET(*max_descripotor, master_readfds) && !FD_ISSET(*max_descripotor, master_writefds))
				*max_descripotor -= 1;
		}
		x_close(clnt_socket);
		RESPONSE_MESSAGE.erase();
	}
	else if ((size_t)sent_len < RESPONSE_MESSAGE.size())
	{
		RESPONSE_MESSAGE = RESPONSE_MESSAGE.substr(0, sent_len);
	}
}

str_	makeResponseMessage(str_ &entity_body)
{
	str_	response_message = "HTTP/1.1 200 OK\r\n";
	response_message += "Connection: close\r\n";
	response_message += "Content-Type: text/plane\r\n";
	response_message += "Content-Length: " + std::to_string(entity_body.size()) + "\r\n\r\n";
	response_message += entity_body;

	return response_message;
}

#define BUFF_SIZE 100000

bool	recvRequest(int clnt_socket, char *buffer)
{
	ssize_t recved_len = 1;

	memset(buffer, '\0', BUFF_SIZE + 1);
	recved_len = recv(clnt_socket, buffer, BUFF_SIZE, MSG_DONTWAIT);
	if (recved_len == -1)
	{
		if (errno != EWOULDBLOCK)
		{
			exitWithPutError("accept() failed");
		}
		return false;
	}
	debug("recv == -1");
	return true;
}

void	storeRequestToMap(int clnt_socket, fd_set *master_readfds, fd_set *master_writefds, map_fd_data_ &fd_data)
{
	char	buffer[BUFF_SIZE + 1];

	if (!recvRequest(clnt_socket, buffer))
		return ;

	map_ite_	ite = fd_data.find(clnt_socket);
	if (ite == fd_data.end())
	{
		fd_data.insert(std::pair<int, t_data>(clnt_socket, t_data{""}));
	}
	RESPONSE_MESSAGE += buffer;
	if (buffer[BUFF_SIZE - 1] == '\0')
	{
		FD_CLR(clnt_socket, master_readfds);
		FD_SET(clnt_socket, master_writefds);
		RESPONSE_MESSAGE = makeResponseMessage(RESPONSE_MESSAGE);
	}
}

void	IOMultiplexingLoop(vec_int_ vec_serv_socket)
{
	fd_set	master_readfds;
	fd_set	master_writefds;
	fd_set	readfds;
	fd_set	writefds;
	int		max_descripotor = initMasterReadfds(vec_serv_socket, &master_readfds);
	int		ready;
	struct timeval		timeout;
	map_fd_data_		fd_data;

	FD_ZERO(&master_writefds);
	while(true)
	{
		memcpy(&writefds, &master_writefds, sizeof(master_writefds));
		memcpy(&readfds, &master_readfds, sizeof(master_readfds));
		timeout.tv_sec  = 0;
		timeout.tv_usec = 200;
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
					sendResponse(fd, &master_readfds, &master_writefds, &max_descripotor, fd_data);
					std::cout << "clnt_socket: " <<  fd << ", max_descripotor: " << max_descripotor << std::endl;
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
						storeRequestToMap(fd, &master_readfds, &master_writefds, fd_data);
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

// siege -b -t 10s http://localhost:8080
// c++ -Wall -Wextra -Werror server.cpp -o server && ./server 8080 8081 8082
// http://localhost:8080
// curl -i -X GET localhost:8080/
// https://github.com/kyamagis/Socket.git master