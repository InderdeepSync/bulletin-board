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
    string peer = (char*)args[0];
    vector<string> peerHostPort = tokenize(peer, ":");
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
    printf("######### 2PC Started. Initial PRECOMMIT sent to peer %s #########\n", peer.c_str());

    const int ALEN = 256;
    char response[ALEN];
    int n;
    while ((n = recv_nonblock(socketId, response, ALEN - 1, 2000000)) != recv_nodata) { // TODO: Reset Timeouts to sane values
        if (n == 0) {
            printf("Connection closed by %s\n", peer.c_str());
            break;
        }
        if (n < 0) {
            perror("recv_nonblock");
            break;
        }

        response[n - 1] = '\0';
        printf("Response Received from Peer %s => %s\n", peer.c_str(), response);

        vector<string> tokens = tokenize(string(response), " ");

        if (tokens[1] == READY and currentStatus == PRECOMMIT_SENT) {
            printf("Peer %s is READY & acknowledged positively\n", peer.c_str());
            pthread_mutex_lock(&peerCommunicationMutex);
            if (--numberOfPositivePrecommitAcknowledgementsPending == 0) {
                pthread_cond_broadcast(&peerCommunicationCondition);
            } else {
                while(numberOfPositivePrecommitAcknowledgementsPending > 0 and not shouldAbort) {
                    printf("Waiting for response/timeout from other peers.\n");
                    pthread_cond_wait(&peerCommunicationCondition, &peerCommunicationMutex);
                }
            }
            pthread_mutex_unlock(&peerCommunicationMutex);

            if (shouldAbort) { // Yes, we are accessing it without obtaining lock. It's not illegal.
                printf("One or more peers timed out or weren't READY. Sending ABORT to peer %s.\n", peer.c_str());
                sendMessage(5.0, ABORT, "");
                break;
            } else {
                sendMessage(5.0, COMMIT, command.c_str());
                currentStatus = COMMIT_SENT;
                printf("Proceeding to next phase. COMMIT Sent to peer %s\n", peer.c_str());
            }
        } else if (tokens[1] == COMMIT_SUCCESS and currentStatus == COMMIT_SENT) {
            printf("Peer %s has positively acknowledged the COMMIT\n", peer.c_str());
            pthread_mutex_lock(&peerCommunicationMutex);
            if (--numberOfSuccessfulCommitAcknowledgementsPending == 0) {
                pthread_cond_broadcast(&peerCommunicationCondition);
            } else {
                while(numberOfSuccessfulCommitAcknowledgementsPending > 0 and not shouldUndoCommit) {
                    printf("Waiting for positive/negative acknowledgement for COMMIT or timeout from other peers.\n");
                    pthread_cond_wait(&peerCommunicationCondition, &peerCommunicationMutex);
                }
            }
            pthread_mutex_unlock(&peerCommunicationMutex);

            if (!shouldUndoCommit) {
                pthread_mutex_lock(&peerCommunicationMutex);
                if (!operationPerformedOnMaster) {
                    printf("Received COMMIT_SUCCESS from all peers. Performing Operation on Master Node\n");
                    operationCompletionMessage = operationToPerform(user, secondArgumentToOperation, false, NO_OPERATION);
                    didMasterOperationSucceed = operationCompletionMessage.find(UNKNOWN) == string::npos;
                    operationPerformedOnMaster = true;
                    printf("Operation %s on Master\n", didMasterOperationSucceed ? "executed succeeded" : "failed to execute");
                }
                pthread_mutex_unlock(&peerCommunicationMutex);
            }
            char* messageToSend = !shouldUndoCommit and didMasterOperationSucceed ? SUCCESS_NOOP: UNSUCCESS_UNDO;
            printf("Transmitting latest transaction status %s to peer %s\n", messageToSend, peer.c_str());
            sendMessage(5.0, messageToSend, "");

            break;
        } else if (tokens[1] == COMMIT_UNSUCCESS and currentStatus == COMMIT_SENT) {
            printf("Peer %s negatively acknowledged the COMMIT\n", peer.c_str());
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
        if (currentStatus == PRECOMMIT_SENT) {
            printf("Peer %s PRECOMMIT acknowledgemment timed out.\n", peer.c_str());
            if (!shouldAbort) {
                shouldAbort = true;
                pthread_cond_broadcast(&peerCommunicationCondition);
            }
        } else if (currentStatus == COMMIT_SENT) {
            printf("Peer %s COMMIT acknowledgement (positive/negative) timed out.\n", peer.c_str());
            if (!shouldUndoCommit) {
                shouldUndoCommit = true;
                pthread_cond_broadcast(&peerCommunicationCondition);
            }
        }
        pthread_mutex_unlock(&peerCommunicationMutex);
    }

    shutdown(socketId, SHUT_RDWR);
    close(socketId);
    printf("######### 2PC Protocol concluded #########\n");

    pthread_exit((void*)operationCompletionMessage.c_str());
}

void resetPeerCommunicationGlobalVars(unsigned long numberOfPeers) {
    numberOfPositivePrecommitAcknowledgementsPending = numberOfSuccessfulCommitAcknowledgementsPending = numberOfPeers;
    shouldAbort = shouldUndoCommit = false;

    operationPerformedOnMaster = didMasterOperationSucceed = false;
}
