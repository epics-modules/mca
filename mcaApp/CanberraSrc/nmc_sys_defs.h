/* NMC_SYS_DEFS.H */

/*******************************************************************************
*
* This file defines constants and structures used by system programs controlling
* networked front-end modules.
*
* The structures and values defined here are independent of the AIM firmware.
* Thus, developers are free to modify this file without introducing problems
* with communicating with the AIM.
*
********************************************************************************
*
* Revision History:
*
*       19-Dec-1993  MLR  Modified from Nuclear Data code
*       25-Sept-1996 TMM  Added "from" argument to nmc_signal()
*       06-May-2000  MLR  Changed order of include files, use < > for system
*                         include files.
*       03-Sept-2000 MLR  Added interlocks per module rather than just global
*                         Made response queue per module rather than global
*                         Added nmc_broadcast_inq_task().
*******************************************************************************/

#include <epicsTypes.h>
#include <ellLib.h>
#include <epicsMessageQueue.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <epicsThread.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef vxWorks
#include <vxWorks.h>
#include <msgQLib.h>
#include <etherLib.h>
#include <semLib.h>
#else
#include <libnet.h>
#include <pcap.h> 		/* Packet Capturing: if this gives you an error try pcap/pcap.h */
#endif
#include <errlog.h>


#include "aim_comm_defs.h"
#include "ncp_comm_defs.h"
#include "nmcmsgdef.h"


#ifdef vxWorks
#else
/* VxWorks has these definitions */
#define OK 			0
#define ERROR 		(-1)
#define FALSE       0
#define TRUE            1

#define IMPORT 

#ifndef _EPICS_AIM_NMC_TYPE_REDEFINITIONS
typedef epicsInt8 		INT8;
typedef epicsUInt8		UINT8;
typedef epicsInt16		INT16;
typedef epicsUInt16		UINT16;
typedef epicsInt32		INT32;
typedef epicsUInt32		UINT32;
typedef epicsFloat32		FLOAT32;
typedef epicsFloat64		FLOAT64;
#define _EPICS_AIM_NMC_TYPE_REDEFINITIONS
#endif /* _EPICS_AIM_NMC_TYPE_REDEFINITIONS */

/* this is defined by VxWorks */
typedef       int             BOOL;
typedef int STATUS;
#endif /* vxWorks */

