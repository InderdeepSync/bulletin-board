#ifndef BULLETIN_BOARD_UTILITIES_H
#define BULLETIN_BOARD_UTILITIES_H

#include <iostream>
#include <vector>
#include <sys/wait.h>
#include <cstring>
#include <cstdio>
#include <pthread.h>

using namespace std;

void tokenize(std::string const &str, const char *delim,
              std::vector<std::string> &out);

int createSocket(string &valueRHOST, string &valueRPORT, int &socketId);

void cleanup_handler(void *arg );

void sendMessageToSocket(float code, char responseText[], char additionalInfo[], int socketToSend);

string createMessage(float code, char responseText[], char additionalInfo[]);

static const string bulletinBoardGreetingText = "Welcome to the Bulletin Board\n1.USER username\n2.READ msg_number\n3.WRITE text\n4.REPLACE msg_num/message\n5.QUIT exit_msg";

std::string trim(const std::string &s);

bool is_true(const string& value);

int createMasterSocket(int port);

void readConfigurationParametersFromFile(const string& configurationFile, int &tmax, int &bulletinBoardServerPort, int &syncServerPort, string &bbfile, vector<string> &peers, bool &isDaemon, bool &debuggingModeEnabled);

void killThreads(vector<pthread_t> &threadsToKill);

int createThreads(int numberOfThreads, void (*serverHandler)(int), void* handlerArgument, vector<pthread_t> &threadsCollection);

string readKeyFromConfigurationFile(string keyToRead, string configurationFile, string defaultValue);

static const string CONFIGURATION_FILE_BBFILE_KEY = "BBFILE";

static const string CONFIGURATION_FILE_DEBUG_KEY = "DEBUG";

static const string CONFIGURATION_FILE_SYNCPORT_KEY = "SYNCPORT";

#endif
