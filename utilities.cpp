#include <bits/stdc++.h>

#include "utilities.h"
#include "tcp-utils.h"
#include <functional>
#include <array>


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

int createMasterSocket(int port) {
    int socketDescriptor = passivesocket(port, 32);

    if (socketDescriptor < 0) {
        perror("passive_socket");
        return 1;
    }

    int reuse;
    setsockopt(socketDescriptor, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    return socketDescriptor;
}

void readConfigurationParametersFromFile(const string& configurationFile, int &tmax, int &bulletinBoardServerPort, int &syncServerPort,
                                         string &bbfile, vector<string> &peers, bool &isDaemon, bool &debuggingModeEnabled) {
    int file = open(configurationFile.c_str(), O_RDONLY,
                    S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);
    if (file != -1) {
        // The file exists on disk. No processing would have been necessary if the file was not present.
        char *lineContent = new char[255];
        while (readline(file, lineContent, 255) != recv_nodata) {
            string trimmedContent = trim(const_cast<char *>(lineContent));
            if (trimmedContent.empty()) {
                continue;
            }
            std::vector<std::string> keyValuePair;
            tokenize(lineContent, "=", keyValuePair);

            string key = keyValuePair[0], value = keyValuePair[1];
            if (key == "THMAX") {
                tmax = std::stoi(value);
            }
            if (key == "BBPORT") {
                bulletinBoardServerPort = std::stoi(value);
            }
            if (key == "SYNCPORT") {
                syncServerPort = std::stoi(value);
            }
            if (key == "BBFILE") {
                bbfile = value;
            }
            if (key == "PEERS") {
                tokenize(value, " ", peers);
            }
            if (key == "DAEMON") {
                isDaemon = is_true(value);
            }
            if (key == "DEBUG") {
                debuggingModeEnabled = is_true(value);
            }
        }

        close(file);
    }
}

void killThreads(vector<pthread_t> &threadsToKill) {
    for (pthread_t threadToKill: threadsToKill) {
        cout << "Killing Thread " << threadToKill << endl;
        pthread_cancel(threadToKill);
        pthread_join(threadToKill, nullptr);
    }

    threadsToKill.clear();
}


int createThreads(int numberOfThreads, void (*serverHandler)(int), void *handlerArgument, vector<pthread_t> &threadsCollection) {
    pthread_t tt;
    pthread_attr_t ta;
    pthread_attr_init(&ta);
    pthread_attr_setdetachstate(&ta, PTHREAD_CREATE_JOINABLE);

    for (int i = 0; i < numberOfThreads; i++) {
        if (pthread_create(&tt, &ta, (void*(*)(void*))serverHandler, handlerArgument) != 0) {
            perror("pthread_create");
            return 1;
        }
        threadsCollection.push_back(tt);
    }
    return 0;
}

void cleanup_handler(void *arg ) {
    int *socketToClose = (int *) arg;

    close(*socketToClose);
    cout << "Closed SocketID "<< *socketToClose <<". Resource Cleanup Successful!" << endl;
}

int obtain_initial_message_number(string file) {
    const int ALEN = 256;
    char req[ALEN];

    int fd = open(file.c_str(), O_CREAT | O_RDONLY,
                  S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);
    int l = 0;
    while (readline(fd, req, ALEN - 1) != recv_nodata) {
        l++;
    }
    close(fd);
    return l;
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
