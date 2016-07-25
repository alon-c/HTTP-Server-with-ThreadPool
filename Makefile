all: server

server: server.c threadpool.c threadpool.h
	gcc -o server server.c threadpool.c -lpthread 