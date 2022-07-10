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
#include <cassert>

#include "board-server.h"
#include "tcp-utils.h"
#include "utilities.h"
#include "file-operations.h"
#include "logger.h"

using namespace std;

vector<pthread_t> bulletinBoardServerThreads;
long int masterSocket;

string bulletin_board_file;
bool delayOperations;
vector<string> peersList;
int tmax, bulletinBoardServerPort;

void handle_bulletin_board_client(int master_socket) {
    pthread_t currentThread = pthread_self();
    cout << "New Thread " << currentThread <<  " launched." << endl;

    sockaddr_in client_address{}; // the address of the client...
    unsigned int client_address_len = sizeof(client_address); // ... and its length

    while (true) {
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
        int slave_socket = accept(master_socket, (struct sockaddr *) &client_address, &client_address_len);
        if (slave_socket < 0) {
            if (errno == EINTR) {
                cout << "accept() interrupted! {Debugging Purposes}" << endl; // TODO: Clearly Examine this scenario upon cancellation/pthread_kill
                return;
            }
            perror("accept");
            return;
        }
        pthread_cleanup_push(cleanup_handler, &slave_socket);

        cout << "########## New Remote Client Accepted ##########" << endl;

        auto sendMessage = [&](float code, char responseText[], char additionalInfo[]) {
            sendMessageToSocket(code, responseText, additionalInfo, slave_socket);
        };

        sendMessage(0.0, "", const_cast<char *>(bulletinBoardGreetingText.c_str()));

        const int ALEN = 256;
        char req[ALEN];
        int n;

        string user = "nobody";

        while ((n = readline(slave_socket, req, ALEN - 1)) != recv_nodata) {
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
            if (req[n - 1] == '\r') {
                req[n - 1] = '\0';
            }

            string inputCommand = req;
            cout << "Command Received from Client: " << inputCommand << endl;

            std::vector<std::string> tokens;
            tokenize(string(inputCommand), " ", tokens);

            if (inputCommand.rfind("QUIT", 0) == 0) {
                break;
            } else if (tokens.size() != 2) {
                sendMessage(0.0, "ERROR", "Malformed Command Received from Client!" );
            } else if (inputCommand.rfind("USER", 0) == 0) {
                // TODO: How to handle multiple USER commands during a single session??
                if (tokens[1].find('/') != std::string::npos) {
                    // The name supplied by client contains '/', which isn't allowed
                    sendMessage(1.2, "ERROR USER", "The 'username' argument cannot contain '/'");
                } else {
                    user = tokens[1];
                    sendMessage(1.0, "HELLO", const_cast<char *>(user.c_str()));
                }
            } else if (inputCommand.rfind("WRITE", 0) == 0) {
                writeToFile(bulletin_board_file, user, tokens[1], slave_socket);
            } else if (inputCommand.rfind("READ", 0) == 0) {
                readMessageFromFile(bulletin_board_file, stoi(tokens[1].c_str()), slave_socket);
            } else if (inputCommand.rfind("REPLACE", 0) == 0) {
                replaceMessageInFile(bulletin_board_file, user, tokens[1], slave_socket);
            } else {
                sendMessage(0.0, "ERROR", "Invalid Command Entered!");
            }
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
        }

        if (n == recv_nodata) {
            // read 0 bytes = EOF:
            cout << "Connection closed by client." << endl;
        }

        sendMessage(4.0, "BYE", "This is Goodbye.");
        shutdown(slave_socket, SHUT_RDWR);
        close(slave_socket);
        pthread_cleanup_pop(0);

        cout << "########## Remote Connection Terminated ##########" << endl;
    }
}

void startBulletinBoardServer() {
    masterSocket = createMasterSocket(bulletinBoardServerPort);
    cout << "Bulletin Board Server up and listening on port " << bulletinBoardServerPort << endl;

    createThreads(tmax, &handle_bulletin_board_client, (void*)masterSocket, bulletinBoardServerThreads);
}

void bulletin_board_sigquit_handler(int signum) {
    killThreads(bulletinBoardServerThreads);
    close(masterSocket);
    cout << "All BulletinBoard Server Threads terminated." << endl;
}

void bulletin_board_sighup_handler(string configurationFile) {
    bulletin_board_sigquit_handler(1);

    vector<string> newPeersList;

    int integerToIgnore; bool booleanToIgnore;
    readConfigurationParametersFromFile(configurationFile, tmax, bulletinBoardServerPort, integerToIgnore, bulletin_board_file, newPeersList, booleanToIgnore, delayOperations);
    if (!newPeersList.empty()) {
        peersList = newPeersList;
    }

    set_initial_message_number(bulletin_board_file);
    setDebuggingPreference(delayOperations);

    startBulletinBoardServer();
    cout << "Reconfiguration Successful. Normal Operation Resumed Successfully! << endl";
}

int board_server(char **argv) {
    bulletinBoardServerPort = atoi(argv[1]);
    delayOperations = strcmp(argv[2], "true") == 0;
    bulletin_board_file = argv[3];
    tmax = atoi(argv[4]);

    int i = 5;
    while (argv[i] != nullptr) {
        peersList.emplace_back(argv[i]);
        i++;
    }

    cout << "Process ID: " << getpid() << endl;

    set_initial_message_number(bulletin_board_file);
    setDebuggingPreference(delayOperations);

    startBulletinBoardServer();

    pthread_exit(nullptr);
}

//int main(int argc, char **argv, char *envp[]) {
//    board_server(argv);
//}


