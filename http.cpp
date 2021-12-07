#include "http.h"

char* read_http(char* curr, string& http_fill) {
    char buffer[4096];

    // search for http end
    char* end_http = strstr(curr, "\r\n\r\n");
    if (end_http == NULL) // if not found
        return NULL;
    
    int index = end_http - curr;

    // copy http to the buffer
    strncpy(buffer, curr, index + 2);
    buffer[index + 2] = '\0';

    http_fill = buffer;
    return end_http + 4;
}

string get_method(string& req){
    int i = req.find(" ");
    string method = req.substr(0, i);

    return method;
}

string get_url(string& req){
    int skip = req.find(" ") + 1;
    int i = req.find(" ", skip);
    string url = req.substr(skip , i - skip);

    return url;
}

void del_req(string& req){
    int i = req.find("\r\n\r\n");
    if (i > 0) req.erase(0, i + 4);
}

string make_headers(int n, string name[], string value[]) {
    string headers = "";
    for (int i = 0; i < n; i++) {
        headers += name[i] + ": " + value[i] + "\r\n";
    }
    return headers;
}