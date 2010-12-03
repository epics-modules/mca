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
*                               types like epicsInt32.  Defined structures to be
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

/* Pack all structures on 1-byte boundaries */
#pragma pack(1)

#define NCP_K_HCMD_SETACQADDR   1               /* SET ACQUISITION ADDRESS */
struct ncp_hcmd_setacqaddr {
        epicsUInt16 adc;                             /* ADC number */
        epicsUInt32 address;                         /* Acquisition address */
        epicsUInt32 limit;                           /* Acquisition limit address */
};

#define NCP_K_HCMD_SETELAPSED   2               /* SET ELAPSED TIMES */
struct ncp_hcmd_setelapsed {
        epicsUInt16 adc;
        epicsUInt32 live;                            /* Elapsed live time */
        epicsUInt32 real;                            /*         real      */
};

#define NCP_K_HCMD_SETMEMORY    3               /* SET MEMORY */
struct ncp_hcmd_setmemory {
        epicsUInt32 address;                         /* Destination address of data */
        epicsUInt32 size;                            /* Amount of data (bytes) */
                                                /* Start of data */
};

#define NCP_K_HCMD_SETMEMCMP    4               /* SET MEMORY COMPRESSED */
struct ncp_hcmd_setmemcmp {
        epicsUInt32 address;                         /* Destination address of data */
        epicsUInt32 size;                            /* Amount of data (bytes) */
                                                /* Start of data */
};

#define NCP_K_HCMD_SETPRESETS   5               /* SET PRESETS */
struct ncp_hcmd_setpresets {
        epicsUInt16 adc;
        epicsUInt32 live;                            /* Preset live time */
        epicsUInt32 real;                            /*        real      */
        epicsUInt32 totals;                          /* Preset total counts */
        epicsUInt32 start;                    	/* Preset totals start channel */
        epicsUInt32 end;                      	/*               end channel */
        epicsUInt32 limit;                    	/* Preset limit */
};

#define NCP_K_HCMD_SETACQSTATUS 6               /* SET ACQUISITION STATUS */
struct ncp_hcmd_setacqstate {
        epicsUInt16 adc;
        epicsInt8 status;                            /* New acquisition status (on/off) */
};

#define NCP_K_HCMD_ERASEMEM     7               /* ERASE MEMORY */
struct ncp_hcmd_erasemem {
        epicsUInt32 address;                  	/* Start address of memory to be erased */
        epicsUInt32 size;                     	/* Number of bytes to be erased */
};

#define NCP_K_HCMD_SETACQMODE   8               /* SET ACQUISITION MODE */
struct ncp_hcmd_setacqmode {
        epicsUInt16 adc;
        epicsInt8 mode;                              /* Acquisition mode; see NCP_C_AMODE_xxx */
};

#define NCP_K_HCMD_RETMEMORY    9               /* RETURN MEMORY */
struct ncp_hcmd_retmemory {
        epicsUInt32 address;                  	/* Start address of memory to return */
        epicsUInt32 size;                     	/* Number of bytes of memory to return */
};

#define NCP_K_HCMD_RETMEMCMP    10              /* RETURN MEMORY COMPRESSED */
struct ncp_hcmd_retmemcmp {
        epicsUInt32 address;                  	/* Start address of memory to return */
        epicsUInt32 size;                     	/* Number of bytes of memory to return */
};
#define NCP_K_MRESP_RETMEMCMP   227
struct ncp_mresp_retmemcmp {
        epicsUInt32 channels;                 	/* Number of channels of encoded data following */
                                                /* Encoded data starts here */
};

#define NCP_K_HCMD_RETADCSTATUS 11              /* RETURN ADC STATUS */
struct ncp_hcmd_retadcstatus {
        epicsUInt16 adc;
};
#define NCP_K_MRESP_ADCSTATUS   35
struct ncp_mresp_retadcstatus {
        epicsInt8 status;                            /* ADC on/off status */
        epicsUInt32 live;                     	/* Elapsed live time */
        epicsUInt32 real;                     	/*         real      */
        epicsUInt32 totals;                   	/*         total counts */
};

#define NCP_K_HCMD_SETHOSTMEM   13              /* SET HOST MEMORY */
struct ncp_hcmd_sethostmem {
        epicsUInt32 address;                  	/* Destination address of host data */
        epicsUInt32 size;                     	/* Amount of data (bytes) */
                                                /* Start of data */
};

#define NCP_K_HCMD_RETHOSTMEM   14              /* RETURN HOST MEMORY */
struct ncp_hcmd_rethostmem {
        epicsUInt32 address;                  	/* Start address of host memory to return */
        epicsUInt32 size;                     	/* Number of bytes of memory to return */
};

