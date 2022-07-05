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

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void sighup_handler(int signum) { // TODO: To be implemented
    cout << "Inside handler function for signal sighup." << endl;
}

void sigquit_handler(int signum) { // TODO: To be implemented
    cout << "Inside handler function for signal sigquit." << endl;
}

char* bulletin_board_file;
int message_number;
bool delayOperations;
vector<string> peersList;

class AccessData {
public:
    int num_readers_active;
    int num_writers_waiting;
    bool writer_active;
};

AccessData concurrencyManagementData = AccessData{};

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

void sendMessageToSocket(float code, char responseText[], char additionalInfo[], int socketToSend) {
    char buffer[255];
    memset(buffer, 0, sizeof buffer);
    snprintf(buffer, 255, "%2.1f %s %s\n", code, responseText, additionalInfo);
    send(socketToSend, buffer, sizeof(buffer), 0);
}

void writeToFile(const string& user, const string& message, int socketToRespond) {
    cout << "Message received from user: " << user << " => " << message << endl;

    size_t threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
    if (delayOperations) {
        cout << "Waiting to enter Critical Region. WRITE operation by thread - " << threadId
             << " pending!" << endl;
    }
    pthread_mutex_lock(&mut);
    concurrencyManagementData.num_writers_waiting += 1;

    while (concurrencyManagementData.num_readers_active > 0 or
           concurrencyManagementData.writer_active) {
        pthread_cond_wait(&cond, &mut);
    }
    concurrencyManagementData.num_writers_waiting -= 1;
    concurrencyManagementData.writer_active = true;
    pthread_mutex_unlock(&mut);

    if (delayOperations) {
        cout << "Entered Critical Region. WRITE operation by thread - " << threadId << " started!"
             << endl;
        sleep(6);
    }

    int fd = open(bulletin_board_file, O_WRONLY | O_APPEND,
                  S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);

    char message_line[255];
    memset(message_line, 0, sizeof message_line);
    snprintf(message_line, 255, "%d/%s/%s\n", message_number, user.c_str(), message.c_str());

    write(fd, message_line, strlen(message_line));
    close(fd);

    if (delayOperations) {
        cout << "Exiting Critical Region. WRITE operation by thread - " << threadId << " completed!"
             << endl;
    }
    pthread_mutex_lock(&mut);
    concurrencyManagementData.writer_active = false;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mut);

    sendMessageToSocket(3.0, "WROTE", const_cast<char*>(std::to_string(message_number++).c_str()), socketToRespond);
}

void readMessageFromFile(int messageNumberToRead, int socketToRespond) {
    if (messageNumberToRead >= message_number or messageNumberToRead < 0) {
        char additionalInfo[255];
        memset(additionalInfo, 0, sizeof additionalInfo);
        snprintf(additionalInfo, 255, "%d %s", messageNumberToRead, "The given message-number does not exist.");

        sendMessageToSocket(2.1, "UNKNOWN", additionalInfo, socketToRespond);
    } else {
        size_t threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
        if (delayOperations) {
            cout << "Waiting to enter Critical Region. READ operation by thread - " << threadId
                 << " pending!"
                 << endl;
        }
        pthread_mutex_lock(&mut);

        while (concurrencyManagementData.num_writers_waiting > 0 or
                concurrencyManagementData.writer_active) {
            pthread_cond_wait(&cond, &mut);
        }
        concurrencyManagementData.num_readers_active += 1;
        pthread_mutex_unlock(&mut);

        if (delayOperations) {
            cout << "Entered Critical Region. READ operation by thread - " << threadId << " started!"
                 << endl;
            sleep(3);
        }

        int fd = open(bulletin_board_file, O_RDONLY,
                      S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);
        // TODO: Handle failure in above gracefully, iff needed

        const int ALEN = 256;
        char temp[ALEN];
        int k = 0;
        while (readline(fd, temp, ALEN - 1) != recv_nodata) {
            if (k == messageNumberToRead) {
                break;
            }
            k++;
        }

        close(fd);

        if (delayOperations) {
            cout << "Exiting Critical Region. READ operation by thread - " << threadId << " completed!"
                 << endl;
        }
        pthread_mutex_lock(&mut);
        concurrencyManagementData.num_readers_active -= 1;
        if (concurrencyManagementData.num_readers_active == 0) {
            pthread_cond_broadcast(&cond);
        }
        pthread_mutex_unlock(&mut);

        for (int i = 0; temp[i] != '\0'; i++) {
            if (temp[i] == '/') {
                temp[i] = ' ';
                break;
            }
        }

        sendMessageToSocket(2.0, "MESSAGE", temp, socketToRespond);
    }
}

