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

int obtainLengthOfLineToBeReplaced(int messageNumberToReplace, int &totalBytesBeforeLineToReplace) {
    int fileDescriptor = open(bulletin_board_file,  O_RDONLY,
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

void optimalReplaceAlgorithm(string newUser, int messageNumberToReplace, string new_message) {
    char lineToStore[4096];
    memset(lineToStore, 0, sizeof lineToStore);
    snprintf(lineToStore, 4096, "%d/%s/%s\n", messageNumberToReplace, newUser.c_str(), new_message.c_str());

    int totalBytesBeforeLineToReplace = 0;
    int lengthOfLineToBeReplaced = obtainLengthOfLineToBeReplaced(messageNumberToReplace, totalBytesBeforeLineToReplace);
    if (lengthOfLineToBeReplaced == strlen(lineToStore)) {
        // Case 1: Old and New line are of same length
        int fd3 = open(bulletin_board_file,  O_WRONLY,
                       S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);

        lseek(fd3, totalBytesBeforeLineToReplace, SEEK_SET);
        write(fd3, lineToStore, strlen(lineToStore));

        close(fd3);
    } else {
        int differenceInLength = strlen(lineToStore) - lengthOfLineToBeReplaced;

        int fd1 = open(bulletin_board_file, O_RDONLY,
                       S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH);
        int fd2 = open(bulletin_board_file, O_WRONLY,
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

int main() {
    string new_message = "SBBY";
    int messageNumberToReplace = 21;
    string newUser = "user";

    optimalReplaceAlgorithm(newUser, messageNumberToReplace, new_message);
}
