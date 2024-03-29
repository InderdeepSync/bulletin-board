#include <iostream>
#include <vector>
#include <sys/wait.h>
#include <cstring>
#include <cstdio>
#include <pthread.h>
#include <functional>

using namespace std;

#ifndef BULLETIN_BOARD_FILE_OPERATIONS_H
#define BULLETIN_BOARD_FILE_OPERATIONS_H

int get_initial_message_number();

void setBulletinBoardFile(string file);

string getBulletinBoardFile();

void acquireWriteLock(string currentCommand);

void releaseWriteLock(string currentCommand);

string writeOperation(const string &user, const string &message, bool holdLock, function<void()> &undoWrite);

void readMessageFromFile(int messageNumberToRead, int socketToRespond);

int obtainLengthOfLineToBeReplaced(int messageNumberToReplace, int &totalBytesBeforeLineToReplace);

void optimalReplaceAlgorithm(string newUser, int messageNumberToReplace, string new_message);

pair<bool, string> areReplaceArgumentsValid(const string &messageNumberAndMessage);

string replaceOperation(const string &user, const string &messageNumberAndMessage, bool holdLock, function<void()> &undoReplace);

pair<string, string> getMessageNumberInfo(int messageNumber);

#endif //BULLETIN_BOARD_FILE_OPERATIONS_H
