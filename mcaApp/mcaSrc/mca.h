/* mca.h -- communication between record and device support for mca record */

#ifndef mcaH
#define mcaH

typedef struct {
   double acquiring;
   double realTime;
   double liveTime;
   double totalCounts;
   double dwellTime;
} mcaAsynAcquireStatus;

typedef enum { 
   MSG_ACQUIRE,
   MSG_READ,
   MSG_SET_CHAS_INT,
   MSG_SET_CHAS_EXT,
   MSG_SET_NCHAN,
   MSG_SET_DWELL,
   MSG_SET_REAL_TIME,
   MSG_SET_LIVE_TIME,
   MSG_SET_COUNTS,
   MSG_SET_LO_CHAN,
   MSG_SET_HI_CHAN,
   MSG_SET_NSWEEPS,
   MSG_SET_MODE_PHA,
   MSG_SET_MODE_MCS,
   MSG_SET_MODE_LIST,
   MSG_GET_ACQ_STATUS,
   MSG_STOP_ACQUISITION,
   MSG_ERASE,
   MSG_SET_SEQ,
   MSG_SET_PSCL
} mcaCommand;

#endif /* mcaH */
