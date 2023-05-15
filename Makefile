
all: serv clnt

serv:
	c++ -Wall -Wextra -Werror -fsanitize=address persistent_connection_server.cpp -o server

clnt:
	c++ -Wall -Wextra -Werror -fsanitize=address client.cpp -o client

servs:
	./server 8080 8081 8082

clnts:
	./client 8080

clean:
	rm server
	rm client

re: clean start