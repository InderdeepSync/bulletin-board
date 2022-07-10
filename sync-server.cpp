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

using namespace std;

vector<pthread_t> syncServerThreads;
long int syncronizationMasterSocket;

const int NUMBER_OF_SYNCRONIZATION_THREADS = 3;
int syncServerPort;

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

        cout << "########## Communication Channel with Peer Established ##########" << client_address.sin_addr.s_addr << endl;

        const int ALEN = 256;
        char req[ALEN];
        const char* ack = "ACK: ";
        int n;

        while ((n = readline(slave_socket, req, ALEN - 1)) != recv_nodata) {
            if (req[n - 1] == '\r') {
                req[n - 1] = '\0';
            }

            string inputCommand = req;
            cout << "Command Received from Client: " << inputCommand << endl;

            vector<string> tokens;
            tokenize(inputCommand, " ", tokens);

            if (inputCommand.rfind("QUIT", 0) == 0) {
                break;
            }

            send(slave_socket,ack,strlen(ack),0);
            send(slave_socket,req,strlen(req),0);
            send(slave_socket,"\n",1,0);
        }

        if (n == recv_nodata) {
            // read 0 bytes = EOF:
            cout << "Connection closed by client." << endl;
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

    createThreads(NUMBER_OF_SYNCRONIZATION_THREADS, handle_sync_server_client, (void*)syncronizationMasterSocket, syncServerThreads);
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
//    sync_server(argv);
//}
