#include <iostream>
#include <vector>
#include <sys/wait.h>
#include <cstring>
#include <cstdio>
#include <pthread.h>

using namespace std;

#ifndef BULLETIN_BOARD_FILE_OPERATIONS_H
#define BULLETIN_BOARD_FILE_OPERATIONS_H

void set_initial_message_number(string file);

int get_initial_message_number();

void writeToFile(string file,const std::string& user, const std::string& message, int socketToRespond);

void readMessageFromFile(string file, int messageNumberToRead, int socketToRespond);

int obtainLengthOfLineToBeReplaced(string file, int messageNumberToReplace, int &totalBytesBeforeLineToReplace);

void optimalReplaceAlgorithm(string file, std::string newUser, int messageNumberToReplace, std::string new_message);

void replaceMessageInFile(string file, const std::string& user, const std::string& messageNumberAndMessage, int socketToSend);

#endif //BULLETIN_BOARD_FILE_OPERATIONS_H
