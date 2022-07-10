#include <array>
#include <bits/stdc++.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <map>
#include <pthread.h>
#include <cstdio>
#include <string>
#include <cstring>
#include <vector>
#include <iostream>
#include <thread>
#include <set>
#include <functional>
#include <algorithm>
#include <cassert>

#include "file-operations.h"
#include "utilities.h"
#include "tcp-utils.h"
#include "logger.h"

using namespace std;

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int message_number;

class AccessData {
public:
    int num_readers_active;
    int num_writers_waiting;
    bool writer_active;
};

void set_initial_message_number(string file) {
    const int ALEN = 256;
    char req[ALEN];

    int fd = open(file.c_str(), O_CREAT | O_RDONLY,
                  S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);
    int l = 0;
    while (readline(fd, req, ALEN - 1) != recv_nodata) {
        l++;
    }
    close(fd);
    message_number = l;
}

int get_initial_message_number() {
    return message_number;
}

AccessData concurrencyManagementData = AccessData{};

void writeToFile(string file, const string& user, const string& message, int socketToRespond) {
    cout << "Message received from user: " << user << " => " << message << endl;

    size_t threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());

    char msg[256];
    memset(msg, 0, sizeof msg);
    snprintf(msg, 255, "Waiting to enter Critical Region. WRITE operation by thread - %zu pending!", threadId);
    logDebugMessage(msg, 0);

    pthread_mutex_lock(&mut);
    concurrencyManagementData.num_writers_waiting += 1;

    while (concurrencyManagementData.num_readers_active > 0 or
           concurrencyManagementData.writer_active) {
        pthread_cond_wait(&cond, &mut);
    }
    concurrencyManagementData.num_writers_waiting -= 1;
    concurrencyManagementData.writer_active = true;
    pthread_mutex_unlock(&mut);

    memset(msg, 0, sizeof msg);
    snprintf(msg, 255, "Entered Critical Region. WRITE operation by thread - %zu started!", threadId);
    logDebugMessage(msg, 6);

    int fd = open(file.c_str(), O_WRONLY | O_APPEND,
                  S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);

    char message_line[255];
    memset(message_line, 0, sizeof message_line);
    snprintf(message_line, 255, "%d/%s/%s\n", message_number++, user.c_str(), message.c_str());

    write(fd, message_line, strlen(message_line));
    close(fd);

    memset(msg, 0, sizeof msg);
    snprintf(msg, 255, "Exiting Critical Region. WRITE operation by thread - %zu completed!", threadId);
    logDebugMessage(msg, 0);

    pthread_mutex_lock(&mut);
    concurrencyManagementData.writer_active = false;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mut);

    sendMessageToSocket(3.0, "WROTE", const_cast<char*>(std::to_string(message_number - 1).c_str()), socketToRespond);
}

