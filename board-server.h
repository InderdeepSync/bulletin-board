
#ifndef BULLETIN_BOARD_BOARD_SERVER_H
#define BULLETIN_BOARD_BOARD_SERVER_H

int board_server(int port, int maxThreads, std::vector<std::string> peers);

void reconfigureGlobalVariablesAndRestartBoardServer(std::string configurationFile);

void terminateBulletinBoardThreadsAndCloseMasterSocket();

#endif //BULLETIN_BOARD_BOARD_SERVER_H
