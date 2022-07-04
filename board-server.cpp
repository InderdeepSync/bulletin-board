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

char* bulletin_board_file = "bbfile"; // TODO: Will be set while parsing command-line arguments
int message_number;

int obtain_initial_message_number() {
    const int ALEN = 256;
    char req[ALEN];

    int fd = open(bulletin_board_file, O_CREAT | O_RDONLY,
                  S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);
    int l = 0;
    while (readline(fd, req, ALEN - 1) != recv_nodata) {
        l++;
    }
    close(fd);
    return l;
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
                return;
            }
            perror("accept");
            return;
        }

        cout << "########## New Remote Client Accepted ##########" << endl;

        char* initialResponse = "0.0 Welcome to the Bulletin Board\n1.USER username\n2.READ msg_number\n3.WRITE text\n4.REPLACE msg_num/message\n";
        send(slave_socket, initialResponse, strlen(initialResponse), 0);

        auto sendMessage = [&](float code, char responseText[], char additionalInfo[]) {
            char buffer[255];
            memset(buffer, 0, sizeof buffer);
            snprintf(buffer, 255, "%2.1f %s %s\n", code, responseText, additionalInfo);
            send(slave_socket, buffer, sizeof(buffer), 0);
        };

        const int ALEN = 256;
        char req[ALEN];
        int n;

        string user = "nobody";

        while ((n = readline(slave_socket, req, ALEN - 1)) != recv_nodata) {
            if (req[n - 1] == '\r') {
                req[n - 1] = '\0';
            }

            string inputCommand = req;
            cout << "Command Received from Client: " << inputCommand << endl;

            std::vector<std::string> tokens;
            tokenize(string(inputCommand), " ", tokens);

            if (inputCommand.rfind("QUIT", 0) == 0) {
                break;
            } else if (inputCommand.rfind("USER", 0) == 0) {
                if (tokens[1].find('/') != std::string::npos) {
                    // The name supplied by client contains '/', which isn't allowed
                    sendMessage(1.2, "ERROR USER", "The 'username' argument cannot contain '/'");
                } else {
                    user = tokens[1];
                    sendMessage(1.0, "HELLO", const_cast<char *>(user.c_str()));
                }
            } else if (inputCommand.rfind("WRITE", 0) == 0) {
                cout << "Message received from user: " << user << " => " << tokens[1] << endl;

                int fd = open(bulletin_board_file, O_WRONLY | O_APPEND,
                              S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);

                char message_line[255];
                memset(message_line, 0, sizeof message_line);
                snprintf(message_line, 255, "%d/%s/%s\n", message_number, user.c_str(), tokens[1].c_str());

                write(fd, message_line, strlen(message_line));
                close(fd);

                sendMessage(3.0, "WROTE", const_cast<char*>(std::to_string(message_number++).c_str()));
            } else if (inputCommand.rfind("READ", 0) == 0) {
                const int messageNumberToRead = stoi(tokens[1].c_str());

                if (messageNumberToRead >= message_number or messageNumberToRead < 0) {
                    char additionalInfo[255];
                    memset(additionalInfo, 0, sizeof additionalInfo);
                    snprintf(additionalInfo, 255, "%d %s", messageNumberToRead, "The given message-number does not exist.");

                    sendMessage(2.1, "UNKNOWN", additionalInfo);
                } else {
                    int fd = open(bulletin_board_file, O_RDONLY,
                                  S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);
                    // TODO: Handle failure in above gracefully, iff needed

                    char temp[ALEN];
                    int k = 0;
                    while (readline(fd, temp, ALEN - 1) != recv_nodata) {
                        if (k == messageNumberToRead) {
                            break;
                        }
                        k++;
                    }

                    close(fd);
                    for (int i = 0; temp[i] != '\0'; i++) {
                        if (temp[i] == '/') {
                            temp[i] = ' ';
                            break;
                        }
                    }

                    sendMessage(2.0, "MESSAGE", temp);
                }
            } else if (inputCommand.rfind("REPLACE", 0) == 0) {
                vector<string> replaceArguments;
                tokenize(tokens[1], "/", replaceArguments);

                const int messageNumberToReplace = stoi(replaceArguments[0]);
                string new_message = replaceArguments[1];

                if (messageNumberToReplace >= message_number or messageNumberToReplace < 0) {
                    sendMessage(3.1, "UNKNOWN", const_cast<char*> (std::to_string(messageNumberToReplace).c_str()));
                } else {
                    int fd = open(bulletin_board_file, O_RDONLY,
                                  S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);
                    int fd2 = open("tempfile", O_WRONLY | O_CREAT,
                                   S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);

                    int l = 0; char textLine[ALEN];

                    while (readline(fd, textLine, ALEN - 1) != recv_nodata) {
                        vector<string> messageTokens;
                        tokenize(string(textLine), "/", messageTokens);

                        if (l == messageNumberToReplace) {
                            char lineToStore[4096];
                            memset(lineToStore, 0, sizeof lineToStore);
                            snprintf(lineToStore, 4096, "%s/%s/%s", messageTokens[0].c_str(), user.c_str(), new_message.c_str());

                            write(fd2, lineToStore, strlen(lineToStore));
                        } else {
                            write(fd2, textLine, strlen(textLine));
                        }
                        write(fd2, "\n", 1);
                        l++;
                    }

                    close(fd);
                    unlink(bulletin_board_file);
                    close(fd2);
                    rename("tempfile", bulletin_board_file);

                    sendMessage(3.0, "WROTE", const_cast<char*> (std::to_string(messageNumberToReplace).c_str()));
                }
            } else {
                sendMessage(0.0, "ERROR", "Invalid Command Entered!");
            }
        }

        if (n == recv_nodata) {
            // read 0 bytes = EOF:
            cout << "Connection closed by client." << endl;
        }

        sendMessage(4.0, "BYE", "This is Goodbye.");
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

    message_number = obtain_initial_message_number();

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



