#ifndef FUNC_CALLER_H
#define FUNC_CALLER_H

#include <iostream>
#include <poll.h>
#include <unistd.h>
#include <netdb.h>
using namespace std;

void end_process();
ssize_t send_caller(pollfd* pfd, void* msg, int len, int timeout);
ssize_t send_caller(pollfd* pfd, string msg, int timeout);
ssize_t recv_caller(pollfd* pfd, void* msg, int len, int timeout);
FILE* fopen_caller(string path, char mode[]);

#endif