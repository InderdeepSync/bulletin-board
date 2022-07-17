#include <pthread.h>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <set>
#include <functional>
#include <algorithm>

#include "peer-communication.h"
#include "file-operations.h"
#include "tcp-utils.h"
#include "utilities.h"

using namespace std;

enum SyncMasterServerStatus {
    PRECOMMIT_SENT, COMMIT_SENT
};

unsigned long numberOfPositivePrecommitAcknowledgementsPending; bool shouldAbort = false;
unsigned long numberOfSuccessfulCommitAcknowledgementsPending; bool shouldUndoCommit = false;
bool operationPerformedOnMaster = false, didMasterOperationSucceed = false;

pthread_mutex_t peerCommunicationMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t peerCommunicationCondition = PTHREAD_COND_INITIALIZER;

int createSocket(string &valueRHOST, string &valueRPORT) {
    int socketId = connectbyport(valueRHOST.c_str(), valueRPORT.c_str());
    if (socketId == err_host) {
        cerr << "Cannot Find Host: " << valueRHOST << endl;
        return -1;
    }
    if (socketId < 0) {
        cout << "Connection refused by " << valueRHOST << ":" << valueRPORT << endl;
        return -1;
    }
    // we now have a valid, connected socket
    cout << "Successfully connected to peer " << valueRHOST << ":" << valueRPORT << " via socketId = "<< socketId << endl;
    return socketId;
}

void* communicateWithPeer(void* args[]) {
    vector<string> peerHostPort = tokenize((char*)args[0], ":");
    string peerHost = peerHostPort[0];
    string peerPort = peerHostPort[1];

    string user = (char*)args[1];
    string command = (char*)args[2];
    string secondArgumentToOperation = (char*) args[3];

    auto operationToPerform = command.find(WRITE) != string::npos ? writeOperation : replaceMessageInFile;
    string operationCompletionMessage;

    int socketId = createSocket(peerHost, peerPort);

    auto sendMessage = [&](float code, const char responseText[], const char additionalInfo[]) {
        sendMessageToSocket(code, responseText, additionalInfo, socketId);
    };

    sendMessage(5.0, PRECOMMIT, user.c_str());
    SyncMasterServerStatus currentStatus = PRECOMMIT_SENT;

    const int ALEN = 256;
    char response[ALEN];
    int n;
    while ((n = recv_nonblock(socketId, response, ALEN - 1, 2000)) != recv_nodata) {
        if (n == 0) {
            cout << "Connection closed by " + peerHost + ":" + peerPort << endl;
            break;
        }
        if (n < 0) {
            perror("recv_nonblock");
            break;
        }

        response[n - 1] = '\0';
        cout << "Response Received from Peer: " << response << endl;

        vector<string> tokens = tokenize(string(response), " ");

        if (tokens[1] == READY and currentStatus == PRECOMMIT_SENT) {
            pthread_mutex_lock(&peerCommunicationMutex);
            if (--numberOfPositivePrecommitAcknowledgementsPending == 0) {
                pthread_cond_broadcast(&peerCommunicationCondition);
            } else {
                while(numberOfPositivePrecommitAcknowledgementsPending > 0 and not shouldAbort) {
                    pthread_cond_wait(&peerCommunicationCondition, &peerCommunicationMutex);
                }
            }
            pthread_mutex_unlock(&peerCommunicationMutex);

            if (shouldAbort) { // Yes, we are accessing it without obtaining lock
                sendMessage(5.0, ABORT, "");
                break;
            } else {
                sendMessage(5.0, COMMIT, command.c_str());
                currentStatus = COMMIT_SENT;
            }
        } else if (tokens[1] == COMMIT_SUCCESS and currentStatus == COMMIT_SENT) {
            pthread_mutex_lock(&peerCommunicationMutex);
            if (--numberOfSuccessfulCommitAcknowledgementsPending == 0) {
                pthread_cond_broadcast(&peerCommunicationCondition);
            } else {
                while(numberOfSuccessfulCommitAcknowledgementsPending > 0 and not shouldUndoCommit) {
                    pthread_cond_wait(&peerCommunicationCondition, &peerCommunicationMutex);
                }
            }
            pthread_mutex_unlock(&peerCommunicationMutex);

            if (!shouldUndoCommit) {
                pthread_mutex_lock(&peerCommunicationMutex);
                if (!operationPerformedOnMaster) {
                    operationCompletionMessage = operationToPerform(user, secondArgumentToOperation, false, NO_OPERATION);
                    didMasterOperationSucceed = operationCompletionMessage.find(UNKNOWN) == string::npos;
                    operationPerformedOnMaster = true;
                }
                pthread_mutex_unlock(&peerCommunicationMutex);
            }

            char* messageToSend = !shouldUndoCommit and didMasterOperationSucceed ? SUCCESS_NOOP: UNSUCCESS_UNDO;
            sendMessage(5.0, messageToSend, "");

            break;
        } else if (tokens[1] == COMMIT_UNSUCCESS and currentStatus == COMMIT_SENT) {
            pthread_mutex_lock(&peerCommunicationMutex);
            if (!shouldUndoCommit){
                shouldUndoCommit = true;
                pthread_cond_broadcast(&peerCommunicationCondition);
            }
            pthread_mutex_unlock(&peerCommunicationMutex);
            break;
        }
    }

    if (n == recv_nodata) {
        cout << "Peer Request Timed out." << endl;
        pthread_mutex_lock(&peerCommunicationMutex);

        // We do not want to broadast in case either of the boolean flags is already true,
        // which implies that an earlier thread previously timed out and other threads have been notified.
        if (currentStatus == PRECOMMIT_SENT and !shouldAbort) {
            shouldAbort = true;
            pthread_cond_broadcast(&peerCommunicationCondition);
        } else if (currentStatus == COMMIT_SENT and !shouldUndoCommit) {
            shouldUndoCommit = true;
            pthread_cond_broadcast(&peerCommunicationCondition);
        }
        pthread_mutex_unlock(&peerCommunicationMutex);
    }

    shutdown(socketId, SHUT_RDWR);
    close(socketId);

    pthread_exit((void*)operationCompletionMessage.c_str());
}

void resetPeerCommunicationGlobalVars(unsigned long numberOfPeers) {
    numberOfPositivePrecommitAcknowledgementsPending = numberOfSuccessfulCommitAcknowledgementsPending = numberOfPeers;
    shouldAbort = shouldUndoCommit = false;

    operationPerformedOnMaster = didMasterOperationSucceed = false;
}
