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

#include "board-server.h"
#include "tcp-utils.h"
#include "utilities.h"

using namespace std;

void sighup_handler(int signum) {
    cout << "Inside handler function for signal sighup." << endl;
}

void sigquit_handler(int signum) {
    cout << "Inside handler function for signal sigquit." << endl;
}

void handle_bulletin_board_client(int master_socket) {
    pthread_t currentThread = pthread_self();
    cout << "New Thread " << currentThread <<  " launched." << endl;

    sockaddr_in client_address{}; // the address of the client...
    unsigned int client_address_len = sizeof(client_address); // ... and its length

    while (true) {
        int slave_socket = accept(master_socket, (struct sockaddr *) &client_address, &client_address_len);
        if (slave_socket < 0) {
            if (errno == EINTR) {
                cout << "accept() interrupted! {Debugging Purposes}" << endl; // TODO: Clearly Examine this scenario upon cancellation/pthread_kill
                return; // continue maybe???
            }
            perror("accept");
            return;
        }

        const int ALEN = 256;
        char req[ALEN];
        int n;

        cout << "########## New Remote Client Accepted ##########" << endl;

        char* initialResponse = "0.0 greeting\n";
        send(slave_socket, initialResponse, strlen(initialResponse), 0);

        while ((n = readline(slave_socket, req, ALEN - 1)) != recv_nodata) {
            if (req[n - 1] == '\r') {
                req[n - 1] = '\0';
            }

            if (strcmp(req, "QUIT") == 0) {
                cout << "Received QUIT, terminating connection." << endl;
                break;
            }

            string inputCommand = req;
            cout << "Command Received from Client: " << inputCommand << endl;

            std::vector<std::string> tokens;
            tokenize(string(inputCommand), " ", tokens);

            // Further handling of various commands here.

            send(slave_socket, req, strlen(req), 0);
            send(slave_socket, "\n", 1, 0);
        }

        if (n == recv_nodata) {
            // read 0 bytes = EOF:
            cout << "Connection closed by client." << endl;
        }

        shutdown(slave_socket, 1);
        close(slave_socket);
        cout << "########## Remote Connection Terminated ##########" << endl;
    }
}

int main(int argc, char **argv, char *envp[]) {
    const int port = 9000;
    int tmax = 1;

    long int master_socket = passivesocket(port, 32);

    if (master_socket < 0) {
        perror("passive_socket");
        return 1;
    }

    int reuse;
    setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    cout << "Bulletin Board Server up and listening on port " << port << endl;

    // Setting up the thread creation:
    pthread_t tt;
    pthread_attr_t ta;
    pthread_attr_init(&ta);
    pthread_attr_setdetachstate(&ta, PTHREAD_CREATE_JOINABLE);

    for (int i = 0; i < tmax; i++) {
        if (pthread_create(&tt, &ta, (void *(*)(void *)) handle_bulletin_board_client, (void *) master_socket) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    cout << "Process ID: " << getpid() << endl;

    signal(SIGHUP, sighup_handler); // kill -HUP <Process ID>
//    signal(SIGINT, sigquit_handler); // kill -INT <Process ID> or Ctrl + C
    signal(SIGQUIT, sigquit_handler); // kill -QUIT <Process ID> or Ctrl + \ [Does not work on CLion for some reason]

    while(true) {
        sleep(1000);
    }
    return 0;
}



