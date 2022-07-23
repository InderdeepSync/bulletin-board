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

#include "tcp-utils.h"
#include "utilities.h"
#include "sync-server.h"
#include "file-operations.h"
#include "logger.h"

using namespace std;

vector<pthread_t> syncServerThreads;
long int syncronizationMasterSocket;

const int NUMBER_OF_SYNCRONIZATION_THREADS = 1;
int syncServerPort;

void setSyncServerPort(int portToSet) {
    syncServerPort = portToSet;
}

enum SyncSlaveServerStatus {
    IDLE, PRECOMMIT_ACKNOWLEDGED, AWAITING_SUCCESS_OR_UNDO_BROADCAST
};

void* handle_sync_server_client(void* arg) {
    printf("New Syncronization Thread %lu launched.\n", pthread_self());

    sockaddr_in client_address{}; // the address of the client...
    unsigned int client_address_len = sizeof(client_address); // ... and its length

    while (true) {
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
        int slave_socket = accept(syncronizationMasterSocket, (struct sockaddr *) &client_address, &client_address_len);
        if (slave_socket < 0) {
            if (errno == EINTR) {
                cout << "accept() interrupted! {Debugging Purposes}" << endl;
                return nullptr;
            }
            perror("accept");
            return nullptr;
        }
        pthread_cleanup_push(cleanup_handler, &slave_socket);

        cout << "########## Communication Channel with Master Established ##########" << endl;

        auto sendMessage = [&](float code, const char responseText[], const char additionalInfo[]) {
            return sendMessageToSocket(code, responseText, additionalInfo, slave_socket);
        };

        SyncSlaveServerStatus currentStatus = IDLE;
        std::function<void()> undoCommitOperation = [](){};
        string operationPerformed;
        string user;

        const int ALEN = 256;
        char req[ALEN];
        int n;

        while ((n = recv_nonblock(slave_socket, req, ALEN - 1, 12000)) != recv_nodata) {
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
            if (n == 0) {
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

            printf("Command Received from Master Node: %s\n", req);
            vector<string> tokens = tokenize(req, " ");

            if (tokens[1] == PRECOMMIT and currentStatus == IDLE) {
                user = tokens[2];
                sendMessage(5.0, READY, "User stored. Syncronization Server available.");
                printf("PRECOMMIT acknowledged positively. Response to Master => 5.0 READY\n");

                currentStatus = PRECOMMIT_ACKNOWLEDGED;
            } else if (tokens[1] == ABORT and currentStatus == PRECOMMIT_ACKNOWLEDGED) {
                printf("Aborting Transaction. No further action needed.\n");
                break;
            } else if (tokens[1] == COMMIT and currentStatus == PRECOMMIT_ACKNOWLEDGED) {
                auto operation = tokens[2] == WRITE ? writeOperation : replaceOperation;
                string response = operation(user, tokens[3], true, undoCommitOperation);

                bool operationFailed = response.find(ERROR) != string::npos;
                string responseText = operationFailed ? COMMIT_UNSUCCESS : COMMIT_SUCCESS;

                sendMessage(5.0, responseText.c_str(), response.c_str());
                if (operationFailed) {
                    break;
                }

                printf("Requested Command COMMITted to disk successfully. Response to Master => 5.0 %s %s\n", responseText.c_str(), response.c_str());
                operationPerformed = tokens[2];
                currentStatus = AWAITING_SUCCESS_OR_UNDO_BROADCAST;
            } else if (tokens[1] == SUCCESS_NOOP and currentStatus == AWAITING_SUCCESS_OR_UNDO_BROADCAST) {
                releaseWriteLock(operationPerformed);
                printf("Received Success confirmation from Master. Releasing Write lock.\n");
                break;
            } else if (tokens[1] == UNSUCCESS_UNDO and currentStatus == AWAITING_SUCCESS_OR_UNDO_BROADCAST) {
                undoCommitOperation();
                releaseWriteLock(operationPerformed);
                printf("%s was successfully undone. Releasing Write lock.\n", operationPerformed.c_str());
                break;
            } else {
                // Invalid Command received from Peer.
            }

            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
        }

        if (n == recv_nodata or n == 0) {
            const char *errorReason = n == 0 ? "abruptly closed" : "timed out";
            const char *expectedResponse = currentStatus == IDLE ? PRECOMMIT : currentStatus == PRECOMMIT_ACKNOWLEDGED
                                                                               ? joinTwoStringsWithDelimiter(ABORT, COMMIT, '/')
                                                                               : joinTwoStringsWithDelimiter(SUCCESS_NOOP, UNSUCCESS_UNDO, '/');
            debug_printf("Socket Connection with Master %s. A %s message was expected.\n", errorReason,
                         expectedResponse);

            if (currentStatus == IDLE or currentStatus == PRECOMMIT_ACKNOWLEDGED) {

            } else if (currentStatus == AWAITING_SUCCESS_OR_UNDO_BROADCAST) {
                undoCommitOperation();
                releaseWriteLock(operationPerformed);
            }
        }

        shutdown(slave_socket, SHUT_RDWR);
        close(slave_socket);
        pthread_cleanup_pop(0);

        printf("########## Communication Channel with Master Closed ##########\n");
    }
}

void startSyncServer() {
    syncronizationMasterSocket = createMasterSocket(syncServerPort);
    printf("Syncronization Server up and listening on port %d.\n", syncServerPort);

    createThreads(NUMBER_OF_SYNCRONIZATION_THREADS, &handle_sync_server_client, syncServerThreads);
}

void terminateSyncronizationThreadsAndCloseMasterSocket() {
    killThreads(syncServerThreads);
    close(syncronizationMasterSocket);
    cout << "All Syncronization Server Threads terminated." << endl;
}

void reconfigureSyncServerPort(string configurationFile) {
    syncServerPort = stoi(readKeyFromConfigurationFile(CONFIGURATION_FILE_SYNCPORT_KEY, configurationFile, to_string(syncServerPort)));
}
