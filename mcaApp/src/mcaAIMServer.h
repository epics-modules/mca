//mcaAIMServer.h

/*
    Author: Mark Rivers
    Date: 04-Sept-2000

*/

#ifndef mcaAIMServerH
#define ip330SweepServerH

enum ip330SweepCommands {cmdErase, cmdStartAcquire, cmdStopAcquire, cmdRead,
                         cmdGetStatus, cmdSetGain, cmdSetDwell, cmdSetNChans,
                         cmdSetLiveTime};

#define numIp330SweepOffsets 3
enum ip330SweepOffsets {statusOffset, elapsedTimeOffset, dwellTimeOffset};

#endif //ip330SweepH
