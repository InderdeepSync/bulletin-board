#include <pthread.h>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <set>
#include <functional>
#include <algorithm>
#include <iterator>
#include <sstream>

#include "peer-communication.h"
#include "file-operations.h"
#include "tcp-utils.h"
#include "utilities.h"
#include "logger.h"

using namespace std;

enum SyncMasterServerStatus {
    PRECOMMIT_SENT, COMMIT_SENT
};

unsigned long numberOfPositivePrecommitAcknowledgementsPending; bool shouldAbort = false;
unsigned long numberOfSuccessfulCommitAcknowledgementsPending; bool didOneOrMorePeersFailToCommit = false; //TODO: Check detach state of all pthread_create calls
string masterOperationCompletionMessage; // TODO: Ensure all global vars in all modules are accessed in a protected manner

void resetPeerCommunicationGlobalVars(unsigned long numberOfPeers) {
    numberOfPositivePrecommitAcknowledgementsPending = numberOfSuccessfulCommitAcknowledgementsPending = numberOfPeers;
    shouldAbort = didOneOrMorePeersFailToCommit = false;

    masterOperationCompletionMessage.clear();
}

string getMasterOperationCompletionMessage() {
    return masterOperationCompletionMessage;
}

pthread_mutex_t peerCommunicationMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t peerCommunicationCondition = PTHREAD_COND_INITIALIZER;

void notifyThreadsToAbort() {
    pthread_mutex_lock(&peerCommunicationMutex);
    if (!shouldAbort) {
        shouldAbort = true;
        pthread_cond_broadcast(&peerCommunicationCondition);
    }
    pthread_mutex_unlock(&peerCommunicationMutex);
}

void notifyThreadsToUndoCommit() {
    pthread_mutex_lock(&peerCommunicationMutex);
    if (!didOneOrMorePeersFailToCommit) {
        didOneOrMorePeersFailToCommit = true;
        pthread_cond_broadcast(&peerCommunicationCondition);
    }
    pthread_mutex_unlock(&peerCommunicationMutex);
}

int createSocket(string peerHost, string peerPort) {
    int socketId = connectbyport(peerHost.c_str(), peerPort.c_str());
    if (socketId == err_host) {
        cerr << "Cannot Find Host: " << peerHost << endl;
        return -1;
    }
    if (socketId < 0) {
        cout << "Connection refused by " << peerHost << ":" << peerPort << endl;
        return -1;
    }
    // we now have a valid, connected socket
    debug_printf("Successfully connected to peer %s:%s via socketId = %d\n", peerHost.c_str(), peerPort.c_str(), socketId);
    return socketId;
}

