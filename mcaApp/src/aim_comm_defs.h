/* AIM_COMM_DEFS.H */

/*******************************************************************************
*
* Acquisition Interface Module (AIM) Communications Definitions
*
*       This module contains definitions for the commands used by networked
*       data acquisition modules to talk to host computers. Note that the
*       basic communications protocol definitions are in NCP_COMM_DEFS.
*
* NOTE: Nearly all of these definitions are tied to the AIM firmware, i.e.
*       they define the format of the data packets to/from the AIM. Thus
*       these definitions cannot be changed unless the AIM firmware is changed.
*
********************************************************************************
*
* Revision History
*
*       30-Dec-1993     MLR     Modified from Nuclear Data source
*       06-May-2000     MLR     Changed definitions to architecture-independent
*                               types like INT32.  Defined structures to be
*                               packed for architecture independence.
*
*******************************************************************************/

/*
* Host to Module Command Definitions
*/

/*
* The command codes are found in ncp_comm_packet.packet_code.
* Following each code definition is its argument structure, and, if required,
* the normal module response code and structure structure.
*/

#define NCP_K_HCMD_SETACQADDR   1               /* SET ACQUISITION ADDRESS */
struct ncp_hcmd_setacqaddr {
        UINT16 adc;                             /* ADC number */
        UINT32 address;                         /* Acquisition address */
        UINT32 limit;                           /* Acquisition limit address */
} __attribute__ ((packed));

#define NCP_K_HCMD_SETELAPSED   2               /* SET ELAPSED TIMES */
struct ncp_hcmd_setelapsed {
        UINT16 adc;
        UINT32 live;                            /* Elapsed live time */
        UINT32 real;                            /*         real      */
} __attribute__ ((packed));

#define NCP_K_HCMD_SETMEMORY    3               /* SET MEMORY */
struct ncp_hcmd_setmemory {
        UINT32 address;                         /* Destination address of data */
        UINT32 size;                            /* Amount of data (bytes) */
                                                /* Start of data */
} __attribute__ ((packed));

#define NCP_K_HCMD_SETMEMCMP    4               /* SET MEMORY COMPRESSED */
struct ncp_hcmd_setmemcmp {
        UINT32 address;                         /* Destination address of data */
        UINT32 size;                            /* Amount of data (bytes) */
                                                /* Start of data */
} __attribute__ ((packed));

#define NCP_K_HCMD_SETPRESETS   5               /* SET PRESETS */
struct ncp_hcmd_setpresets {
        UINT16 adc;
        UINT32 live;                            /* Preset live time */
        UINT32 real;                            /*        real      */
        UINT32 totals;                          /* Preset total counts */
        UINT32 start;                    	/* Preset totals start channel */
        UINT32 end;                      	/*               end channel */
        UINT32 limit;                    	/* Preset limit */
} __attribute__ ((packed));

#define NCP_K_HCMD_SETACQSTATUS 6               /* SET ACQUISITION STATUS */
struct ncp_hcmd_setacqstate {
        UINT16 adc;
        INT8 status;                            /* New acquisition status (on/off) */
} __attribute__ ((packed));

#define NCP_K_HCMD_ERASEMEM     7               /* ERASE MEMORY */
struct ncp_hcmd_erasemem {
        UINT32 address;                  	/* Start address of memory to be erased */
        UINT32 size;                     	/* Number of bytes to be erased */
} __attribute__ ((packed));

#define NCP_K_HCMD_SETACQMODE   8               /* SET ACQUISITION MODE */
struct ncp_hcmd_setacqmode {
        UINT16 adc;
        INT8 mode;                              /* Acquisition mode; see NCP_C_AMODE_xxx */
} __attribute__ ((packed));

#define NCP_K_HCMD_RETMEMORY    9               /* RETURN MEMORY */
struct ncp_hcmd_retmemory {
        UINT32 address;                  	/* Start address of memory to return */
        UINT32 size;                     	/* Number of bytes of memory to return */
} __attribute__ ((packed));

#define NCP_K_HCMD_RETMEMCMP    10              /* RETURN MEMORY COMPRESSED */
struct ncp_hcmd_retmemcmp {
        UINT32 address;                  	/* Start address of memory to return */
        UINT32 size;                     	/* Number of bytes of memory to return */
} __attribute__ ((packed));
#define NCP_K_MRESP_RETMEMCMP   227
struct ncp_mresp_retmemcmp {
        UINT32 channels;                 	/* Number of channels of encoded data following */
                                                /* Encoded data starts here */
} __attribute__ ((packed));

#define NCP_K_HCMD_RETADCSTATUS 11              /* RETURN ADC STATUS */
struct ncp_hcmd_retadcstatus {
        UINT16 adc;
} __attribute__ ((packed));
#define NCP_K_MRESP_ADCSTATUS   35
struct ncp_mresp_retadcstatus {
        INT8 status;                            /* ADC on/off status */
        UINT32 live;                     	/* Elapsed live time */
        UINT32 real;                     	/*         real      */
        UINT32 totals;                   	/*         total counts */
} __attribute__ ((packed));

