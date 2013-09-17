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
#include <epicsString.h>

#ifdef USE_SOCKETS
  #include <sys/socket.h>
  #include <net/if.h>
#endif

#if defined(vxWorks)
  #include <vxWorks.h>
  #include <msgQLib.h>
  #include <etherLib.h>
  #include <semLib.h>
  #include <sockLib.h>
  #include <net/if_llc.h>
  #include "llc.h"
#elif defined (CYGWIN32) || defined(_WIN32)
  #include <pcap.h>
#else
  #ifdef USE_SOCKETS
   #include "linux-llc.h"
  #else
    #include <libnet.h>
    #include <pcap.h>
  #endif
#endif
#include <errlog.h>


#include "aim_comm_defs.h"
#include "ncp_comm_defs.h"
#include "nmcmsgdef.h"

#define INTERFACE_NAME_SIZE 100
#define SNAP_SIZE 5
#ifndef ETH_ALEN
  #define ETH_ALEN 6
#endif

#ifndef AF_LLC
  #define AF_LLC 26
#endif

#if !defined(LLC_SAP_SNAP)
  #define LLC_SAP_SNAP 0xAA
#endif

#if !defined(LLC_SNAP_LSAP) && defined(LLC_SAP_SNAP)
  #define LLC_SNAP_LSAP LLC_SAP_SNAP
#endif

#ifdef vxWorks
#else
  /* VxWorks has these definitions */
  #define OK                         0
  #define ERROR                 (-1)
  #define FALSE       0
  #define TRUE            1

  #define IMPORT 

  /* this is defined by VxWorks */
  typedef       int             BOOL;
  typedef int STATUS;
#endif /* vxWorks */

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
   signed char module_type;            /* module type */
   struct nmc_comm_info_struct *comm_device; /* Pointer to network link to module */
   unsigned char address[ETH_ALEN];   /* module network address */
   unsigned char owner_id[ETH_ALEN];  /* owner id of module */
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


#ifdef USE_LIBNET
  /* We used to call this an ifnet structure.  That is OK on Linux, which does not define that structure,
     but it could lead to problems on BSD systems.  Use another name instead. */
  struct libnet_ifnet {
          char    *if_name;              /* interface name, e.g. eth0 */
                  short   if_unit;       /* not used, always 0 */
                  libnet_t *libnet;      /* libnet_t struct for packet injection */
                  char errbuf[LIBNET_ERRBUF_SIZE];
                  struct  libnet_ether_addr *hw_address;     /* Ethernet Mac-Address of our device */
  };
#endif

/*
* This structure contains data relating to the communications channel over
* which we talk to networked modules.
*/

struct nmc_comm_info_struct {
   char valid;                    /* structure entry valid */
   char type;                     /* Network device type */
   char *name;                    /* Network device name */
   unsigned char sys_address[ETH_ALEN];  /* System network address */
   epicsThreadId status_pid;       /* Thread ID of status dispatch */
   epicsThreadId broadcast_pid;    /* Thread ID of broadcast poller  */
   epicsThreadId capture_pid;      /* Thread ID of ether capture thread */
   float timeout_time;            /* time in s */
   int header_size;               /* The size of the device dependent header */
   int max_msg_size;              /* Largest possible message size */
   int max_tries;                 /* Number of command retries allowed */
   epicsMessageQueueId statusQ;   /* Message queue for status messages */
   unsigned char response_snap[SNAP_SIZE]; /* NI SNAP ID for normal messages */
   unsigned char status_snap[SNAP_SIZE];  /* NI SNAP ID for status/event messages */
#ifdef USE_SOCKETS
   int sockfd;                    /* Socket */
   struct sockaddr_llc dest;      /* addr struct for sending (address overwritten) */
#else
   pcap_t *pcap;                          /* Pointer to pcap structure */
#endif
#ifdef USE_LIBNET
   struct libnet_ifnet *pIf;      /* Pointer to libnet_ifnet structure */
#endif
   unsigned char response_sap;    /* NI SAP address for normal messages */
   unsigned char status_sap;      /* NI SAP address for status/event messages*/
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
           if (aimDebug > 0) errlogPrintf("AIM MODULE_INTERLOCK_ON failed!\n")
#define MODULE_INTERLOCK_OFF(module) \
        epicsMutexUnlock(nmc_module_info[module].module_mutex)


/* Function prototypes */
#ifdef __cplusplus
  extern "C" {
#endif

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
#ifdef USE_SOCKETS
  IMPORT void nmcEtherGrab(char *buffer, int length);
#else
  void nmcEtherGrab(unsigned char *,const struct pcap_pkthdr*, const unsigned char*);
#endif
void nmcEthCapture(struct nmc_comm_info_struct *);
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
IMPORT STATUS nmc_acqu_getmemory(int module, int adc, int saddress, int nrows,
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
IMPORT STATUS nmc_show_modules();


#ifdef __cplusplus
}
#endif /*__cplusplus */
