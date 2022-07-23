
#ifndef BULLETIN_BOARD_BOARD_SERVER_H
#define BULLETIN_BOARD_BOARD_SERVER_H

int initializeBoardServerGlobalVars(int port, int maxThreads, std::vector<std::string> peers);

void startBulletinBoardServer();

void reconfigureBoardServerGlobalVars(std::string configurationFile);

void terminateBulletinBoardThreadsAndCloseMasterSocket();

#endif //BULLETIN_BOARD_BOARD_SERVER_H