#define NCP_K_HCMD_SETOWNER     15              /* SET OWNER */
struct ncp_hcmd_setowner {
        epicsUInt8 owner_id[6];              	/* New owner ID */
        epicsInt8 owner_name[8];                     /* New owner name */
};

#define NCP_K_HCMD_SETOWNEROVER 16              /* SET OWNER OVERRIDE */
struct ncp_hcmd_setownerover {
        epicsUInt8 owner_id[6];              	/* New owner ID */
        epicsInt8 owner_name[8];                     /* New owner name */
};

#define NCP_K_HCMD_RESET        17              /* RESET MODULE */
#define NCP_K_HCMD_SETDISPLAY   18              /* SET DISPLAY CHARACTERISTICS */
#define NCP_K_HCMD_RETDISPLAY   19              /* RETURN DISPLAY */
#define NCP_K_HCMD_DIAGNOSE     20              /* DIAGNOSE */

#define NCP_K_HCMD_SENDICB      21              /* SEND TO ICB */
struct ncp_hcmd_sendicb {
        epicsUInt32 registers;                	/* Number of address/data pairs to write */
        struct {
           epicsUInt8 address;               	/* ICB address where data will be sent */
           epicsUInt8 data;                  	/* data to write */
        }  addresses[64];
};

#define NCP_K_HCMD_RECVICB      22              /* RECEIVE FROM ICB */
struct ncp_hcmd_recvicb {
        epicsUInt32 registers;                	/* Number of addresses to read */
        epicsUInt8 address[64];              	/* ICB addresses from which data is to be read */
};

#define NCP_K_HCMD_SETUPACQ     23              /* SETUP ACQUISITION */
struct ncp_hcmd_setupacq {
        epicsUInt16 adc;
        epicsUInt32 address;                  	/* Acquisition address */
        epicsUInt32 alimit;                   	/* Acquisition limit address */
        epicsUInt32 plive;                    	/* Preset live time */
        epicsUInt32 preal;                    	/*        real      */
        epicsUInt32 ptotals;                  	/* Preset total counts */
        epicsUInt32 start;                    	/* Preset totals start channel */
        epicsUInt32 end;                      	/*               end channel */
        epicsUInt32 plimit;                   	/* Preset limit */
        epicsUInt32 elive;                    	/* Elapsed live time */
        epicsUInt32 ereal;                    	/*         real      */
        epicsInt8 mode;                              /* Acquisition mode; see NCP_C_AMODE_xxx */
};

#define NCP_K_HCMD_RETACQSETUP  24              /* RETURN ACQUISITION SETUP INFO */
struct ncp_hcmd_retacqsetup {
        epicsUInt16 adc;
};
#define NCP_K_MRESP_RETACQSETUP 203
struct ncp_mresp_retacqsetup {
        epicsUInt32 address;                  	/* Acquisition address */
        epicsUInt32 alimit;                   	/* Acquisition limit address */
        epicsUInt32 plive;                    	/* Preset live time */
        epicsUInt32 preal;                    	/*        real      */
        epicsUInt32 ptotals;                  	/* Preset total counts */
        epicsUInt32 start;                    	/* Preset totals start channel */
        epicsUInt32 end;                      	/*               end channel */
        epicsUInt32 plimit;                   	/* Preset limit */
        epicsInt8 mode;
};

#define NCP_K_HCMD_SETMODEVSAP  25              /* SET MODULE EVENT MESSAGE ADDRESSES*/
struct ncp_hcmd_setmodevsap {
        epicsUInt16  mevsource;              	/* 0-255 are reserved for    */
                                                /* adc numbers. 255-65536    */
                                                /* are types NCP_K_MEVSRC_   */
        epicsInt8    mev_dsap;                       /* dest. service access point */
        epicsInt8    mev_ssap;                       /* sour. service access point */
        epicsInt8    snap_id[5];                     /* SNAP protocol ID */
};

#define NCP_K_HCMD_RETLISTMEM   26              /* RETURN LIST BUFFER */
struct ncp_hcmd_retlistmem {
        epicsUInt16 adc;                     	/* adc number */
        epicsUInt32 offset;                   	/* byte offset from start of buffer of transfer start */
        epicsUInt32 size;                     	/* number of bytes to return */
        epicsInt8 buffer;                            /* 0 for "1st" buffer, 1 for "2nd" */
};

