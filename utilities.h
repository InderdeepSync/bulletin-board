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

std::string trim(const std::string &s);

bool is_true(const string& value);

#endif
