#include <pthread.h>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <functional>
#include <algorithm>
#include <map>

#include "peer-communication.h"
#include "file-operations.h"
#include "tcp-utils.h"
#include "utilities.h"
#include "logger.h"

using namespace std;

enum SyncMasterServerStatus {
    PRECOMMIT_SENT, COMMIT_SENT
};

class PeerCommunicationStatistics {
public:
    PeerCommunicationStatistics(){}

    unsigned long numberOfPositivePrecommitAcknowledgementsPending;
    bool shouldAbort = false;

    unsigned long numberOfSuccessfulCommitAcknowledgementsPending;
    bool didOneOrMorePeersFailToCommit = false;
    string masterOperationCompletionMessage;

    PeerCommunicationStatistics(unsigned long peersCount):
        numberOfPositivePrecommitAcknowledgementsPending(peersCount),
        numberOfSuccessfulCommitAcknowledgementsPending(peersCount) {}
};

std::map<pthread_t, PeerCommunicationStatistics> threadIdStatsMapping;

void resetPeerCommunicationGlobalVars(pthread_t controllerThread, unsigned long numberOfPeers) {
    threadIdStatsMapping.insert({controllerThread, PeerCommunicationStatistics(numberOfPeers)});
}

string getMasterOperationCompletionMessage(pthread_t controllerThread) {
    string result = threadIdStatsMapping[controllerThread].masterOperationCompletionMessage;
    threadIdStatsMapping.erase(controllerThread);
    return result;
}

pthread_mutex_t peerCommunicationMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t peerCommunicationCondition = PTHREAD_COND_INITIALIZER;

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
    char* command = tinfo->req;

    pthread_t controllerThread = tinfo->controller_thread_id;

    auto notifyThreadsToAbort = [controllerThread]() {
        pthread_mutex_lock(&peerCommunicationMutex);
        if (!threadIdStatsMapping[controllerThread].shouldAbort) {
            threadIdStatsMapping[controllerThread].shouldAbort = true;
            pthread_cond_broadcast(&peerCommunicationCondition);
        }
        pthread_mutex_unlock(&peerCommunicationMutex);
    };

    auto notifyThreadsToUndoCommit = [controllerThread]() {
        pthread_mutex_lock(&peerCommunicationMutex);
        if (!threadIdStatsMapping[controllerThread].didOneOrMorePeersFailToCommit) {
            threadIdStatsMapping[controllerThread].didOneOrMorePeersFailToCommit = true;
            pthread_cond_broadcast(&peerCommunicationCondition);
        }
        pthread_mutex_unlock(&peerCommunicationMutex);
    };

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
            threadIdStatsMapping[controllerThread].numberOfPositivePrecommitAcknowledgementsPending -= 1;
            if (threadIdStatsMapping[controllerThread].numberOfPositivePrecommitAcknowledgementsPending == 0) {
                pthread_cond_broadcast(&peerCommunicationCondition);
            } else {
                while(threadIdStatsMapping[controllerThread].numberOfPositivePrecommitAcknowledgementsPending > 0 and not threadIdStatsMapping[controllerThread].shouldAbort) {
                    pthread_cond_wait(&peerCommunicationCondition, &peerCommunicationMutex);
                }
            }
            pthread_mutex_unlock(&peerCommunicationMutex);
            if (threadIdStatsMapping[controllerThread].shouldAbort) { // Yes, we are accessing it without obtaining lock. It's not illegal.
                debug_printf("One or more peers timed out or weren't READY. Sending ABORT to peer %s.\n", peer);
                sendMessage(5.0, ABORT, "");
                break;
            } else {
                sendMessage(5.0, COMMIT, command);
                currentStatus = COMMIT_SENT;
                debug_printf("Proceeding to next phase. COMMIT Sent to peer %s\n", peer);
            }
        } else if (tokens[1] == COMMIT_SUCCESS and currentStatus == COMMIT_SENT) {
            debug_printf("Peer %s has positively acknowledged the COMMIT\n", peer);
//            if (peer == string("127.0.0.1:10002")) {
//                debug_sleep(13);
//            }
            pthread_mutex_lock(&peerCommunicationMutex);
            threadIdStatsMapping[controllerThread].numberOfSuccessfulCommitAcknowledgementsPending -= 1;
            if (threadIdStatsMapping[controllerThread].numberOfSuccessfulCommitAcknowledgementsPending == 0) {
                pthread_cond_broadcast(&peerCommunicationCondition);
            } else {
                while(threadIdStatsMapping[controllerThread].numberOfSuccessfulCommitAcknowledgementsPending > 0 and not threadIdStatsMapping[controllerThread].didOneOrMorePeersFailToCommit) {
                    debug_printf("Waiting for positive/negative acknowledgement for COMMIT or timeout from other peers.\n");
                    pthread_cond_wait(&peerCommunicationCondition, &peerCommunicationMutex);
                }
            }
            pthread_mutex_unlock(&peerCommunicationMutex);

            if (not threadIdStatsMapping[controllerThread].didOneOrMorePeersFailToCommit) {
                pthread_mutex_lock(&peerCommunicationMutex);
                if (threadIdStatsMapping[controllerThread].masterOperationCompletionMessage.empty()) {
                    debug_printf("Received COMMIT_SUCCESS from all peers. Performing Operation on Master Node\n");

                    vector<string> commandTokens = tokenize(command, " ");
                    auto operationToPerform = commandTokens[0] == WRITE ? writeOperation : replaceOperation;
                    threadIdStatsMapping[controllerThread].masterOperationCompletionMessage = operationToPerform(user, commandTokens[1], false, ref_ignore<function<void()>>);

                    debug_printf("Local Master Operation Completion Message => %s", threadIdStatsMapping[controllerThread].masterOperationCompletionMessage.c_str());
                }
                pthread_mutex_unlock(&peerCommunicationMutex);
            }
            bool didMasterOperationSucceed = threadIdStatsMapping[controllerThread].masterOperationCompletionMessage.find(ERROR) == string::npos;
            bool shouldSendPositiveConfirmationToPeers = not threadIdStatsMapping[controllerThread].didOneOrMorePeersFailToCommit and didMasterOperationSucceed;
            char* messageToSend = shouldSendPositiveConfirmationToPeers ? SUCCESS_NOOP : UNSUCCESS_UNDO;

            debug_printf("Transmitting latest local operation status %s to peer %s\n", messageToSend, peer);
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
