Inderdeep Singh
isingh213@ubishops.ca
Student ID: 002308494

NOTE: Please refer to [protocol2pc.txt](protocol2pc.txt) for a detailed description of the synchronization protocol; it outlines the various possibilites in great detail, and explains how they've been adequately accounted for.

1) '-d' flag delays READ by three seconds, while it delays WRITE and REPLACE operations by 6 seconds. It also increases the verbosity of the server, printing additional debug logs for ease of administration.
By default, this is disabled.

2) Note that the timeout for threads spawned by master for communicating with the peers as well as the sync-server threads has been set to 7 sec, to prevent them from closing the connection in case debugging is enabled on the node they are communicating with.
We would not want the peer to timeout if the master takes 6 sec to perform the operation locally and vice-versa. Ideally, we should set these values to around 200 milliseconds.

3) Daemon mode is enabled by default. '-f' disables daemon mode, i.e. the Server process is NOT detached, allowing user to examine the operations of the server closely, and
standard output logs are displayed on the terminal window, instead of being redirected to bbserv.log.
Other considerations such as closing input/output descriptors, changing current working directory[In this case set to "." itself], putting the process in background, handling signals, severing the connection
from controlling tty, etc are also taken care of in daemon mode.

4) The file manipulation commands are pretty straightforward.

5) File Access Control is implemented using a mutex, a condition variable and a data structure encapsulating details such as the num_readers_active, num_writers_waiting, writer_active.
The code itself is write-preferring in nature. Code is written by referencing the pseudocode given at https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock.
The following is a direct translation from there for your reference.

'''

    struct AccessData {
        int num_readers_active;
        int num_writers_waiting;
        bool writer_active;
    };

    AccessData shared_statistics{};

    pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

    void *reader(int param) {
        pthread_mutex_lock(&mut);

        while(shared_statistics.num_writers_waiting > 0 or shared_statistics.writer_active) {
            pthread_cond_wait(&cond, &mut);
        }
        shared_statistics.num_readers_active += 1;
        pthread_mutex_unlock(&mut);

        printf("\n%d Reader is inside Critical Region.",param);
        sleep(3);
        printf("\n%d Reader is about to leave critical region.",param);

        pthread_mutex_lock(&mut);
        shared_statistics.num_readers_active -= 1;
        if (shared_statistics.num_readers_active == 0) {
            pthread_cond_broadcast(&cond);
        }
        pthread_mutex_unlock(&mut);


        return nullptr;
    }

    void *writer(int param) {
        pthread_mutex_lock(&mut);
        shared_statistics.num_writers_waiting += 1;

        while(shared_statistics.num_readers_active > 0 or shared_statistics.writer_active) {
            pthread_cond_wait(&cond, &mut);
        }
        shared_statistics.num_writers_waiting -= 1;
        shared_statistics.writer_active = true;
        pthread_mutex_unlock(&mut);

        printf("\n%d Writer is inside Critical Section", param);
        sleep(3);

        printf("\n%d writer is leaving critical region",param);


        pthread_mutex_lock(&mut);
        shared_statistics.writer_active = false;
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mut);

        return nullptr;
    }

'''

6) We allow the client to change their username mid-session by sending the USER command multiple times in a single session, which is not considered invalid.
By default, if no USER comand has been issued, the default name 'nobody' is assumed(Akin to browsing an application as a guest user).
Subsequent commands issued will take into account the latest username assumed by the client at the time.

7) Both accept() and read() are guaranteed cancellation points, hence pthread_cancel() is sufficient to cancel without much concern. However, in order to deallocate resources, a cleanup_handler is installed
that closes the socket when an active thread is cancelled[using pthread_cleanup_push and pthread_cleanup_pop]. This handler is not invoked under normal thread lifecycle.

8) The optimalReplaceAlgorithm() in file-operations.cpp has been designed to be ROBUST and highly EFFICIENT; It may even be overkill for a small application such as ours.

9) In order to prevent a current command sent by the client from being cancelled due to receipt of signals, we disable cancellation with pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr) upon receiving a command
and re-enable it later upon its completion with pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr). The

10) readlineFromSocket() utility function in tcp-utils.cpp allows us to handle any combination (of at least one character) of '\r' and '\n' as line terminator with.
This would handle not just '\r', '\n', '\r\n' and '\n\r', but any other conceivable terminator of variable length formed using these two characters.

11) debug_printf() in logger.cpp module has been designed to emulate and behavle exactly like printf(), but takes debuggingModeEnabled flag into consideration. Has been internally implemented using vprintf().

12) debug_sleep() in logger.cpp uses nanosleep() instead of sleep() as we only wish for the calling thread to sleep, not the entire process.

13) Each bulletin-board thread (tmax in number) can simultaneously initiate syncronization with peers. Hence the need for std::map<pthread_t, PeerCommunicationStatistics> threadIdStatsMapping in peer-communications.cpp,
which allows each board thread to have its own unique instance of PeerCommunicationStatistics. This is essential. An alternative approach would be to restrict the number of concurrrent syncronizations to 1 per node.

14) Condition variables have been used to ensure the 2PC protocol specification is adhered to and inter-thread communication is achieved. The code is intuitive and straightforward.

15) Timeout handling and abrupt socket close has been dutifully handled on both sides of the communication channel. enums SyncSlaveServerStatus and SyncMasterServerStatus allow us to perceive the entire process as a state machine.

16) The undo functionality is sleek and easy-to-comprehend. Before performing the operation (either write or replace), we are in a position to know what exactly state the file should be reset to. Instead of clunky and tedious alternate approaches,
we create a lambda function that knows exactly how to undo. Refer to writeOperation and replaceOperation in file-operations.cpp. Later on, we simply call it. It couldn't be any simpler. Note that we hold onto the writeLock until we receive SUCCESS_NOOP/UNSUCCESS_UNDO or a timeout occurs.
This is because we should not allow others to read/write until the state of the file has been finalized.


There is probably a lot more that could be mentioned here. I sincerely request you to read the code. I assure you the code is self-documenting.
I have attempted to incorporate the best programming design patterns and techniques I know of. Any variables global have been kept so for a reason.
Various functionalitites are segregated into distinct modules and functions. Code readability and reuse is given special emphasis.