#define AIM_DEBUG(l,FMT,V...) {if (l <= aimDebug) \
              { errlogPrintf("%s(%d):",__FILE__,__LINE__); \
                            errlogPrintf(FMT,##V);}}
extern volatile int aimDebug;

#define NMC_K_MAX_MODULES 64                    /* we can know about 64 modules */
#define NMC_K_CAPTURESIZE  2048                 /* Linux pcap Capture Buffer Size*/

/*
* This structure contains information concerning the state of networked modules
* known to the system.
*/

struct nmc_module_info_struct {
   char valid;                         /* allocation flag for this module number */
   char module_ownership_state;        /* module ownership state */
   char module_comm_state;             /* module communications status */
   char module_type;                   /* module type */
   struct nmc_comm_info_struct *comm_device; /* Pointer to network link to module */
   unsigned char address[6];           /* module network address */
   unsigned char owner_id[6];          /* owner id of module */
   char owner_name[8];                 /* owner name of module */
   unsigned char num_of_inputs;        /* number of inputs provided by module */
   unsigned int input_usage_map;       /* bits are 0 if an input is free */
   unsigned int acq_mem_size;          /* acquisition memory size in bytes */
   unsigned int free_address;          /* first free address in acquisition memory (8k blocks for AIM) */
   unsigned char inqmsg_counter;       /* counter of unanswered inquiry messages */
   unsigned char current_message_number; /* counter of current message number */
   unsigned char hw_revision;          /* module hardware rev level */
   unsigned char fw_revision;          /* module firmware rev level */
   unsigned char xmt_errors;           /* transmit error counter */
   unsigned char rcv_errors;           /* receive error counter */
   unsigned char timeout_errors;       /* timeout error counter */
   unsigned short int message_counter; /* total messages sent/received */
   epicsMessageQueueId responseQ;      /* message queue for response messages */
   epicsMutexId module_mutex;          /* Mutual exclusion semaphore */
   struct response_packet *in_pkt;     /* Input packet buffer */
   struct response_packet *out_pkt;    /* Output packet buffer */
};

/*
* Define nmc_module_info_struct.module_ownership_state codes
*/

#define NMC_K_MOS_UNKNOWN 0         /* module state is unknown */
#define NMC_K_MOS_UNOWNED 1         /* module is not owned by us */
#define NMC_K_MOS_NOTBYUS 2         /* module is owned, but not by us */
#define NMC_K_MOS_OWNEDBYUS 3       /* module is owned by us */
#define NMC_K_MOS_BEINGBOUGHT 4     /* we're in the process of buying the module */

/*
* Define nmc_module_info_struct.module_comm_state codes
*/

#define NMC_K_MCS_UNKNOWN 0         /* unknown */
#define NMC_K_MCS_UNREACHABLE 1     /* module is unreachable */
#define NMC_K_MCS_REACHABLE 2       /* module is reachable */

#ifdef vxWorks
#else
/* ifnet: linux version is different from VxWorks */
struct ifnet {
        char    *if_name;               /* interface name, e.g. eth0 */
		short   if_unit;				/* not used, always 0 */
		struct  libnet_link_int *netlnk;	/* netlnk struct for packet injection */
		struct  ether_addr *hw_address;     /* Ethernet Mac-Address of our device */
};
/* vxWorks has this defined, Linux also
struct  ether_header {
        epicsUInt8  ether_dhost[6];
        epicsUInt8  ether_shost[6];
        epicsUInt16 ether_type;
}; */
#endif
/*
* This structure contains data relating to the communications channel over
* which we talk to networked modules.
*/

struct nmc_comm_info_struct {
   char valid;                    /* structure entry valid */
   char type;                     /* Network device type */
   char name[10];                 /* Network device name */
   unsigned char sys_address[6];  /* System network address */
   struct ifnet *pIf;             /* Pointer to ifnet structure */
   epicsThreadId status_pid;       /* Thread ID of status dispatch */
   epicsThreadId broadcast_pid;    /* Thread ID of broadcast poller  */
#ifdef vxWorks
#else
   epicsThreadId capture_pid;      /* Thread ID of ether capture thread */
#endif
/* this was the no of ticks for timeout, it is float seconds now*/
   float timeout_time;            /* time in s */
   int header_size;               /* The size of the device dependent header */
   int max_msg_size;              /* Largest possible message size */
   int max_tries;                 /* Number of command retries allowed */
   epicsMessageQueueId statusQ;   /* Message queue for status messages */
   unsigned char response_sap;    /* NI SAP address for normal messages */
   unsigned char status_sap;      /* NI SAP address for status/event messages */
   unsigned char response_snap[5]; /* NI SNAP ID for normal messages */
   unsigned char status_snap[5];  /* NI SNAP ID for status/event messages */
};

#define NMC_K_MAX_IDS 4                         /* Number of network devices */

#define NMC_K_DTYPE_ETHERNET 1                  /* Ethernet network type */

/* Define parameters for the message queues used to pass Ethernet messages */
#define MAX_RESPONSE_Q_MSG_SIZE sizeof(struct response_packet)
/*The following assumes status packet is bigger than event packet */
#define MAX_STATUS_Q_MSG_SIZE   sizeof(struct status_packet)
#define MAX_RESPONSE_Q_MESSAGES 5
#define MAX_STATUS_Q_MESSAGES   24

/* Definitions for the linked list of semaphores for event messages */
struct nmc_sem_node
   {
   struct nmc_sem_node *next;
   struct nmc_sem_node *previous;
   int module;
   int adc;
   int event_type;
   epicsEventId evt_id;
   };
   
/* Interlock access to global variables. */
#define GLOBAL_INTERLOCK_ON epicsMutexLock(nmc_global_mutex)
#define GLOBAL_INTERLOCK_OFF epicsMutexUnlock(nmc_global_mutex)

#define MODULE_INTERLOCK_ON(module) \
        if (epicsMutexLock(nmc_module_info[module].module_mutex)) \
           AIM_DEBUG(1, "MODULE_INTERLOCK_ON failed!\n")
#define MODULE_INTERLOCK_OFF(module) \
        epicsMutexUnlock(nmc_module_info[module].module_mutex)


/* Function prototypes */

/* These routines are in nmc_comm_subs_1.c */
void nmc_cleanup();
IMPORT STATUS nmc_initialize(char *device);
IMPORT STATUS nmcStatusDispatch(struct nmc_comm_info_struct *net);
IMPORT STATUS nmc_status_hdl(struct nmc_comm_info_struct *net,
                             struct status_packet *pkt);
IMPORT STATUS nmc_owner_hdl(int module, struct ncp_comm_header *p);
IMPORT STATUS nmc_getmsg(int module, struct response_packet *pkt, int size, int *actual);
IMPORT STATUS nmc_flush_input(int module);
IMPORT STATUS nmc_putmsg(int module, struct response_packet *pkt, int size);
IMPORT STATUS nmc_sendcmd(int module, int command, void *data, int dsize,
                       void *response, int rsize, int *size, int oflag);
IMPORT STATUS nmc_get_niaddr(char *device, unsigned char *addr);
IMPORT STATUS nmc_findmod_by_addr(int *module, unsigned char *address);
IMPORT int    nmc_check_module(int module, int *err,
                               struct nmc_comm_info_struct **net);
IMPORT STATUS nmc_signal(char *from,int err);

/* These routines are in nmc_comm_subs_2.c" */
IMPORT STATUS nmc_allocmodnum(int *module);
IMPORT STATUS nmc_buymodule(int module, int override);
IMPORT STATUS nmc_allocate_memory(int module, int size, int *base_address);
IMPORT STATUS nmc_build_enet_addr(int input_addr, unsigned char *output_addr);
IMPORT STATUS nmc_broadcast_inq_task(struct nmc_comm_info_struct *net);
#ifdef vxWorks
IMPORT STATUS nmcEtherGrab(struct ifnet *pIf, char *buffer, int length);
#else
pcap_handler nmcEtherGrab(unsigned char *,const struct pcap_pkthdr*, const unsigned char*);
void nmcEthCapture(struct nmc_comm_info_struct *);
#endif
IMPORT STATUS nmc_broadcast_inq(struct nmc_comm_info_struct *net,
                                int inqtype, int addr);
IMPORT STATUS nmc_freemodule(int module, int override);
IMPORT STATUS nmc_byte_order_in(void *pkt);
IMPORT STATUS nmc_byte_order_out(void *pkt);

/* These routines are in nmc_user_subs_1.c */
IMPORT STATUS nmc_acqu_statusupdate(int module, int adc, int group, int address,
                                 int mode, int *live, int *real, int *totals,
                                 int *status);
IMPORT STATUS nmc_acqu_getmemory_cmp(int module, int adc, int saddress, int nrows,
                                   int start, int row, int channels,
                                   int *address);
IMPORT STATUS ndl_diffdecm(unsigned char *input, int channels, int *output,
                        int chans_left, int *actual_chans);

/* These routines are in nmc_user_subs_2.c */
IMPORT STATUS nmc_acqu_getlistbuf(int module, int adc, int buffer, int bytes,
                               int *address, int release);
IMPORT STATUS nmc_acqu_event_hdl(struct event_packet *epkt);
IMPORT STATUS nmc_acqu_addeventsem(int module, int adc, int event_type,
                                epicsEventId evt_id);
IMPORT STATUS nmc_acqu_remeventsem(int module, int adc, int event_type,
                                epicsEventId evt_id);
IMPORT STATUS nmc_acqu_getliststat(int module, int adc, int *acquire, int *current,
                                int *buff1stat, int *buff1bytes,
                                int *buff2stat, int *buff2bytes);
IMPORT STATUS nmc_acqu_setup(int module, int adc, int address, int channels,
                          int plive, int preal, int ptotals,
                          int ptschan, int ptechan, int mode);
IMPORT STATUS nmc_acqu_setstate(int module, int adc, int state);
IMPORT STATUS nmc_acqu_erase(int module, int address, int size);
IMPORT STATUS nmc_acqu_setpresets(int module, int adc, int plive, int preal,
                                  int ptotals, int ptschan, int ptechan);
IMPORT STATUS nmc_acqu_setelapsed(int module, int adc, int elive, int ereal);


#ifdef __cplusplus
}
#endif /*__cplusplus */