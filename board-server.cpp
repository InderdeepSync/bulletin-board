#include <sys/wait.h>
#include <sys/types.h>
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

#include "board-server.h"
#include "tcp-utils.h"
#include "utilities.h"
#include "file-operations.h"
#include "peer-communication.h"
#include "logger.h"

using namespace std;

vector<pthread_t> bulletinBoardServerThreads;
long int bulletinBoardMasterSocket;

vector<string> peersList;
int tmax, bulletinBoardServerPort;

void handle_bulletin_board_client(int master_socket) {
    pthread_t currentThread = pthread_self();
    printf("New Bulletin Board Thread %lu launched.\n", currentThread);

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

        auto sendMessage = [&](float code, const char responseText[], const char additionalInfo[]) {
            return sendMessageToSocket(code, responseText, additionalInfo, slave_socket);
        };

        sendMessage(0.0, "", bulletinBoardGreetingText.c_str());

        const int ALEN = 256;
        char req[ALEN];
        int n;

        string user = "nobody";

        while ((n = readline(slave_socket, req, ALEN - 1)) != recv_nodata) {
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
            if (req[n - 1] == '\r') {
                req[n - 1] = '\0';
            }

            cout << "Command Received from Client: " << req << endl;
            vector<string> tokens = tokenize(string(req), " ");

            if (tokens.size() != 2) {
                sendMessage(0.0, "ERROR", "Malformed Command Received from Client!. To Exit, type: QUIT bye" );
            } else if (tokens[0] == "QUIT") {
                break;
            } else if (tokens[0] == "USER") {
                // TODO: How to handle multiple USER commands during a single session??
                if (tokens[1].find('/') != std::string::npos) {
                    // The name supplied by client contains '/', which isn't allowed
                    sendMessage(1.2, "ERROR USER", "The 'username' argument cannot contain '/'");
                } else {
                    user = tokens[1];
                    sendMessage(1.0, "HELLO", user.c_str());
                }
            } else if (tokens[0] == READ) {
                readMessageFromFile(stoi(tokens[1]), slave_socket);
            } else if (tokens[0] == WRITE or tokens[0] == REPLACE) {
                unsigned long peersCount = peersList.size();
                if (peersCount == 0) {
                    auto operation = tokens[0] == WRITE ? writeOperation : replaceMessageInFile;
                    string response = operation(user, tokens[1], false, NO_OPERATION);
                    send(slave_socket, response.c_str(), response.size(), 0);
                } else {
                    vector<pthread_t> peerThreads;

                    struct thread_info *tinfo = (thread_info*) calloc(peersCount, sizeof(*tinfo));
                    resetPeerCommunicationGlobalVars(peersCount);
                    for (int i = 0; i < peersCount; i++ ) {
                        string peer = peersList.at(i);
                        printf("Creating thread to communicate with %s\n", peer.c_str());

                        tinfo[i].peer = convertStringToCharArray(peer);
                        tinfo[i].user = convertStringToCharArray(user);
                        tinfo[i].req = req;
                        tinfo[i].secondArgumentToOperation = convertStringToCharArray(tokens[1]);

                        pthread_t tt;
                        if (pthread_create(&tt, nullptr, (void*(*)(void*))communicateWithPeer, &tinfo[i]) != 0) {
                            perror("pthread_create");
                        }
                        peerThreads.push_back(tt);
                    }

                    void* operationResponse;
                    for (pthread_t peerThread: peerThreads) {
                        pthread_join(peerThread, &operationResponse);
                    }
                    free(tinfo);
                    char* response = (char*)operationResponse;

                    if (strlen(response) == 0) {
                        sendMessage(3.2, joinTwoStringsWithDelimiter("ERROR", tokens[0].c_str(), ' '), "Syncronization Failed");
                    } else {
                        send(slave_socket, response, strlen(response), 0);
                    }
                    free(operationResponse);
                }
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
    bulletinBoardMasterSocket = createMasterSocket(bulletinBoardServerPort);
    printf("Bulletin Board Server up and listening on port %d.\n", bulletinBoardServerPort);

    createThreads(tmax, &handle_bulletin_board_client, (void*)bulletinBoardMasterSocket, bulletinBoardServerThreads);
}

void terminateBulletinBoardThreadsAndCloseMasterSocket() {
    killThreads(bulletinBoardServerThreads);
    close(bulletinBoardMasterSocket);
    cout << "All BulletinBoard Server Threads terminated." << endl;
}

void reconfigureGlobalVariablesAndRestartBoardServer(string configurationFile) {
    vector<string> newPeersList;

    readConfigurationParametersFromFile(configurationFile, tmax, bulletinBoardServerPort, ref_ignore<int>, ref_ignore<string>, newPeersList, ref_ignore<bool>, ref_ignore<bool>);
    if (!newPeersList.empty()) {
        peersList = newPeersList;
    }

    startBulletinBoardServer();
}

int board_server(char **argv) {
    bulletinBoardServerPort = atoi(argv[1]);
    tmax = atoi(argv[2]);

    int i = 3;
    while (argv[i] != nullptr) {
        peersList.emplace_back(argv[i]);
        i++;
    }

    startBulletinBoardServer();
    pthread_exit(nullptr);
}