void readMessageFromFile(string file, int messageNumberToRead, int socketToRespond) {
    if (messageNumberToRead >= message_number or messageNumberToRead < 0) {
        char additionalInfo[255];
        memset(additionalInfo, 0, sizeof additionalInfo);
        snprintf(additionalInfo, 255, "%d %s", messageNumberToRead, "The given message-number does not exist.");

        sendMessageToSocket(2.1, "UNKNOWN", additionalInfo, socketToRespond);
    } else {
        size_t threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());

        char msg[256];
        memset(msg, 0, sizeof msg);
        snprintf(msg, 255, "Waiting to enter Critical Region. READ operation by thread - %zu pending!", threadId);
        logDebugMessage(msg, 0);

        pthread_mutex_lock(&mut);

        while (concurrencyManagementData.num_writers_waiting > 0 or
               concurrencyManagementData.writer_active) {
            pthread_cond_wait(&cond, &mut);
        }
        concurrencyManagementData.num_readers_active += 1;
        pthread_mutex_unlock(&mut);

        memset(msg, 0, sizeof msg);
        snprintf(msg, 255, "Entered Critical Region. READ operation by thread - %zu started!", threadId);
        logDebugMessage(msg, 3);

        int fd = open(file.c_str(), O_RDONLY,
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

        memset(msg, 0, sizeof msg);
        snprintf(msg, 255, "Exiting Critical Region. READ operation by thread - %zu completed!", threadId);
        logDebugMessage(msg, 0);

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

int obtainLengthOfLineToBeReplaced(string file, int messageNumberToReplace, int &totalBytesBeforeLineToReplace) {
    int fileDescriptor = open(file.c_str(),  O_RDONLY,
                              S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);

    const int ALEN = 256;
    int n, l = 0; char textLine[ALEN];

    while ((n = readline(fileDescriptor, textLine, ALEN - 1)) != recv_nodata) {
        if (l == messageNumberToReplace) {
            close(fileDescriptor);
            return n + 1;
        } else if (l < messageNumberToReplace) {
            totalBytesBeforeLineToReplace += (n + 1);
        }
        l++;
    }
    assert(false); // Will Never happen anyway.
}

void optimalReplaceAlgorithm(string file, string newUser, int messageNumberToReplace, string new_message) {
    char lineToStore[4096];
    memset(lineToStore, 0, sizeof lineToStore);
    snprintf(lineToStore, 4096, "%d/%s/%s\n", messageNumberToReplace, newUser.c_str(), new_message.c_str());

    int totalBytesBeforeLineToReplace = 0;
    int lengthOfLineToBeReplaced = obtainLengthOfLineToBeReplaced(file, messageNumberToReplace, totalBytesBeforeLineToReplace);
    if (lengthOfLineToBeReplaced == strlen(lineToStore)) {
        // Case 1: Old and New line are of same length
        int fd3 = open(file.c_str(),  O_WRONLY,
                       S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);

        lseek(fd3, totalBytesBeforeLineToReplace, SEEK_SET);
        write(fd3, lineToStore, strlen(lineToStore));

        close(fd3);
    } else {
        int differenceInLength = strlen(lineToStore) - lengthOfLineToBeReplaced;

        int fd1 = open(file.c_str(), O_RDONLY,
                       S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);
        int fd2 = open(file.c_str(), O_WRONLY,
                       S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);

        if (differenceInLength < 0) {
            // Case 2: Shift Backwards and Truncate
            lseek(fd1, totalBytesBeforeLineToReplace + lengthOfLineToBeReplaced, SEEK_SET);

            lseek(fd2, totalBytesBeforeLineToReplace, SEEK_SET);
            write(fd2, lineToStore, strlen(lineToStore));
            long currentWriterPosition = lseek(fd2, 0, SEEK_CUR);

            struct stat fileStatistics;
            fstat(fd2, &fileStatistics);

            char readOutput[256];
            int fileSizeAfterReplacement = fileStatistics.st_size + differenceInLength;

            while (currentWriterPosition < fileSizeAfterReplacement) {
                memset(readOutput, 0, sizeof readOutput);
                read(fd1, readOutput, 1);
                write(fd2, readOutput, 1);

                currentWriterPosition = lseek(fd2, 0, SEEK_CUR);
            }

            ftruncate(fd2, fileSizeAfterReplacement);
        } else {
            // Case 3: Shift Forward
            long currentReaderPosition = lseek(fd1, 0 - 1, SEEK_END);
            lseek(fd2, differenceInLength - 1, SEEK_END);

            char readOutput[256];

            int temp = totalBytesBeforeLineToReplace + lengthOfLineToBeReplaced;
            while (currentReaderPosition >= temp) {
                memset(readOutput, 0, sizeof readOutput);
                read(fd1, readOutput, 1);
                write(fd2, readOutput, 1);

                currentReaderPosition = lseek(fd1, -2, SEEK_CUR);
                lseek(fd2, -2, SEEK_CUR);
            }

            lseek(fd2, totalBytesBeforeLineToReplace, SEEK_SET);
            write(fd2, lineToStore, strlen(lineToStore));
        }

        close(fd1);
        close(fd2);
    }
}

void replaceMessageInFile(string file, const string& user, const string& messageNumberAndMessage, int socketToSend) {
    vector<string> replaceArguments;
    tokenize(messageNumberAndMessage, "/", replaceArguments);

    const int messageNumberToReplace = stoi(replaceArguments[0]);
    string new_message = replaceArguments[1];

    if (messageNumberToReplace >= message_number or messageNumberToReplace < 0) {
        sendMessageToSocket(3.1, "UNKNOWN", const_cast<char*> (std::to_string(messageNumberToReplace).c_str()), socketToSend);
    } else {
        size_t threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());

        char msg[256];
        memset(msg, 0, sizeof msg);
        snprintf(msg, 255, "Waiting to enter Critical Region. REPLACE operation by thread - %zu pending!", threadId);
        logDebugMessage(msg, 0);

        pthread_mutex_lock(&mut);
        concurrencyManagementData.num_writers_waiting += 1;

        while (concurrencyManagementData.num_readers_active > 0 or
               concurrencyManagementData.writer_active) {
            pthread_cond_wait(&cond, &mut);
        }
        concurrencyManagementData.num_writers_waiting -= 1;
        concurrencyManagementData.writer_active = true;
        pthread_mutex_unlock(&mut);

        memset(msg, 0, sizeof msg);
        snprintf(msg, 255, "Entered Critical Region. REPLACE operation by thread - %zu started!", threadId);
        logDebugMessage(msg, 6);

        optimalReplaceAlgorithm(file, user, messageNumberToReplace, new_message);

        memset(msg, 0, sizeof msg);
        snprintf(msg, 255, "Exiting Critical Region. REPLACE operation by thread - %zu completed!", threadId);
        logDebugMessage(msg, 0);

        pthread_mutex_lock(&mut);
        concurrencyManagementData.writer_active = false;
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mut);

        sendMessageToSocket(3.0, "WROTE", const_cast<char*> (std::to_string(messageNumberToReplace).c_str()), socketToSend);
    }
}