#define NCP_K_HCMD_RELLISTMEM   27              /* RELEASE LIST BUFFER */
struct ncp_hcmd_rellistmem {
        epicsUInt16 adc;                     	/* adc number */
        epicsInt8 buffer;                            /* 0 for "1st" buffer, 1 for "2nd" */
};

#define NCP_K_HCMD_RETLISTSTAT  28              /* RETURN LIST ACQ STATUS */
struct ncp_hcmd_retliststat {
        epicsUInt16 adc;                     	/* adc number */
};
#define NCP_K_MRESP_RETLISTSTAT 251
struct ncp_mresp_retliststat {
        epicsInt8 status;                            /* acquire on/off state */
        epicsInt8 current_buffer;                    /* current acquire buffer: 0 for "1st" buffer, 1 for "2nd" */
        epicsInt8 buffer_1_full;                     /* 1st buffer status: 1 if full of data */
        epicsUInt32 offset_1;                 	/* number of bytes of data in 1st buffer */
        epicsInt8 buffer_2_full;                     /* 2nd buffer status: 1 if full of data */
        epicsUInt32 offset_2;                 	/* number of bytes of data in 2nd buffer */
};

#define NCP_K_HCMD_RETMEMSEP    29              /* RETURN MEMORY SEPARATED */
struct ncp_hcmd_retmemsep {
        epicsUInt32 address;                  	/* start address of first chunk of memory to return*/
        epicsUInt32 size;                     	/* number of bytes per chunk */
        epicsUInt32 offset;                   	/* byte offset between start of each chunk */
        epicsUInt32 chunks;                   	/* number of chunks to return */
};                     /* Note: total bytes returned will be */
                                                /*      size*chunks */

#define NCP_K_HCMD_RESETLIST    30              /* RESET LIST MODE */
struct ncp_hcmd_resetlist {
        epicsUInt16 adc;                     	/* adc number */
};

/*
* Define format for host memory usage. This is used by hosts to store module
* setup information in the module itself. There are 256 bytes of memory total.
*/

        struct nam_hostmem_v0_struct {

           epicsInt8 initialized;                    /* True if host mem has been setup */
           epicsInt8 version;                        /* Version number of this structure */
           epicsInt32 acqmem_usage_map[4];           /* Map of 8K blocks of acq memory */

           struct {
                epicsInt8 used;                      /* This input is used */
                epicsInt8 name[29];                  /* Configuration name of the input */
                epicsInt8 owner[12];                 /* Name of input's owner */
                epicsInt32 channels;                 /* Number of channels assigned to input */
                epicsUInt16 rows;            	/*           rows */
                epicsUInt8 groups;           	/*           groups */
                epicsInt32 memory_start;             /* Start of acq mem for this input */
                epicsUInt8 year;             	/* Acquisition start year (base = 1900) */
                epicsUInt8 month;            	/*                   month */
                epicsUInt8 day;              	/*                   day */
                epicsUInt8 hour;             	/*                   hour */
                epicsUInt8 minute;           	/*                   minute */
                epicsUInt8 seconds;          	/*                   seconds */
           } input[4];
        };

/*
* This structure defines the return argument from NMC_ACQU_GETACQSETUP; it
* characterizes the acquisition setup state of an AIM ADC.
*/

struct nmc_acqsetup_struct {

        epicsInt8 name[33];                          /* configuration name */
        epicsInt8 owner[12];                         /*               owner */
        epicsInt32 channels;
        epicsInt32 rows;
        epicsInt32 groups;
        epicsInt32 memory_start;                     /* acq memory start address */
        epicsInt32 plive;                            /* preset live time */
        epicsInt32 preal;                            /*        real      */
        epicsInt32 ptotals;                          /*        total counts */
        epicsInt32 start;                            /* preset totals start region */
        epicsInt32 end;                              /* preset totals end region */
        epicsInt32 plimit;                           /* preset limit */
        epicsInt32 mode;                             /* acquisition mode */
        epicsInt32 astime[2];                        /* acqusition start date/time */
        epicsInt32 agroup;                           /* acquire group */
        epicsInt32 status;                           /* acquire status */
        epicsInt32 live;                             /* elapsed live time */
        epicsInt32 real;                             /*         real      */
        epicsInt32 totals;                           /*         total counts */
};


/* Revert to previous packing */
#pragma pack()

/*
* Miscellaneous definitions
*/

/*
* Module acquisition modes
*/

#define NCP_C_AMODE_PHA         1               /* PHA */
#define NCP_C_AMODE_PHALFC      2               /* PHA/LFC */
#define NCP_C_AMODE_DLIST       3               /* Double buffered list */
