/*
* temporary definitions
*
* sem_init: This  manual  page  documents  POSIX  1003.1b semaphores, 
  not to be confused with SystemV semaphores as
  described in ipc(5), semctl(2) and semop(2).

wie posixtest zeigt ünterstützt Linux alles bis auf Posix Message Queues.

*/

#ifndef _PORTABLE_INCLUDED

#include <epicsTypes.h>
/* VxWorks Definitions */
#define OK 			0
#define ERROR 		(-1)
#define FALSE       0
#define TRUE            1

#define IMPORT 

typedef epicsInt8 		INT8;
typedef epicsUInt8		UINT8;
typedef epicsInt16		INT16;
typedef epicsUInt16		UINT16;
typedef epicsInt32		INT32;
typedef epicsUInt32		UINT32;
typedef epicsFloat32		FLOAT32;
typedef epicsFloat64		FLOAT64;

/* this is defined by VxWorks */
typedef       int             BOOL;

/* this is for linux */
typedef int STATUS;


#define _PORTABLE_INCLUDED
#endif
