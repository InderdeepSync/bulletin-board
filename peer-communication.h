
#ifndef BULLETIN_BOARD_PEER_COMMUNICATION_H
#define BULLETIN_BOARD_PEER_COMMUNICATION_H

void* communicateWithPeer(void* arg);

void resetPeerCommunicationGlobalVars(unsigned long numberOfPeers);

std::string getMasterOperationCompletionMessage();

struct thread_info {    /* Used as argument to thread_start() */
    pthread_t thread_id;        /* ID returned by pthread_create() */
    char *user;
    char *peer;
    char *req;
    char *secondArgumentToOperation;
};

#endif //BULLETIN_BOARD_PEER_COMMUNICATION_H
