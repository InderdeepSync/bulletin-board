
#ifndef BULLETIN_BOARD_BOARD_SERVER_H
#define BULLETIN_BOARD_BOARD_SERVER_H

int board_server(char **argv);

void bulletin_board_sighup_handler(std::string configurationFile);

void bulletin_board_sigquit_handler(int signum);

#endif //BULLETIN_BOARD_BOARD_SERVER_H
