
#ifndef BULLETIN_BOARD_PEER_COMMUNICATION_H
#define BULLETIN_BOARD_PEER_COMMUNICATION_H

void* communicateWithPeer(void* arg);

void resetPeerCommunicationGlobalVars(pthread_t controllerThread, unsigned long numberOfPeers);

std::string getMasterOperationCompletionMessage(pthread_t controllerThread);

struct thread_info {    /* Used as argument to thread_start() */
    pthread_t thread_id;        /* ID returned by pthread_create() */
    pthread_t controller_thread_id;
    char *user;
    char *peer;
    char *req;
    char *secondArgumentToOperation;
};

#endif //BULLETIN_BOARD_PEER_COMMUNICATION_H
