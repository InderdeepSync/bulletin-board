
#ifndef BULLETIN_BOARD_SYNC_SERVER_H
#define BULLETIN_BOARD_SYNC_SERVER_H

void setSyncServerPort(int portToSet);

void startSyncServer();

void terminateSyncronizationThreadsAndCloseMasterSocket();

void reconfigureSyncServerPort(string configurationFile);

#endif //BULLETIN_BOARD_SYNC_SERVER_H
