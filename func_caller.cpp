#include "func_caller.h"

void end_process() {
    printf("[+] Connection ended with error\n");
    exit(1);
}

ssize_t send_caller(pollfd* pfd, void* msg, int len, int timeout) {
    int pval, s = 0;
    
    pfd[0].events = POLLOUT;
    pval = poll(pfd, 1, timeout);
    
    if(pfd[0].revents == POLLOUT) {
        if ((s = send(pfd[0].fd, msg, len, 0)) == -1) {
            perror("** Error: failed to send ");
            end_process();
        }
    }
    else if(pval == -1) {
        perror("** Error in poll ");
        end_process();
    }
    return s;
}

ssize_t send_caller(pollfd* pfd, string msg, int timeout) {
    return send_caller(pfd, &msg[0], msg.length(), timeout);
}

ssize_t recv_caller(int fd, void* msg, size_t len) {
    ssize_t r;
    if ((r = recv(fd, msg, len, 0)) == -1) {
        perror("** Error in recv ");
        end_process();
    }
    return r;
}

ssize_t recv_caller(pollfd* pfd, void* msg, int len, int timeout) {
    int pval, r = 0;
    
    pfd[0].events = POLLIN; // wakeup when data is ready to recv()
    pval = poll(pfd, 1, timeout);
    
    if(pfd[0].revents & POLLIN) {
        if ((r = recv(pfd[0].fd, msg, len, 0)) == -1) {
            perror("** failed to recieve ");
            end_process();
        }
    }
    else if(pval == -1) {
        perror("** error in poll ");
        end_process();
    }
    return r;
}
FILE* fopen_caller(string path, char mode[]) {
    FILE* f = fopen(path.c_str(), mode);
    if(f == NULL) {
        perror("** Error: failed to read file ");
        end_process();
    }
    return f;
}