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

void cleanup_handler(void *arg ) {
    int *socketToClose = (int *) arg;

    close(*socketToClose);
    cout << "Closed SocketID "<< *socketToClose <<". Resource Cleanup Successful!" << endl;
}

void sendMessageToSocket(float code, char responseText[], char additionalInfo[], int socketToSend) {
    char buffer[255];
    memset(buffer, 0, sizeof buffer);
    snprintf(buffer, 255, "%2.1f %s %s\n", code, responseText, additionalInfo);
    send(socketToSend, buffer, sizeof(buffer), 0);
}

const std::string WHITESPACE = " \n\r\t\f\v";


std::string ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

std::string rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

std::string trim(const std::string &s) {
    return rtrim(ltrim(s));
}

bool is_true(const string& value) {
    vector<string> truthyValues = {"1", "true"};
    return std::find(truthyValues.begin(), truthyValues.end(), value) != truthyValues.end();
};
