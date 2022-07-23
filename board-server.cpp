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

int initializeBoardServerGlobalVars(int port, int maxThreads, vector<string> peers) {
    bulletinBoardServerPort = port;
    tmax = maxThreads;
    peersList = peers;
}

void* handle_bulletin_board_client(void* arg) {
    pthread_t threadId = pthread_self();
    printf("New Bulletin Board Thread %lu launched.\n", threadId);

    sockaddr_in client_address{}; // the address of the client...
    unsigned int client_address_len = sizeof(client_address); // ... and its length

    while (true) {
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
        int slave_socket = accept(bulletinBoardMasterSocket, (struct sockaddr *) &client_address, &client_address_len);
        if (slave_socket < 0) {
            if (errno == EINTR) {
                cout << "accept() interrupted! {Debugging Purposes}" << endl;
                return nullptr;
            }
            perror("accept");
            return nullptr;
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

        char residue = '\0';
        while ((n = readlineFromSocket(slave_socket, req, ALEN - 1, residue)) != recv_nodata) {
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);

            cout << "Command Received from Client: " << req << endl;
            vector<string> tokens = tokenize(req, " ");

            if (tokens.size() != 2) {
                sendMessage(0.0, "ERROR", "A Valid command must contain 2 tokens separated by ' '!. To Exit, type: QUIT bye");
            } else if (tokens[0] == "QUIT") {
                break;
            } else if (tokens[0] == "USER") {
                if (tokens[1].find('/') != std::string::npos or trim(tokens[1]).empty()) {
                    // The name supplied by client contains '/', which isn't allowed
                    sendMessage(1.2, "ERROR USER", "The 'username' argument must be non-empty & cannot contain '/'");
                } else {
                    user = tokens[1];
                    sendMessage(1.0, "HELLO", user.c_str());
                }
            } else if (tokens[0] == READ) {
                if (not is_number(tokens[1])) {
                    sendMessage(2.2, "ERROR READ", "Unable to parse given argument into a valid messageNumber");
                } else {
                    readMessageFromFile(stoi(tokens[1]), slave_socket);
                }
            } else if (tokens[0] == WRITE or tokens[0] == REPLACE) {
                if (tokens[0] == REPLACE ) {
                    auto result = areReplaceArgumentsValid(tokens[1]);
                    if (not result.first) {
                        string errorMessage = result.second;
                        send(slave_socket, errorMessage.c_str(), errorMessage.length(), 0);
                        continue;
                    }
                }

                unsigned long peersCount = peersList.size();
                if (peersCount == 0) {
                    auto operation = tokens[0] == WRITE ? writeOperation : replaceOperation;
                    string response = operation(user, tokens[1], false, ref_ignore<function<void()>>);
                    send(slave_socket, response.c_str(), response.size(), 0);
                } else {
                    struct thread_info *tinfo = (thread_info*) calloc(peersCount, sizeof(*tinfo));
                    resetPeerCommunicationGlobalVars(threadId, peersCount);
                    for (int i = 0; i < peersCount; i++) {
                        string peer = peersList.at(i);
                        printf("Creating thread to communicate with %s\n", peer.c_str());

                        tinfo[i].peer = convertStringToCharArray(peer);
                        tinfo[i].user = convertStringToCharArray(user);
                        tinfo[i].req = req;
                        tinfo[i].secondArgumentToOperation = convertStringToCharArray(tokens[1]);
                        tinfo[i].controller_thread_id = threadId;

                        if (pthread_create(&tinfo[i].thread_id, nullptr, &communicateWithPeer, &tinfo[i]) != 0) {
                            perror("pthread_create");
                        }
                    }

                    for (int i = 0; i < peersCount; i++) {
                        pthread_join(tinfo[i].thread_id, nullptr);
                    }
                    free(tinfo);

                    string response = getMasterOperationCompletionMessage(threadId);
                    if (response.empty()) {
                        sendMessage(3.2, joinTwoStringsWithDelimiter(ERROR, tokens[0].c_str(), ' '), "Syncronization Failed");
                    } else {
                        send(slave_socket, response.c_str(), response.length(), 0);
                    }
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

    createThreads(tmax, &handle_bulletin_board_client, bulletinBoardServerThreads);
}

void terminateBulletinBoardThreadsAndCloseMasterSocket() {
    killThreads(bulletinBoardServerThreads);
    close(bulletinBoardMasterSocket);
    cout << "All BulletinBoard Server Threads terminated." << endl;
}

void reconfigureBoardServerGlobalVars(string configurationFile) {
    vector<string> newPeersList;

    readConfigurationParametersFromFile(configurationFile, tmax, bulletinBoardServerPort, ref_ignore<int>, ref_ignore<string>, newPeersList, ref_ignore<bool>, ref_ignore<bool>);
    if (!newPeersList.empty()) {
        peersList = newPeersList;
    }
}
