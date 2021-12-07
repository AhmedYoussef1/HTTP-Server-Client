#ifndef SIMPLE_HTTP_H
#define SIMPLE_HTTP_H

#include <iostream>
#include <string.h>
using namespace std;

const string HTTP_OK = "HTTP/1.1 200 OK\r\n";
const string HTTP_NOT_FOUND = "HTTP/1.1 404 Not Found\r\n";
const string HTTP_BAD_REQUEST = "HTTP/1.1 400 Bad Request\r\n";
const string HTTP_SERVER_ERROR = "HTTP/1.1 500 Internal Server Error\r\n";
const string HTTP_SERVER_BUSY = "HTTP/1.1 503 Service Unavailable\r\n";

// retrieve http request_line and headers
// return pointer to the char after the http
char* read_http(char* curr, string& http_fill);

// return http method
string get_method(string& req);

// return url in the request line
string get_url(string& req);

string make_headers(int n, string name[], string value[]);

#endif