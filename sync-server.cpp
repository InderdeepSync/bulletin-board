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

#include "tcp-utils.h"
#include "utilities.h"
#include "sync-server.h"
#include "file-operations.h"
#include "logger.h"

using namespace std;

vector<pthread_t> syncServerThreads;
long int syncronizationMasterSocket;

const int NUMBER_OF_SYNCRONIZATION_THREADS = 3;
int syncServerPort;

enum SyncSlaveServerStatus {
    IDLE, PRECOMMIT_ACKNOWLEDGED, AWAITING_SUCCESS_OR_UNDO_BROADCAST
};

void handle_sync_server_client(int master_socket) {
    pthread_t currentThread = pthread_self();
    cout << "New Syncronization Thread " << currentThread <<  " launched." << endl;

    sockaddr_in client_address{}; // the address of the client...
    unsigned int client_address_len = sizeof(client_address); // ... and its length

    while (true) {
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
        int slave_socket = accept(master_socket, (struct sockaddr *) &client_address, &client_address_len);
        if (slave_socket < 0) {
            if (errno == EINTR) {
                cout << "accept() interrupted! {Debugging Purposes}" << endl;
                return;
            }
            perror("accept");
            return;
        }
        pthread_cleanup_push(cleanup_handler, &slave_socket);

        cout << "########## Communication Channel with Peer Established ##########" << endl;

        auto sendMessage = [&](float code, char responseText[], char additionalInfo[]) {
            sendMessageToSocket(code, responseText, additionalInfo, slave_socket);
        };

        SyncSlaveServerStatus currentStatus = IDLE;
        std::function<void()> undoCommitOperation = [](){};
        string operationPerformed;
        string user;

        const int ALEN = 256;
        char req[ALEN];
        int n;

        while ((n = recv_nonblock(slave_socket, req, ALEN - 1, 10000)) != recv_nodata) {
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
            if (n == 0) {
                cout << "Connection closed by Master Node." << endl;
                break;
            }
            if (n < 0) {
                perror("recv_nonblock");
                break;
            }

            // NOTE: rec_nonblock does not get rid of \n, unlike readline.
            req[n - 1] = '\0'; // Replace last character '\n' with the null byte
            if (req[n - 2] == '\r') {
                req[n - 2] = '\0';
            }

            string inputCommand = req;
            cout << "Command Received from Client: " << inputCommand << endl;

            vector<string> tokens;
            tokenize(inputCommand, " ", tokens);

            if (inputCommand.rfind("PRECOMMIT", 0) == 0 && currentStatus == IDLE) {
                user = tokens[1];
                sendMessage(5.0, "READY", "User stored. Syncronization Server available.");

                currentStatus = PRECOMMIT_ACKNOWLEDGED;
            } else if (inputCommand.rfind("ABORT", 0) == 0 && currentStatus == PRECOMMIT_ACKNOWLEDGED) {
                break;
            } else if (inputCommand.rfind("COMMIT", 0) == 0 && currentStatus == PRECOMMIT_ACKNOWLEDGED) {
                if (tokens[1] == "WRITE") {
                    acquireWriteLock("WRITE");
                    string response = writeOperation(user, tokens[2], undoCommitOperation);
                    sendMessage(5.0, "COMMIT_SUCCESS", const_cast<char*>(response.c_str()));

                    operationPerformed = "WRITE";
                } else if (tokens[1] == "REPLACE") {
                    string response = replaceMessageInFile(user, tokens[2], true, undoCommitOperation);

                    bool replaceCommandFailed = response.find("UNKNOWN") != string::npos;
                    string responseText =  replaceCommandFailed ? "COMMIT_UNSUCCESS" : "COMMIT_SUCCESS";

                    sendMessage(5.0, const_cast<char*>(responseText.c_str()), const_cast<char*>(response.c_str()));
                    if (replaceCommandFailed) {
                        break;
                    }

                    operationPerformed = "REPLACE";
                }

                currentStatus = AWAITING_SUCCESS_OR_UNDO_BROADCAST;
            } else if (inputCommand.rfind("SUCCESS_NOOP", 0) == 0 && currentStatus == AWAITING_SUCCESS_OR_UNDO_BROADCAST) {
                releaseWriteLock(operationPerformed);
                break;
            } else if (inputCommand.rfind("UNSUCCESS_UNDO", 0) == 0 && currentStatus == AWAITING_SUCCESS_OR_UNDO_BROADCAST) {
                undoCommitOperation();
                releaseWriteLock(operationPerformed);
                break;
            } else {
                // Invalid Command received from Peer.
            }

            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
        }
        // If timeout occured, check which state am I in. Based on that, take action // RElease lock if held
        if (n == recv_nodata) {
            if (currentStatus == IDLE or currentStatus == PRECOMMIT_ACKNOWLEDGED) {

            } else if (currentStatus == AWAITING_SUCCESS_OR_UNDO_BROADCAST) {
                undoCommitOperation();
                releaseWriteLock(operationPerformed);
            }
        }

        shutdown(slave_socket, SHUT_RDWR);
        close(slave_socket);
        pthread_cleanup_pop(0);

        cout << "########## Remote Connection Terminated ##########" << endl;
    }
}

void startSyncServer() {
    syncronizationMasterSocket = createMasterSocket(syncServerPort);
    cout << "Syncronization Server up and listening on port " << syncServerPort << endl;

    createThreads(NUMBER_OF_SYNCRONIZATION_THREADS, &handle_sync_server_client, (void*)syncronizationMasterSocket, syncServerThreads);
}

void terminateSyncronizationThreadsAndCloseMasterSocket() {
    killThreads(syncServerThreads);
    close(syncronizationMasterSocket);
    cout << "All Syncronization Server Threads terminated." << endl;
}

void reconfigureGlobalVariablesAndRestartSyncServer(string configurationFile) {
    syncServerPort = stoi(readKeyFromConfigurationFile(CONFIGURATION_FILE_SYNCPORT_KEY, configurationFile, to_string(syncServerPort)));

    startSyncServer();
}

int sync_server(char **argv) {
    syncServerPort = atoi(argv[1]);

    startSyncServer();
    pthread_exit(nullptr);
}

//int main(int argc, char **argv, char *envp[]) {
//    setBulletinBoardFile("bbfile");
//    setDebuggingPreference(false);
//    sync_server(argv);
//}
