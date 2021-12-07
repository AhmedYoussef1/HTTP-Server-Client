all: server client

server:
	@g++ -g server.cpp func_caller.cpp http.cpp -lpthread -o bin/server
	@echo "server was made"

client:
	@g++ -g func_caller.cpp http.cpp client.cpp -o bin/client
	@echo "client was made"