void replaceMessageInFile(const string& user, const string& messageNumberAndMessage, int socketToSend) {
    vector<string> replaceArguments;
    tokenize(messageNumberAndMessage, "/", replaceArguments);

    const int messageNumberToReplace = stoi(replaceArguments[0]);
    string new_message = replaceArguments[1];

    if (messageNumberToReplace >= message_number or messageNumberToReplace < 0) {
        sendMessageToSocket(3.1, "UNKNOWN", const_cast<char*> (std::to_string(messageNumberToReplace).c_str()), socketToSend);
    } else {
        size_t threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
        if (delayOperations) {
            cout << "Waiting to enter Critical Region. REPLACE operation by thread - " << threadId
                 << " pending!" << endl;
        }
        pthread_mutex_lock(&mut);
        concurrencyManagementData.num_writers_waiting += 1;

        while (concurrencyManagementData.num_readers_active > 0 or
               concurrencyManagementData.writer_active) {
            pthread_cond_wait(&cond, &mut);
        }
        concurrencyManagementData.num_writers_waiting -= 1;
        concurrencyManagementData.writer_active = true;
        pthread_mutex_unlock(&mut);

        if (delayOperations) {
            cout << "Entered Critical Region. REPLACE operation by thread - " << threadId << " started!"
                 << endl;
            sleep(6);
        }

        int fd = open(bulletin_board_file, O_RDONLY,
                      S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);
        int fd2 = open("tempfile", O_WRONLY | O_CREAT,
                       S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);

        const int ALEN = 256;
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

        if (delayOperations) {
            cout << "Exiting Critical Region. WRITE operation by thread - " << threadId << " completed!"
                 << endl;
        }
        pthread_mutex_lock(&mut);
        concurrencyManagementData.writer_active = false;
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mut);

        sendMessageToSocket(3.0, "WROTE", const_cast<char*> (std::to_string(messageNumberToReplace).c_str()), socketToSend);
    }
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

        char* initialResponse = "0.0 Welcome to the Bulletin Board\n1.USER username\n2.READ msg_number\n3.WRITE text\n4.REPLACE msg_num/message\n5.QUIT exit_msg\n";
        send(slave_socket, initialResponse, strlen(initialResponse), 0);

        auto sendMessage = [&](float code, char responseText[], char additionalInfo[]) {
            sendMessageToSocket(code, responseText, additionalInfo, slave_socket);
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
                // TODO: How to handle multiple USER commands during a single session??
                if (tokens[1].find('/') != std::string::npos) {
                    // The name supplied by client contains '/', which isn't allowed
                    sendMessage(1.2, "ERROR USER", "The 'username' argument cannot contain '/'");
                } else {
                    user = tokens[1];
                    sendMessage(1.0, "HELLO", const_cast<char *>(user.c_str()));
                }
            } else if (inputCommand.rfind("WRITE", 0) == 0) {
                writeToFile(user, tokens[1], slave_socket);
            } else if (inputCommand.rfind("READ", 0) == 0) {
                const int messageNumberToRead = stoi(tokens[1].c_str());
                readMessageFromFile(messageNumberToRead, slave_socket);
            } else if (inputCommand.rfind("REPLACE", 0) == 0) {
                replaceMessageInFile(user, tokens[1], slave_socket);
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

int board_server(char **argv) {
    const int port = atoi(argv[1]);
    delayOperations = strcmp(argv[2], "true") == 0;
    bulletin_board_file = argv[3];
    int tmax = atoi(argv[4]);

    int i = 5;
    while (argv[i] != nullptr) {
        peersList.emplace_back(argv[i]);
        i++;
    }

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

//int main(int argc, char **argv, char *envp[]) {
//    board_server(argv);
//}


