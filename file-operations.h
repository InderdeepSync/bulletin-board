#include <iostream>
#include <vector>
#include <sys/wait.h>
#include <cstring>
#include <cstdio>
#include <pthread.h>

using namespace std;

#ifndef BULLETIN_BOARD_FILE_OPERATIONS_H
#define BULLETIN_BOARD_FILE_OPERATIONS_H

int get_initial_message_number();

void setBulletinBoardFile(string file);

string getBulletinBoardFile();

void writeToFile(const std::string& user, const std::string& message, int socketToRespond);

void readMessageFromFile(int messageNumberToRead, int socketToRespond);

int obtainLengthOfLineToBeReplaced(int messageNumberToReplace, int &totalBytesBeforeLineToReplace);

void optimalReplaceAlgorithm(std::string newUser, int messageNumberToReplace, std::string new_message);

void replaceMessageInFile(const std::string& user, const std::string& messageNumberAndMessage, int socketToSend);

#endif //BULLETIN_BOARD_FILE_OPERATIONS_H