#define NCP_K_HCMD_SETHOSTMEM   13              /* SET HOST MEMORY */
struct ncp_hcmd_sethostmem {
        UINT32 address;                  	/* Destination address of host data */
        UINT32 size;                     	/* Amount of data (bytes) */
                                                /* Start of data */
} __attribute__ ((packed));

#define NCP_K_HCMD_RETHOSTMEM   14              /* RETURN HOST MEMORY */
struct ncp_hcmd_rethostmem {
        UINT32 address;                  	/* Start address of host memory to return */
        UINT32 size;                     	/* Number of bytes of memory to return */
} __attribute__ ((packed));

#define NCP_K_HCMD_SETOWNER     15              /* SET OWNER */
struct ncp_hcmd_setowner {
        UINT8 owner_id[6];              	/* New owner ID */
        INT8 owner_name[8];                     /* New owner name */
} __attribute__ ((packed));

#define NCP_K_HCMD_SETOWNEROVER 16              /* SET OWNER OVERRIDE */
struct ncp_hcmd_setownerover {
        UINT8 owner_id[6];              	/* New owner ID */
        INT8 owner_name[8];                     /* New owner name */
} __attribute__ ((packed));

#define NCP_K_HCMD_RESET        17              /* RESET MODULE */
#define NCP_K_HCMD_SETDISPLAY   18              /* SET DISPLAY CHARACTERISTICS */
#define NCP_K_HCMD_RETDISPLAY   19              /* RETURN DISPLAY */
#define NCP_K_HCMD_DIAGNOSE     20              /* DIAGNOSE */

#define NCP_K_HCMD_SENDICB      21              /* SEND TO ICB */
struct ncp_hcmd_sendicb {
        UINT32 registers;                	/* Number of address/data pairs to write */
        struct {
           UINT8 address;               	/* ICB address where data will be sent */
           UINT8 data;                  	/* data to write */
        }  __attribute__ ((packed)) addresses[64];
} __attribute__ ((packed));

#define NCP_K_HCMD_RECVICB      22              /* RECEIVE FROM ICB */
struct ncp_hcmd_recvicb {
        UINT32 registers;                	/* Number of addresses to read */
        UINT8 address[64];              	/* ICB addresses from which data is to be read */
} __attribute__ ((packed));

#define NCP_K_HCMD_SETUPACQ     23              /* SETUP ACQUISITION */
struct ncp_hcmd_setupacq {
        UINT16 adc;
        UINT32 address;                  	/* Acquisition address */
        UINT32 alimit;                   	/* Acquisition limit address */
        UINT32 plive;                    	/* Preset live time */
        UINT32 preal;                    	/*        real      */
        UINT32 ptotals;                  	/* Preset total counts */
        UINT32 start;                    	/* Preset totals start channel */
        UINT32 end;                      	/*               end channel */
        UINT32 plimit;                   	/* Preset limit */
        UINT32 elive;                    	/* Elapsed live time */
        UINT32 ereal;                    	/*         real      */
        INT8 mode;                              /* Acquisition mode; see NCP_C_AMODE_xxx */
} __attribute__ ((packed));

#define NCP_K_HCMD_RETACQSETUP  24              /* RETURN ACQUISITION SETUP INFO */
struct ncp_hcmd_retacqsetup {
        UINT16 adc;
} __attribute__ ((packed));
#define NCP_K_MRESP_RETACQSETUP 203
struct ncp_mresp_retacqsetup {
        UINT32 address;                  	/* Acquisition address */
        UINT32 alimit;                   	/* Acquisition limit address */
        UINT32 plive;                    	/* Preset live time */
        UINT32 preal;                    	/*        real      */
        UINT32 ptotals;                  	/* Preset total counts */
        UINT32 start;                    	/* Preset totals start channel */
        UINT32 end;                      	/*               end channel */
        UINT32 plimit;                   	/* Preset limit */
        INT8 mode;
} __attribute__ ((packed));

#define NCP_K_HCMD_SETMODEVSAP  25              /* SET MODULE EVENT MESSAGE ADDRESSES*/
struct ncp_hcmd_setmodevsap {
        UINT16  mevsource;              	/* 0-255 are reserved for    */
                                                /* adc numbers. 255-65536    */
                                                /* are types NCP_K_MEVSRC_   */
        INT8    mev_dsap;                       /* dest. service access point */
        INT8    mev_ssap;                       /* sour. service access point */
        INT8    snap_id[5];                     /* SNAP protocol ID */
} __attribute__ ((packed));

