#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <map>
#include <pthread.h>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <set>
#include <functional>
#include <algorithm>
#include <sys/ioctl.h>
#include <unistd.h>
#include <getopt.h>

#include "tcp-utils.h"
#include "utilities.h"
#include "board-server.h"

using namespace std;

string getConfigurationFileParameterFromTerminal(int argc, char **argv) {
    string configurationFile;

    int c;
    while ((c = getopt(argc, argv, "b:T:p:s:fdc:")) != -1) {
        if ((char)c == 'c') {
            configurationFile = string(optarg);
            cout << "configuration file specified via command line as => " << configurationFile << endl;
        }
    }
    optind = 0; // To reset getopt
    return configurationFile;
}

void writeToProcessIdFile() {
    int processIdFileDescriptor = open("./bbserv.pid", O_WRONLY | O_CREAT | O_TRUNC,
                                       S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);
    if (processIdFileDescriptor < 0) {
        perror("open bbserv.pid");
    }
    int processId = getpid();

    cout << "Process ID: " << processId << endl;

    char buffer[255];
    memset(buffer, 0, sizeof buffer);
    snprintf(buffer, 255, "Process ID: %d\n", processId);

    write(processIdFileDescriptor, buffer, strlen(buffer));
    close(processIdFileDescriptor);
}

int main(int argc, char **argv, char *envp[]) {
    int tmax = 20, bulletinBoardServerPort = 9000, syncServerPort = 10000;
    bool isDaemon = true, debuggingModeEnabled = false;
    vector<string> peers; // possibly empty => implies a Non-Replicated, single node Server
    string bbfile;

    string configurationFile = getConfigurationFileParameterFromTerminal(argc, argv);
    if (configurationFile.empty()) {
        configurationFile = "./bbserv.conf";
        cout << "No configuration file specified via command line. The default " << configurationFile << " shall be used." << endl;
    }

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

    int c;
    while ((c = getopt(argc, argv, "b:T:p:s:fdc:")) != -1) {
        if ((char)c == 'b') {
            bbfile = string(optarg);
            cout << "bbfile configured via command line as => " << bbfile << endl;
        }
        if ((char)c == 'T') {
            tmax = atoi(optarg);
            cout << "Tmax configured via command line as => " << tmax << endl;
        }
        if ((char)c == 'p') {
            bulletinBoardServerPort = atoi(optarg);
            cout << "bp (Bulletin Board Server port) configured via command line as => " << bulletinBoardServerPort << endl;
        }
        if ((char)c == 's') {
            syncServerPort = atoi(optarg);
            cout << "sp (Synchronization Server port) configured via command line as => " << syncServerPort << endl;
        }
        if ((char)c == 'f') {
            isDaemon = false;
            cout << "Server compelled to run in foreground [NOT a Daemon] via command line." << endl;
        }
        if ((char)c == 'd') {
            debuggingModeEnabled = true;
            cout << "Debugging Mode activated via command line." << endl;
        }
    }

    if (bbfile.empty()) {
        cout << "Neither the configuration file " << configurationFile << " nor the command-line specified the required parameter bbfile. Server unable to start." << endl;
        return 1;
    }

    if (isDaemon) {
        int background_process = fork();
        if (background_process < 0) {
            perror("startup fork");
            return 1;
        }
        if (background_process) // parent dies!
            return 0;

        setpgid(getpid(),0); // Leave the current process group
        chdir(".");

        for (int i = getdtablesize() - 1; i >= 0 ; i--) {
            close(i);
        }

        int fd = open("/dev/tty", O_RDWR);
        ioctl(fd, TIOCNOTTY);
        close(fd);

        open("/dev/null", O_RDWR);
        fd = open("./bbserv.log", O_WRONLY | O_CREAT | O_APPEND,
                  S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);
        dup(fd);
    }

    signal(SIGPIPE, SIG_IGN);
    umask(0137);

    writeToProcessIdFile();

    int numberOfPeersSpecifiedViaCommandLine = argc - optind;
    for (int i = 0; i < numberOfPeersSpecifiedViaCommandLine; i++) {
        peers.emplace_back(string(argv[i + optind]));
    }

    string delayReadWriteString = debuggingModeEnabled ? "true" : "false";
    char* delayOperations = const_cast<char *>(delayReadWriteString.c_str());

    int totalPeers = peers.size();
    char* board_server_arguments[totalPeers + 6];
    board_server_arguments[0] = "executableName";
    board_server_arguments[1] = strdup(std::to_string(bulletinBoardServerPort).c_str());
    board_server_arguments[2] = delayOperations;
    board_server_arguments[3] = const_cast<char *>(bbfile.c_str());
    board_server_arguments[4] = strdup(std::to_string(tmax).c_str());

    for (int i = 0; i < totalPeers; i++) {
        board_server_arguments[i + 5] = const_cast<char *>(peers[i].c_str());
    }
    board_server_arguments[totalPeers + 5] = nullptr;

    pthread_t tt;
    pthread_attr_t ta;
    pthread_attr_init(&ta);
    pthread_attr_setdetachstate(&ta,PTHREAD_CREATE_DETACHED);

    if (pthread_create(&tt, &ta, (void* (*) (void*)) board_server, (void*)board_server_arguments) != 0) {
        perror("pthread_create");
        return 1;
    }

    // TODO: Start the board-server and sync-server here

    pthread_exit(nullptr);
}
