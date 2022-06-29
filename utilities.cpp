#include <bits/stdc++.h>

#include "utilities.h"
#include "tcp-utils.h"


// Borrowed from https://java2blog.com/split-string-space-cpp/
void tokenize(std::string const &str, const char *delim,
              std::vector<std::string> &out) {
    char *token = strtok(const_cast<char *>(str.c_str()), delim);
    while (token != nullptr) {
        out.emplace_back(token);
        token = strtok(nullptr, delim);
    }
}

int createSocket(string &valueRHOST, string &valueRPORT, int &socketId) {
    socketId = connectbyport(const_cast<char *>(valueRHOST.c_str()),const_cast<char *>(valueRPORT.c_str()));
    if (socketId == err_host) {
        cerr << "Cannot Find Host: " << valueRHOST << endl;
        return 1;
    }
    if (socketId < 0) {
        cout << "Connection refused by " << valueRHOST << ":" << valueRPORT << endl;
        socketId = -1;
        return 1;
    }
    // we now have a valid, connected socket
    cout << "Successfully connected to remote host " << valueRHOST << ":" << valueRPORT << " via socketId="<< socketId << endl;
    return 0;
}
