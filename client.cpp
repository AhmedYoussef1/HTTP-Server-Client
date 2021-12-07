#include <iostream>
#include <unistd.h>     // fork, close
#include <string.h>     // memset
#include <netdb.h>      // socket, addrinfo
#include <poll.h>
#include "func_caller.h"
#include "http.h"

using namespace std;

const int BUFF_SIZE = 10240;
const int TIMEOUT = 10000;  // Connection timeout in millisecond
const int FOUT = 1000;       // recieving file timout
const string PATH = "../client_files"; 
const string COMMANDS = "../client_files/commands"; 

int connect_to_server(char host[], char port[]);
void get_some_data(pollfd* pfd, string url);
void post_some_data(pollfd* pfd, string url);

int main(int argc, char **argv)
{
    string host = (argc > 1) ? argv[1] : "127.0.0.1";
    string port = (argc > 2) ? argv[2] : "80";

    // connect to the server
    struct pollfd pfd[1];
    pfd[0].fd = connect_to_server(&host[0], &port[0]);

    printf("[+] Connection started\n");

    // read commands from file
    char f_method[15], f_url[100];
    string method, url, command_path;

    command_path = COMMANDS + "/commands1.txt";
    FILE* file = fopen(&command_path[0], "r");
    if (file == NULL) {
        perror("** fopen");
        exit(1);
    }

    printf("[+] Running commands...\n");

    while(!feof(file)) {
        fscanf(file, "%s %s\n", f_method, f_url);
        printf("--> %s %s\n", f_method, f_url);

        method = f_method;
        url = (string)"/" + f_url;

        if(method == "client_get")
            get_some_data(pfd, url);
        else if(method == "client_post")
            post_some_data(pfd, url);
        else
            printf("** Wrong command -> %s %s\n", f_method, f_url);
    }
    
    close(pfd[0].fd);
    printf("[+] Connection ended\n");

    return 0;
}

int connect_to_server(char s_host[], char s_port[]) {
    int sockfd, numbytes;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(s_host, s_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("** socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("** connect");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "** failed to connect\n");
        exit(1);
    }

    freeaddrinfo(servinfo); // all done with this structure
    return sockfd;
}

void get_some_data(pollfd* pfd, string url) {
    char buffer[BUFF_SIZE];
    string req, res;
    // send get request
    req = "GET " + url + " HTTP/1.1\r\n\r\n";
    send_caller(pfd, req, TIMEOUT);

    // recieve response
    int len = recv_caller(pfd, buffer, BUFF_SIZE - 1, TIMEOUT);
    buffer[len] = '\0';

    if(len == 0) {
        printf("** Server doesn't response");
        exit(1);
    }

    // read http and get pointer to the data
    char* data = read_http(buffer, res);

    printf("recieved:\n----------\n%s\n----------\n",&res[0]);

    // if every thing ok
    if(res.find(HTTP_OK) == 0) {
        // put response into a file
        len = len - (data - buffer); // data length
        string path = PATH + url;
        FILE* file = fopen(&path[0], "wb");
        if(file == NULL) {
            perror("** Faied to open file ");
            return;
        }

        if (fwrite(data, 1, len, file) != len) return;
        // keep reading from the socket untill recieving zero bytes
        while((len = recv_caller(pfd, buffer, BUFF_SIZE - 1, FOUT)) > 0) {
            if (fwrite(buffer, 1, len, file) != len) {
                perror("** Faied to save the file ");
                return;
            }
        }
    }
}

void post_some_data(pollfd* pfd, string url) {
    char buffer[BUFF_SIZE];
    string req, res, path;
    int read, sent;

    path = PATH + url;
    FILE* file = fopen(&path[0], "rb");

    if(file == NULL) {
        perror("** Faied to open the file ");
        return;
    }

    // send post request
    req = "POST " + url + " HTTP/1.1\r\n\r\n";
    send_caller(pfd, req, TIMEOUT);

    // send the file
    do {
        if ((read = fread(buffer, 1, sizeof(buffer) - 1, file)) > 0)
            sent = send_caller(pfd, buffer, read, TIMEOUT);
        else
            sent = 0;
    }while (read > 0 and read == sent);

    if (read != sent) {
        printf("** Failed to send the file\n");
        // TODO make new connection instead of exit(1)
        exit(1); // because this error may corrupt other requests in the same connection
    }

    // recieve response
    read = recv_caller(pfd, buffer, BUFF_SIZE - 1, TIMEOUT);
    buffer[read] = '\0';

    if(read == 0) {
        printf("** Server doesn't response");
        exit(1);
    }

    read_http(buffer, res);
    printf("recieved:\n----------\n%s\n----------\n",&res[0]);
    fclose(file);
}
