HTTP server (HTTP server part 2)

1. server.c
implementation of a limited HTTP server that: - Constructs an HTTP response based on client's request. 
- Sends the response to the client.

note: Valgrind checked, no memory leacs or errors what so ever. compile with: threadpool files and use  -lpthread flag.

---

Threadpool (HTTP server part 1)

1. threadpool.c
implementation of a threadpool that reprisents a simple implemantation of a threadpool algoritem.

note: Valgrind checked, no memory leacs or errors what so ever. compile with -lpthread flag,