#define NCP_K_HCMD_RETLISTMEM   26              /* RETURN LIST BUFFER */
struct ncp_hcmd_retlistmem {
        UINT16 adc;                     	/* adc number */
        UINT32 offset;                   	/* byte offset from start of buffer of transfer start */
        UINT32 size;                     	/* number of bytes to return */
        INT8 buffer;                            /* 0 for "1st" buffer, 1 for "2nd" */
} __attribute__ ((packed));

#define NCP_K_HCMD_RELLISTMEM   27              /* RELEASE LIST BUFFER */
struct ncp_hcmd_rellistmem {
        UINT16 adc;                     	/* adc number */
        INT8 buffer;                            /* 0 for "1st" buffer, 1 for "2nd" */
} __attribute__ ((packed));

#define NCP_K_HCMD_RETLISTSTAT  28              /* RETURN LIST ACQ STATUS */
struct ncp_hcmd_retliststat {
        UINT16 adc;                     	/* adc number */
} __attribute__ ((packed));
#define NCP_K_MRESP_RETLISTSTAT 251
struct ncp_mresp_retliststat {
        INT8 status;                            /* acquire on/off state */
        INT8 current_buffer;                    /* current acquire buffer: 0 for "1st" buffer, 1 for "2nd" */
        INT8 buffer_1_full;                     /* 1st buffer status: 1 if full of data */
        UINT32 offset_1;                 	/* number of bytes of data in 1st buffer */
        INT8 buffer_2_full;                     /* 2nd buffer status: 1 if full of data */
        UINT32 offset_2;                 	/* number of bytes of data in 2nd buffer */
} __attribute__ ((packed));

#define NCP_K_HCMD_RETMEMSEP    29              /* RETURN MEMORY SEPARATED */
struct ncp_hcmd_retmemsep {
        UINT32 address;                  	/* start address of first chunk of memory to return*/
        UINT32 size;                     	/* number of bytes per chunk */
        UINT32 offset;                   	/* byte offset between start of each chunk */
        UINT32 chunks;                   	/* number of chunks to return */
} __attribute__ ((packed));                     /* Note: total bytes returned will be */
                                                /*      size*chunks */

#define NCP_K_HCMD_RESETLIST    30              /* RESET LIST MODE */
struct ncp_hcmd_resetlist {
        UINT16 adc;                     	/* adc number */
} __attribute__ ((packed));

/*
* Define format for host memory usage. This is used by hosts to store module
* setup information in the module itself. There are 256 bytes of memory total.
*/

        struct nam_hostmem_v0_struct {

           INT8 initialized;                    /* True if host mem has been setup */
           INT8 version;                        /* Version number of this structure */
           INT32 acqmem_usage_map[4];           /* Map of 8K blocks of acq memory */

           struct {
                INT8 used;                      /* This input is used */
                INT8 name[29];                  /* Configuration name of the input */
                INT8 owner[12];                 /* Name of input's owner */
                INT32 channels;                 /* Number of channels assigned to input */
                UINT16 rows;            	/*           rows */
                UINT8 groups;           	/*           groups */
                INT32 memory_start;             /* Start of acq mem for this input */
                UINT8 year;             	/* Acquisition start year (base = 1900) */
                UINT8 month;            	/*                   month */
                UINT8 day;              	/*                   day */
                UINT8 hour;             	/*                   hour */
                UINT8 minute;           	/*                   minute */
                UINT8 seconds;          	/*                   seconds */
           } __attribute__ ((packed)) input[4];
        } __attribute__ ((packed));

/*
* This structure defines the return argument from NMC_ACQU_GETACQSETUP; it
* characterizes the acquisition setup state of an AIM ADC.
*/

struct nmc_acqsetup_struct {

        INT8 name[33];                          /* configuration name */
        INT8 owner[12];                         /*               owner */
        INT32 channels;
        INT32 rows;
        INT32 groups;
        INT32 memory_start;                     /* acq memory start address */
        INT32 plive;                            /* preset live time */
        INT32 preal;                            /*        real      */
        INT32 ptotals;                          /*        total counts */
        INT32 start;                            /* preset totals start region */
        INT32 end;                              /* preset totals end region */
        INT32 plimit;                           /* preset limit */
        INT32 mode;                             /* acquisition mode */
        INT32 astime[2];                        /* acqusition start date/time */
        INT32 agroup;                           /* acquire group */
        INT32 status;                           /* acquire status */
        INT32 live;                             /* elapsed live time */
        INT32 real;                             /*         real      */
        INT32 totals;                           /*         total counts */
} __attribute__ ((packed));

/*
* Miscellaneous definitions
*/

/*
* Module acquisition modes
*/

#define NCP_C_AMODE_PHA         1               /* PHA */
#define NCP_C_AMODE_PHALFC      2               /* PHA/LFC */
#define NCP_C_AMODE_DLIST       3               /* Double buffered list */
