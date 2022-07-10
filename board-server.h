
#ifndef BULLETIN_BOARD_BOARD_SERVER_H
#define BULLETIN_BOARD_BOARD_SERVER_H

int board_server(char **argv);

void reconfigureGlobalVariablesAndRestartBoardServer(std::string configurationFile);

void terminateBulletinBoardThreadsAndCloseMasterSocket();

#endif //BULLETIN_BOARD_BOARD_SERVER_H
