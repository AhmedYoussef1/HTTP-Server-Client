#include <iostream>
#include <unistd.h>     // fork, close
#include <string.h>     // memset
#include <netdb.h>      // socket, addrinfo
#include <poll.h>
#include <fcntl.h>
#include <pthread.h>
#include "func_caller.h"
#include "http.h"
using namespace std;

const int MAX_CONN = 20;        // maximum number of connections
const int BACKLOG = 10;         // maximum number of pending connections
const int BUFF_SIZE = 10240;    // buffer size
const int FOUT = 1000;           // recieving file timout
const int MAX_TOUT = 21000, MIN_TOUT = 1000;    // maximum/minimum timeout

const string GET_PATH = "../server_files/get";
const string POST_PATH = "../server_files/post";

int connections = 0;        // number of connections
int timeout = MAX_TOUT;     // Connection timeout

pthread_mutex_t conn_lock;
pthread_mutex_t fd_lock;

void* connection_handler(void* socket_fd);
void request_handler(pollfd* pfd, char req[], int len);
void GET_handler(pollfd* pfd, string& req);
void POST_handler(pollfd* pfd, string& req, char data[], int len);
void connections_inc();
void connections_dec();

int main(int argc, char **argv) 
{
    string port = (argc > 1) ? argv[1] : "80";    // default port is 80

    // initialize variables
    connections = 0;
    timeout = MAX_TOUT;
    if(pthread_mutex_init(&fd_lock, NULL) or pthread_mutex_init(&conn_lock, NULL)) {
        perror("** mutex_init");
        exit(1);
    }

    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    int rv;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("** socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("** bind");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        perror("** failed to bind");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("** listen");
        exit(1);
    }

    printf("[+] Waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("** accept");
            continue;
        }
        
        if (connections < MAX_CONN) {
            // avoid modifying new_fd and th_id before the new thread read them
            pthread_mutex_lock(&fd_lock);
            pthread_t th_id;
            int new_fd_save = new_fd;

            // create thread for the new connection
            pthread_create(&th_id, NULL, connection_handler, &new_fd_save);
        } else {
            string res = HTTP_SERVER_BUSY + "\r\n";
            send(new_fd, &res[0], res.length(), 0);
        }
    }

    return 0;
}

// Note that all connections are persistent
void* connection_handler(void* socket_fd) {
    int fd = *((int*)socket_fd);
    pthread_mutex_unlock(&fd_lock); // fd readed successfuly
    
    connections_inc();
    printf("{%d} Connection Started\n", fd);

    char buffer[BUFF_SIZE];
    struct pollfd pfd[1];
    pfd[0].fd = fd;

    while(1) {
        int l = recv_caller(pfd, buffer, BUFF_SIZE - 1, timeout);
        buffer[l] = '\0';

        if (pfd[0].revents & POLLIN) { // if data recieved   
            if(l == 0){ // l = 0 means that client has closed the connection
                printf("{%d} Connection closed\n", fd);
                break;
            }
            request_handler(pfd, buffer, l);
            continue;
        }
        else if (l == 0)
            printf("{%d} Timeout\n", fd);
        break;
    }

    close(fd);
    connections_dec();
    printf("{%d} Connection Ended\n", fd);
    pthread_exit(NULL);
}

void request_handler(pollfd* pfd, char buffer[], int len) {
    char* curr = buffer;
    string req;

    // get http (request_line and headers) and get pointer to the data
    if ((curr = read_http(curr, req)) == NULL) { // if no http found
        send_caller(pfd, HTTP_BAD_REQUEST + "\r\n", timeout);
    }
    else {
        printf("recieved:\n----------\n%s\n----------\n",&req[0]);

        string method = get_method(req);
        if (method == "GET") {
            GET_handler(pfd, req);
        }
        else if (method == "POST")
            POST_handler(pfd, req, curr, len - (int)(curr - buffer));
        else
            send_caller(pfd, HTTP_BAD_REQUEST + "\r\n", timeout);
    }
}

void GET_handler(pollfd* pfd, string& req) {
    string url = get_url(req);
    if (url == "/") url = "/index.html";

    string path = GET_PATH + url;

    FILE* f = fopen(&path[0], "rb");

    if (f == NULL) {
        send_caller(pfd, HTTP_NOT_FOUND + "\r\n", timeout);
    }
    else {
        char buffer[BUFF_SIZE];
        size_t read, sent;
        long fsize;

        // get file size
        fseek(f, 0, SEEK_END);
        fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        // make headers
        string names[] = {"Content-Length"};
        string values[] = {to_string(fsize)};
        string headers = make_headers(1, names, values);
        // make response line
        string http_res = HTTP_OK + headers + "\r\n";

        // send http
        send_caller(pfd, http_res, timeout);

        // send file
        do {
            if ((read = fread(buffer, 1, sizeof(buffer) - 1, f)) > 0)
                sent = send_caller(pfd, buffer, read, timeout);
            else
                sent = 0;
        }while (read > 0 and read == sent);

        if (read != sent)
            printf("** Failed to send the file\n");
    }
}

void POST_handler(pollfd* pfd, string& req, char data[], int len) {
    char buffer[BUFF_SIZE];
    string url, path, res; 
    int read = len, saved;

    url = get_url(req);
    path = POST_PATH + url;

    // receive data and save it
    FILE* file = fopen(&path[0], "wb");
    if(file == NULL) {
        perror("** Faied to open file ");
        send_caller(pfd, HTTP_SERVER_ERROR + "\r\n", timeout);
        return;
    }

    if (fwrite(data, 1, read, file) != read) {
        perror("** Faied to save the file ");
        send_caller(pfd, HTTP_SERVER_ERROR + "\r\n", timeout);
        fclose(file);
        return;
    }
    
    // keep reading from the socket untill recieving zero bytes
    while((read = recv_caller(pfd, buffer, BUFF_SIZE - 1, FOUT)) > 0) {
        if (fwrite(buffer, 1, read, file) != read) {
            perror("** Faied to save the file ");
            send_caller(pfd, HTTP_SERVER_ERROR + "\r\n", timeout);
            fclose(file);
            return;
        }
    }
    
    fclose(file);
    send_caller(pfd, HTTP_OK + "\r\n", timeout);
}

void connections_inc() {
    pthread_mutex_lock(&conn_lock);
    int time = MAX_TOUT - (1000 * ++connections);
    timeout = (time <= MIN_TOUT) ? MIN_TOUT : time;
    pthread_mutex_unlock(&conn_lock);
}

void connections_dec() {
    pthread_mutex_lock(&conn_lock);
    int time = MAX_TOUT - (1000 * --connections);
    timeout = (time <= MIN_TOUT) ? MIN_TOUT : time;
    pthread_mutex_unlock(&conn_lock);
}