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

#include "tcp-utils.h"
#include "utilities.h"

using namespace std;

int main(int argc, char **argv, char *envp[]) {
    int tmax = 20, bulletinBoardServerPort = 9000, syncServerPort = 10000;
    bool isDaemon = true, debuggingModeEnabled = false;
    vector<string> peers; // possibly empty => implies a Non-Replicated, single node Server

    string bbfile;
    char* configurationFile = "./bbserv.conf"; // TODO: May be specified via command-line parameter

    int file = open(configurationFile, O_RDONLY,
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
    while ((c = getopt(argc, argv, "b:T:p:s:fd")) != -1) {
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

    int numberOfPeers = argc - optind;
    for (int i = 0; i < numberOfPeers; i++) {
        peers.emplace_back(string(argv[i + optind]));
    }

    if (bbfile.empty()) {
        cout << "Neither the configuration file " << configurationFile << " nor the command-line specified the required parameter bbfile. Server unable to start." << endl;
        return 1;
    }

    // TODO: Start the board-server and sync-server here

    pthread_exit(nullptr);
}
