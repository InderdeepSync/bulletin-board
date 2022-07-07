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

using namespace std;

char *bulletin_board_file = "demo.txt";


int main() {
    string new_message = "SBBY";
    int messageNumberToReplace = 21;
    string newUser = "user";
}
