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

//char *bulletin_board_file = "demo.txt";
//bool delayOperations;

//extern int testVariable;
extern int message_number;

//vector<pthread_t> aliveThreads;

void sync_server_sigquit_handler(int signum) {

}

void sync_server_sighup_handler(int signum) {

}

void handle_sync_server_client(int master_socket) {
    pthread_t currentThread = pthread_self();
    cout << "New Thread " << currentThread <<  " launched." << endl;

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

            std::vector<std::string> tokens;
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

        shutdown(slave_socket, 1);
        close(slave_socket);
        pthread_cleanup_pop(0);

        cout << "########## Remote Connection Terminated ##########" << endl;
    }
}

int sync_server(char **argv) {
    const int port = 10000;
//    delayOperations = strcmp(argv[2], "true") == 0;
//    bulletin_board_file = argv[3];
    int tmax = 3;

    long int master_socket = passivesocket(port, 32);

    if (master_socket < 0) {
        perror("passive_socket");
        return 1;
    }

    int reuse;
    setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    cout << "Syncronization Server up and listening on port " << port << endl;

    // Setting up the thread creation:
    pthread_t tt;
    pthread_attr_t ta;
    pthread_attr_init(&ta);
    pthread_attr_setdetachstate(&ta, PTHREAD_CREATE_JOINABLE);

//    message_number = obtain_initial_message_number();
    cout << "Process ID: " << getpid() << endl;

    for (int i = 0; i < tmax; i++) {
        if (pthread_create(&tt, &ta, (void *(*)(void *)) handle_sync_server_client, (void *) master_socket) != 0) {
            perror("pthread_create");
            return 1;
        }
//        aliveThreads.push_back(tt);
    }

    pthread_exit(nullptr);
}

int main(int argc, char **argv, char *envp[]) {
    sync_server(argv);
}
