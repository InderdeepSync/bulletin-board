
#ifndef BULLETIN_BOARD_SYNC_SERVER_H
#define BULLETIN_BOARD_SYNC_SERVER_H

int sync_server(char **argv);

void terminateSyncronizationThreadsAndCloseMasterSocket();

void reconfigureGlobalVariablesAndRestartSyncServer(string configurationFile);

#endif //BULLETIN_BOARD_SYNC_SERVER_H