void* communicateWithPeer(void* arg) {
    struct thread_info *tinfo = (thread_info*)arg;
    char* peer = tinfo->peer;
    vector<string> peerHostPort = tokenize(peer, ":");
    string peerHost = peerHostPort[0];
    string peerPort = peerHostPort[1];

    string user = tinfo->user;
    string command = tinfo->req;
    string secondArgumentToOperation = tinfo->secondArgumentToOperation;

    int socketId = createSocket(peerHost, peerPort);
    if (socketId == -1) {
        notifyThreadsToAbort();
        return nullptr;
    }

    auto sendMessage = [&](float code, const char responseText[], const char additionalInfo[]) {
        return sendMessageToSocket(code, responseText, additionalInfo, socketId);
    };

    sendMessage(5.0, PRECOMMIT, user.c_str());
    SyncMasterServerStatus currentStatus = PRECOMMIT_SENT;
    debug_printf("######### 2PC Started. Initial PRECOMMIT sent to peer %s #########\n", peer);

    const int ALEN = 256;
    char response[ALEN];
    int n;
    while ((n = recv_nonblock(socketId, response, ALEN - 1, 7000)) != recv_nodata) {
        if (n == 0) {
            break;
        }
        if (n < 0) {
            perror("recv_nonblock");
            break;
        }

        response[n - 1] = '\0';
        debug_printf("Response Received from Peer %s => %s\n", peer, response);

        vector<string> tokens = tokenize(response, " ");

        if (tokens[1] == READY and currentStatus == PRECOMMIT_SENT) {
            debug_printf("Peer %s is READY & acknowledged positively\n", peer);
            pthread_mutex_lock(&peerCommunicationMutex);
            if (--numberOfPositivePrecommitAcknowledgementsPending == 0) {
                pthread_cond_broadcast(&peerCommunicationCondition);
            } else {
                while(numberOfPositivePrecommitAcknowledgementsPending > 0 and not shouldAbort) {
                    debug_printf("Peer %s now waiting for response/timeout from other peers.\n", peer);
                    pthread_cond_wait(&peerCommunicationCondition, &peerCommunicationMutex);
                }
            }
            pthread_mutex_unlock(&peerCommunicationMutex);
//            if (peer == string("127.0.0.1:10002")) {
//                debug_sleep(9);
//            }
            if (shouldAbort) { // Yes, we are accessing it without obtaining lock. It's not illegal.
                debug_printf("One or more peers timed out or weren't READY. Sending ABORT to peer %s.\n", peer);
                sendMessage(5.0, ABORT, "");
                break;
            } else {
                sendMessage(5.0, COMMIT, command.c_str());
                currentStatus = COMMIT_SENT;
                debug_printf("Proceeding to next phase. COMMIT Sent to peer %s\n", peer);
            }
        } else if (tokens[1] == COMMIT_SUCCESS and currentStatus == COMMIT_SENT) {
            debug_printf("Peer %s has positively acknowledged the COMMIT\n", peer);
            pthread_mutex_lock(&peerCommunicationMutex);
            if (--numberOfSuccessfulCommitAcknowledgementsPending == 0) {
                pthread_cond_broadcast(&peerCommunicationCondition);
            } else {
                while(numberOfSuccessfulCommitAcknowledgementsPending > 0 and not didOneOrMorePeersFailToCommit) {
                    debug_printf("Waiting for positive/negative acknowledgement for COMMIT or timeout from other peers.\n");
                    pthread_cond_wait(&peerCommunicationCondition, &peerCommunicationMutex);
                }
            }
            pthread_mutex_unlock(&peerCommunicationMutex);

            if (not didOneOrMorePeersFailToCommit) {
                pthread_mutex_lock(&peerCommunicationMutex);
                if (masterOperationCompletionMessage.empty()) {
                    debug_printf("Received COMMIT_SUCCESS from all peers. Performing Operation on Master Node\n");

                    auto operationToPerform = command.find(WRITE) != string::npos ? writeOperation : replaceOperation;
                    masterOperationCompletionMessage = operationToPerform(user, secondArgumentToOperation, false, ref_ignore<function<void()>>);

                    debug_printf("Local Master Operation Completion Message => %s\n", masterOperationCompletionMessage.c_str());
                }
                pthread_mutex_unlock(&peerCommunicationMutex);
            }
            bool didMasterOperationSucceed = masterOperationCompletionMessage.find(ERROR) == string::npos;
            bool shouldUndoCommit = didOneOrMorePeersFailToCommit or not didMasterOperationSucceed;
            char* messageToSend = shouldUndoCommit ? UNSUCCESS_UNDO : SUCCESS_NOOP;

            debug_printf("Transmitting latest local operation status %s to peer %s", messageToSend, peer);
            sendMessage(5.0, messageToSend, "");

            break;
        } else if (tokens[1] == COMMIT_UNSUCCESS and currentStatus == COMMIT_SENT) {
            debug_printf("Peer %s negatively acknowledged the COMMIT\n", peer);

            notifyThreadsToUndoCommit();
            break;
        }
    }

    if (n == recv_nodata or n == 0) {
        const char* errorReason = n == 0 ? "abruptly closed" : "timed out";
        const char* expectedResponse = currentStatus == PRECOMMIT_SENT ? READY : joinTwoStringsWithDelimiter(COMMIT_SUCCESS, COMMIT_UNSUCCESS, '/');
        debug_printf("Socket Connection with peer %s %s. A %s message was expected.\n", peer, errorReason, expectedResponse);

        if (currentStatus == PRECOMMIT_SENT) {
            notifyThreadsToAbort();
        } else if (currentStatus == COMMIT_SENT) {
            notifyThreadsToUndoCommit();
        }
    }

    shutdown(socketId, SHUT_RDWR);
    close(socketId);
    printf("######### Connection with %s closed. #########\n", peer);
}
