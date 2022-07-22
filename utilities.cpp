#include <bits/stdc++.h>
#include <functional>
#include <array>

#include "utilities.h"
#include "tcp-utils.h"
#include "file-operations.h"

vector<string> tokenize(char* str, const char *delim) {
    vector<string> tokens;

    char *tab2 = new char[strlen(str) + 1];
    strcpy(tab2, str);

    char *saveptr;
    char *token = strtok_r(tab2, delim, &saveptr);
    while (token != nullptr) {
        tokens.emplace_back(token);
        token = strtok_r(nullptr, delim, &saveptr);
    }
    delete[] tab2;
    return tokens;
}

int createMasterSocket(int port) {
    int socketDescriptor = passivesocket(port, 32);

    if (socketDescriptor < 0) {
        perror("passive_socket");
        return 1;
    }

    return socketDescriptor;
}

char* convertStringToCharArray(string temp) {
    char *tab2 = new char[temp.length() + 1];
    strcpy(tab2, temp.c_str());
    return tab2;
}

void readConfigurationParametersFromFile(const string& configurationFile, int &tmax, int &bulletinBoardServerPort, int &syncServerPort,
                                         string &bbfile, vector<string> &peers, bool &isDaemon, bool &debuggingModeEnabled) {
    int file = open(configurationFile.c_str(), O_RDONLY,
                    S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);
    if (file != -1) {
        // The file exists on disk. No processing would have been necessary if the file was not present.
        char *lineContent = new char[255];
        while (readline(file, lineContent, 255) != recv_nodata) {
            string trimmedContent = trim(lineContent);
            if (trimmedContent.empty()) {
                continue;
            }
            vector<string> keyValuePair = tokenize(lineContent, "=");

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
                vector<string> configFilePeers = tokenize(convertStringToCharArray(value), " ");
                peers.insert(peers.end(), configFilePeers.begin(), configFilePeers.end());
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

string readKeyFromConfigurationFile(string keyToRead, string configurationFile, string defaultValue) {
    string result = defaultValue;

    int file = open(configurationFile.c_str(), O_RDONLY,
                    S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);
    if (file != -1) {
        // The file exists on disk. No processing would have been necessary if the file was not present.
        char *lineContent = new char[255];
        while (readline(file, lineContent, 255) != recv_nodata) {
            string trimmedContent = trim(lineContent);
            if (trimmedContent.empty()) {
                continue;
            }
            std::vector<std::string> keyValuePair = tokenize(lineContent, "=");

            if (keyValuePair[0] == keyToRead) {
                result = keyValuePair[1];
                break;
            }
        }

        close(file);
    }
    return result;
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

    shutdown(*socketToClose, SHUT_RDWR);
    close(*socketToClose);
    cout << "Closed SocketID "<< *socketToClose <<". Resource Cleanup Successful!" << endl;
}

string createMessage(float code, const char responseText[], const char additionalInfo[], bool shouldTerminateWithNewline) {
    char buffer[255];
    memset(buffer, 0, sizeof buffer);
    snprintf(buffer, 255, "%2.1f %s %s%s", code, responseText, additionalInfo, shouldTerminateWithNewline ? "\n" : "");

    return string(buffer);
}

ssize_t sendMessageToSocket(float code, const char responseText[], const char additionalInfo[], int socketToSend) {
    char buffer[255];
    memset(buffer, 0, sizeof buffer);
    snprintf(buffer, 255, "%2.1f %s %s\n", code, responseText, additionalInfo);
    return send(socketToSend, buffer, strlen(buffer), 0);
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

char* joinTwoStringsWithDelimiter(char* str1, const char* str2, char delimiter) {
    char* full_text = (char*)malloc(strlen(str1) + 1 + strlen(str2) + 1);
    strcpy(full_text, str1 );
    strcat(full_text, &delimiter);
    strcat(full_text, str2);
    return full_text;
}

bool is_number(const std::string& s) {
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

int readline(const int fd, char* buf, const size_t max) {
    return readlineFromSocket(fd, buf, max, ref_ignore<char>);
}

