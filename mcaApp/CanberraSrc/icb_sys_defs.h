/*******************************************************************************
*
* This module contains structure and constant definitions that are used in the
* VAX/VMS Instrument Control Bus (ICB) Control System.
*
********************************************************************************
*
* Revision History:
*
*       21-Jul-1988     RJH     Original
*       25-Oct-1988     RJH     Add ICB_K_ASTAT_xxxx definitions
*       17-Jan-1989     RJH     Add ICB_M_INIT_xxxx definitions
*       07-Oct-1991     RJH     Modifications to support CCNIM and new ICB
*                               register formats
*       12-Feb-1992     TLG     More CCNIM modifications
*       30-Jul-1992     RJH     Make sure that the ICB global section structure
*                               is packed for Alpha CPUs
*       12-Mar-1993     TLG     Adding additional structure elements for the
*                               ICB HVPS.
*       06-Dec-1999     MLR     Minor changes to avoid compiler warnings
*       25-Jul-2000     MLR     Changed parameters to icb_initialize and
*                               icb_crmpsc
*
*******************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#include "epicsTypes.h"
	
#define ICB_K_MAX_MODULES 96    /* The number of entries in ICB_MODULE_INFO */

/*
* Define module availability status codes, returned from ICB_GET_MODULE_STATE
*/

#define ICB_K_ASTAT_NOCTRLR 0
#define ICB_K_ASTAT_NOMODULE 1
#define ICB_K_ASTAT_MODPRESENT 2
#define ICB_K_ASTAT_MODINUSE 3
#define ICB_K_ASTAT_FAILED 4
#define ICB_K_ASTAT_ATTENTION 5

/*
* Define flag bit arguments for ICB_INITIALIZE
*/

#define ICB_M_INIT_CLRDB 1      /* clear the database if it already exists */
#define ICB_M_INIT_RONLY 2      /* open database readonly */



/*******************************************************************************
*
* CCNIM module structures
*
*******************************************************************************/

#define ICB_K_MAX_CACHED_PARAMS 32
#define ICB_K_MAX_CCNIM_TYPES   3
#define ICB_K_CCNIM_ADC         0
#define ICB_K_CCNIM_AMP         1
#define ICB_K_CCNIM_HVPS        2


/*
* Define the structure that stores the parameter code and a flag that
* indicates that it's been stored in the cache but not sent to the
* ICB module itself.
*/
        typedef struct {

                epicsInt32    pcode;          /* CAM parameter code   */
                epicsInt32    cached_flag;    /* Boolian              */

        } ICB_CACHED_PARAM;



/*******************************************************************************
*
* Define the generic module structure.  All modules will have the same
* first set of elements.
*
*******************************************************************************/

        typedef struct {

                epicsInt32 icb_index;         /* Module ICB database index    */
                ICB_CACHED_PARAM param_list[ICB_K_MAX_CACHED_PARAMS];
                epicsInt8 extra_header[24];  /* Space for more header info   */

        } ICB_CCNIM_ANY;


/*******************************************************************************
*
* Define the ADC module structures.
*
*******************************************************************************/

/*--------------*/
/* ADC Flags    */
/*--------------*/

        typedef struct {
                unsigned antic   : 1;   /* Anti-coincidence mode          */
                unsigned latec   : 1;   /* Late (vs. early) coinc mode    */
                unsigned delpk   : 1;   /* Delayed (vs. auto) peak detect */
                unsigned cimcaif : 1;   /* CI (vs. ND) ADC interface      */
                unsigned nonov   : 1;   /* Non-overlap transfer mode      */
                unsigned ltcpur  : 1;   /* LTC/PUR Sig.                   */
                unsigned online  : 1;   /* Module is on-line              */
                unsigned atten   : 1;   /* Module requires attention      */
        } ICB_ADC_FLAGS;

/*----------------------*/
/* ADC Verify Flags     */
/*----------------------*/

        typedef struct {
                unsigned id      : 1;   /* Module serial number           */
                unsigned range   : 1;   /* ADC range                      */
                unsigned offset  : 1;   /* ADC offset                     */
                unsigned acqmode : 1;   /* ADC acquisition mode (8 char)  */
                unsigned cnvgain : 1;   /* ADC conversion gain            */
                unsigned lld     : 1;   /* Lower level descriminator      */
                unsigned uld     : 1;   /* Upper level descriminator      */
                unsigned zero    : 1;   /* ADC zero                       */
                unsigned antic   : 1;   /* Anti-coincidence mode          */
                unsigned latec   : 1;   /* Late (vs. early) coinc mode    */
                unsigned delpk   : 1;   /* Delayed (vs. auto) peak detect */
                unsigned nonov   : 1;   /* Non-overlap transfer mode      */
                unsigned ltcpur  : 1;   /* LTC/PUR Sig.                   */
        } ICB_ADC_VFLAGS;

/*----------------------*/
/* ADC Main Structure   */
/*----------------------*/

        typedef struct {

                epicsInt32 icb_index;         /* Module ICB database index    */
                ICB_CACHED_PARAM param_list[ICB_K_MAX_CACHED_PARAMS];
                epicsInt8 extra_header[24];  /* Space for more header info   */

                /*----------------------*/
                /* ADC CAM values       */
                /*----------------------*/

                epicsInt32 range;             /* ADC range                    */
                epicsInt32 offset;            /* ADC offset                   */
                epicsInt8 acqmode[12];       /* ADC acquisition mode (8 char)*/
                epicsInt32 cnvgain;           /* ADC conversion gain          */
                epicsInt32 abs_cnvlim;        /* Absolute conv. gain limit    */
                float lld;               /* Lower level descriminator    */
                float uld;               /* Upper level descriminator    */
                float zero;              /* ADC zero                     */
                epicsInt8 extra_values[32];  /* Space for more values        */

                /*--------------------------------*/
                /* ADC CAM flags (CAM_L_ADCFLAGS) */
                /*--------------------------------*/

                union {
                    epicsInt32 lword;         /* Standard flags field         */
                    ICB_ADC_FLAGS bit;  /* Individual flag bit struct   */
                } flags;

                /*----------------------------------------*/
                /* ADC CAM verify flags (CAM_L_ADCVFLAGS) */
                /*----------------------------------------*/

                union {
                    epicsInt32 lword;         /* Standard flags field         */
                    ICB_ADC_VFLAGS bit; /* Individual flag bit struct   */
                } vflags;

                epicsInt32 extra_flags[1];    /* Space for extra values         */

        } ICB_CCNIM_ADC;


/*******************************************************************************
*
* Define the Amplifier module structure.
*
*******************************************************************************/

/*--------------*/
/* AMP Flags    */
/*--------------*/

        typedef struct {
                unsigned diff    : 1;   /* Differential (vs. normal) input  */
                unsigned negpol  : 1;   /* Negative (vs. positive) polarity */
                unsigned compinh : 1;   /* Compliment inhibit polarity      */
                unsigned purej   : 1;   /* Pileup rejection enable/disable  */
                unsigned online  : 1;   /* Module is on-line                */
                unsigned motrbusy: 1;   /* Amplifier motor busy             */
                unsigned pzbusy  : 1;   /* Amplifier pole zero busy         */
                unsigned atten   : 1;   /* Module requires attention        */
                unsigned pzstart : 1;   /* Start AMP auto pole zero         */
                unsigned pzfail  : 1;   /* Amplifier pole zero failed       */
                unsigned motrfail: 1;   /* Amplifier motor failed           */
        } ICB_AMP_FLAGS;

/*----------------------*/
/* AMP Verify Flags     */
/*----------------------*/

        typedef struct {
                unsigned id      : 1;   /* Module serial number               */
                unsigned prampt  : 1;   /* Preamp type (RC,TRP) (8 char)      */
                unsigned hwgain1 : 1;   /* Course gain                        */
                unsigned hwgain2 : 1;   /* Fine gain                          */
                unsigned hwgain3 : 1;   /* Super fine gain                    */
                unsigned shapem  : 1;   /* Shaping mode (8 char)              */
                unsigned pz      : 1;   /* Pole zero                          */
                unsigned blrtype : 1;   /* Base-line restore (SYM,ASYM) (8 char) */
                unsigned dtctype : 1;   /* Dead-time control (Normal,LFC) (8 char) */
                unsigned tc      : 1;   /* AMP time constant (seconds)        */
                unsigned negpol  : 1;   /* Negative (vs. positive) polarity   */
                unsigned compinh : 1;   /* Compliment inhibit polarity        */
                unsigned purej   : 1;   /* Pileup rejection enable/disable    */
                unsigned diff    : 1;   /* Differential (vs. normal) input    */
        } ICB_AMP_VFLAGS;

/*----------------------*/
/* AMP Main Structure   */
/*----------------------*/

        typedef struct {

                epicsInt32 icb_index;         /* Module ICB database index    */
                ICB_CACHED_PARAM param_list[ICB_K_MAX_CACHED_PARAMS];
                epicsInt8 extra_header[24];  /* Space for more header info   */

                /*----------------------*/
                /* AMP CAM values       */
                /*----------------------*/

                epicsInt8 pramptype[12];     /* Preamp type (RC,TRP) (8 char)      */
                float gain;              /* Virtual gain (hwg1 * hwg2 * hwg3)  */
                float hwgain1;           /* Course gain                        */
                float hwgain2;           /* Fine gain                          */
                float hwgain3;           /* Super fine gain                    */
                epicsInt8 shapemode[12];     /* Shaping mode (8 char)              */
                epicsInt32 pz;                /* Pole zero                          */
                epicsInt8 blrtype[12];       /* Base-line restore (SYM,ASYM) (8 char) */
                epicsInt8 dtctype[12];       /* Dead-time control (Normal,LFC) (8 char) */
                float tc;                /* AMP time constant (seconds)        */
                epicsInt32 hwgain2_timer;     /* Callback Timer id for fine gain    */
                epicsInt8 extra_values[32];  /* Space for more values              */

                /*--------------------------------*/
                /* AMP CAM flags (CAM_L_AMPFLAGS) */
                /*--------------------------------*/

                union {
                    epicsInt32 lword;         /* Standard flags field         */
                    ICB_AMP_FLAGS bit;  /* Individual flag bit struct   */
                } flags;

                /*----------------------------------------*/
                /* AMP CAM verify flags (CAM_L_AMPVFLAGS) */
                /*----------------------------------------*/

                union {
                    epicsInt32 lword;         /* Standard flags field         */
                    ICB_AMP_VFLAGS bit; /* Individual flag bit struct   */
                } vflags;

                epicsInt32 extra_flags[1];    /* Space for more values            */

        } ICB_CCNIM_AMP;


/*******************************************************************************
*
* Define the HVPS module structure.
*
*******************************************************************************/

/*--------------*/
/* HVPS Flags   */
/*--------------*/

        typedef struct {
                unsigned ovle     : 1;  /* Overload latch enable        */
                unsigned inhle    : 1;  /* Inhibit latch enable         */
                unsigned lvinh    : 1;  /* 5v/12v inhibit               */
                unsigned pol      : 1;  /* Output polarity              */
                unsigned inh      : 1;  /* Inhibit                      */
                unsigned ov       : 1;  /* Overload                     */
                unsigned stat     : 1;  /* Status (on/off)              */
                unsigned online   : 1;  /* Module is on-line            */
                unsigned ovinres  : 1;  /* Overload/Inhibit reset       */
                unsigned atten    : 1;  /* Module requires attention    */
                unsigned fastramp : 1;  /* Use the fast ramp method     */
                unsigned busy     : 1;  /* HVPS module is busy          */
        } ICB_HVPS_FLAGS;

/*----------------------*/
/* HVPS Verify Flags    */
/*----------------------*/

        typedef struct {
                unsigned id     : 1;    /* Module serial number         */
                unsigned volt   : 1;    /* High voltage setting         */
                unsigned ovle   : 1;    /* Overload latch enable        */
                unsigned inhle  : 1;    /* Inhibit latch enable         */
                unsigned lvinh  : 1;    /* 5v/12v inhibit               */
                unsigned pol    : 1;    /* Output polarity              */
                unsigned stat   : 1;    /* Status (on/off)              */
        } ICB_HVPS_VFLAGS;

/*----------------------*/
/* HVPS Main Structure  */
/*----------------------*/

        typedef struct {

                epicsInt32 icb_index;         /* Module ICB database index    */
                ICB_CACHED_PARAM param_list[ICB_K_MAX_CACHED_PARAMS];
                epicsInt8 extra_header[24];  /* Space for more header info   */

                /*----------------------*/
                /* HVPS CAM values      */
                /*----------------------*/

                float voltage;           /* High voltage setting         */
                float voltlim;           /* Voltage limit                */
                float abs_voltlim;       /* Absolute limit for module    */
                float current_level;     /* Current voltage level        */
                void *ramp_ast_id;      /* Id of ramp function AST      */
                                        /*  Non-zero indicates that a   */
                                        /*  ramp is in progress         */
                epicsInt8 extra_values[24];  /* Space for more values        */

                /*----------------------------------*/
                /* HVPS CAM flags (CAM_L_HVPSFLAGS) */
                /*----------------------------------*/

                union {
                    epicsInt32 lword;         /* Standard flags field         */
                    ICB_HVPS_FLAGS bit; /* Individual flag bit struct   */
                } flags;

                /*------------------------------------------*/
                /* HVPS CAM verify flags (CAM_L_HVPSVFLAGS) */
                /*------------------------------------------*/

                union {
                    epicsInt32 lword;         /* Standard flags field         */
                    ICB_HVPS_VFLAGS bit;/* Individual flag bit struct   */
                } vflags;

                epicsInt32 extra_flags[1];    /* Space for more values        */

        } ICB_CCNIM_HVPS;


/*
* Declare a structure to pass a list of parameters to the handlers
*/

typedef struct _ICB_PARAM_LIST {

        epicsInt32    pcode;
        epicsInt32    reserved;
        void    *value;

} ICB_PARAM_LIST;



/*
* Define flag bit arguments for 2nd generation ICB module handlers
*/

#define ICB_M_HDLR_WRITE        0x00000001      /* Write specified parameters */
                                                /*  to the ICB module         */
#define ICB_M_HDLR_READ         0x00000002      /* Read specified parameters  */
                                                /*  from the ICB module       */
#define ICB_M_HDLR_INITIALIZE   0x00000004      /* Initialize the ICB module  */

#define ICB_M_HDLR_CACHE_ONLY   0x00000008      /* CACHE only flag            */
#define ICB_M_HDLR_WRITE_CACHE  0x00000009      /* Write to CACHE only        */

#define ICB_M_HDLR_FLUSH_CACHE  0x00000010      /* Send all parameters that   */
                                                /*  have been cached but not  */
                                                /*  sent to the ICB module.   */

#define ICB_M_HDLR_NOVERIFY     0x00010000      /* Do do not verify write     */



/* Flags for the ICB_HVPS_CONVERT_VOLTAGE() function */

#define ICB_M_HVPS_DAC_TO_VOLTAGE       1


/*
* Declare the structure that contains info about all ICB modules that we know
* about.
*/

#ifdef __ALPHA
#pragma member_alignment __save
#pragma nomember_alignment
#endif

typedef struct icb_module_info_struct {

        char valid;             /* flag: this entry is used */
        char address[10];       /* address specification of the module */
        signed char module_type; /* the type (i.e., ID register) of the module */
        unsigned char registers[14];    /* copy of the modules's registers */
        int ni_module;          /* for NI based modules: the NI module number */
        char base_address;      /* the ICB address of the 1st register of the module */
        char present;           /* flag: module is actually present on the ICB */
        char failed;            /* flag: module has asserted its "failed" indication */
        char initialize;        /* flag: module should be initialized (by POLL_CONTROLLER) */
        char attention;         /* flag: module attention has been set */
        int (*handler)();       /* address of a routine that handles reset/svc rqst for this type of module */
        ICB_CCNIM_ANY  *pcache; /* Pointer to cache structure for this module */
        int module_sn;          /* the module serial number (unique to every module) */
        signed char r0_read;    /* Bits read from module's register 0   */
        signed char r0_write;   /* Bits to write to the module's register 0 */
        char was_reset;         /* Indicates that module has been reset */
        char monitor_state;     /* If set, requests that AICONTROL monitor the*/
                                /*  module's state                            */
        char state_change;      /* There has been a change in state */

} ICB_MODULE_INFO;

#ifdef __ALPHA
#pragma member_alignment __restore
#endif



/* Function prototypes for the ICB routines */

/* Functions in icb_control_subs.c */
int icb_initialize();
int icb_findmod_by_address(char *dsc_address, int *index);
int icb_find_free_entry(int *index);
int icb_insert_module(int index,char *dsc_address, int type, int flags);
int icb_output(int index, unsigned int reg, unsigned int registers,
        unsigned char values[]);
int icb_get_ni_module(struct icb_module_info_struct *entry);
int icb_output_all(char *address, int index, int registers,
        unsigned char values[]);
int icb_output_register(char *address, int index, int reg,
        unsigned char value[]);
int icb_input(int index, unsigned int reg, unsigned int registers,
        unsigned char values[]);
int icb_input_all(char *address, int index, int registers,
        unsigned char values[]);
int icb_input_register(char *address, int index, int reg,
        unsigned char value[]);
int icb_refresh_module(char *address, int index);
int icb_normalize_address(char *dsc_input, char *dsc_output);

/* Functions in icb_control_subs2.c */
int icb_poll_controller(char *dsc_controller);
int icb_scan_controller(char *dsc_controller, signed char array[]);
int icb_poll_ni_controller(int module);
int icb_get_module_id(int index);
int icb_read_nvram(int index, int addr, int *value);
int icb_write_nvram(int index, int addr, unsigned char value);

/* Functions in icb_crmpsc.c */
int icb_crmpsc();

/* Functions in icb_get_module_status.c */
int icb_get_module_state(int index);

/* Functions in icb_handler_subs.c */
epicsInt32 icb_hvps_hdlr (epicsInt32 index, ICB_PARAM_LIST *params, epicsInt32 flags);
epicsInt32 icb_adc_hdlr (epicsInt32 index, ICB_PARAM_LIST *params, epicsInt32 flags);
epicsInt32 icb_amp_hdlr (epicsInt32 index, ICB_PARAM_LIST *params, epicsInt32 flags);
int icb_validate_module (epicsInt32 index, ICB_PARAM_LIST *params,
                                                 epicsInt32 *present, epicsInt32 *reset, epicsInt32 flags);
int icb_init_ccnim_cache (epicsInt32 index, epicsInt32 flags);
ICB_PARAM_LIST *icb_build_cached_plist (ICB_CCNIM_ANY *ccany);
int icb_set_cached_flags (ICB_CCNIM_ANY *ccany, ICB_PARAM_LIST *params);
epicsInt32 icb_hvps_write (ICB_CCNIM_HVPS *hvps, ICB_PARAM_LIST *params, epicsInt32 flags);
epicsInt32 icb_hvps_read (ICB_CCNIM_HVPS *hvps, ICB_PARAM_LIST *params, epicsInt32 flags);
epicsInt32 icb_hvps_verify (ICB_CCNIM_HVPS *hvps, ICB_PARAM_LIST *params, epicsInt32 flags,
                              epicsUInt8 *reg_list);
epicsInt32 icb_hvps_ramp_voltage (ICB_CCNIM_HVPS *hvps);
epicsInt32 icb_hvps_send_voltage (ICB_CCNIM_HVPS *hvps, float voltage, epicsInt32 flags);
epicsInt32 icb_hvps_convert_voltage (ICB_CCNIM_HVPS *hvps, float *voltage, epicsInt32 flags);
epicsInt32 icb_hvps_set_state (ICB_CCNIM_HVPS *hvps, epicsInt32 state, epicsInt32 flags);
epicsInt32 icb_adc_write (ICB_CCNIM_ADC *adc, ICB_PARAM_LIST *params, epicsInt32 flags);
epicsInt32 icb_adc_read (ICB_CCNIM_ADC *adc, ICB_PARAM_LIST *params, epicsInt32 flags);
epicsInt32 icb_adc_verify (ICB_CCNIM_ADC *adc, ICB_PARAM_LIST *params, epicsInt32 flags,
                                 epicsUInt8 *reg_list);
epicsInt32 icb_amp_write (ICB_CCNIM_AMP *amp, ICB_PARAM_LIST *params, epicsInt32 flags);
epicsInt32 icb_amp_read (ICB_CCNIM_AMP *amp, ICB_PARAM_LIST *params, epicsInt32 flags);
epicsInt32 icb_amp_verify (ICB_CCNIM_AMP *amp, ICB_PARAM_LIST *params, epicsInt32 flags,
                                 epicsUInt8 *reg_list);
epicsInt32 icb_amp_vgain_to_hwgain (ICB_CCNIM_AMP *amp, epicsInt32 flags);
epicsInt32 icb_amp_write_gain2 (ICB_CCNIM_AMP *amp);
epicsInt32 icb_amp_complete_position (ICB_CCNIM_AMP *amp);
epicsInt32 icb_amp_test_gain2_complete (ICB_CCNIM_AMP *amp);
epicsInt32 icb_amp_home_motor (ICB_CCNIM_AMP *amp);
epicsInt32 icb_amp_test_home (ICB_CCNIM_AMP *amp);
epicsUInt32 icb_amp_compute_motor_pos (ICB_CCNIM_AMP *amp);
epicsInt32 icb_amp_get_nvram_motor_pos (epicsInt32 index, epicsInt32 *nvpos, epicsInt32 flags);
epicsInt32 icb_amp_put_nvram_motor_pos (epicsInt32 index, epicsInt32 nvpos, epicsInt32 flags);
epicsInt32 icb_amp_put_motor_pos (epicsInt32 index, epicsInt32 pos, epicsInt32 flags);
epicsInt32 icb_amp_start_pz (ICB_CCNIM_AMP *amp);
epicsInt32 icb_amp_test_pz_complete (ICB_CCNIM_AMP *amp);
epicsInt32 icb_amp_write_pz (ICB_CCNIM_AMP *amp);
epicsInt32 icb_adc_encode_chns (epicsUInt32 chns);
epicsInt32 icb_write_csr (ICB_CCNIM_ANY *ccnim, epicsInt32 perm_bits, epicsInt32 temp_bits,
                                        epicsInt32 mask);
epicsInt32 icb_monitor_modules ();

/* The following routines are in icb_strings.c */
int icb_dsc2str (epicsInt8 *str, epicsInt8 *dsc, epicsInt32 max_len);
int icb_str2dsc (epicsInt8 *dsc,     epicsInt8 *str);
int StrNCmp (epicsInt8 *s1, epicsInt8 *s2, int len);
int StrUpCase (epicsInt8 *dst, epicsInt8 *src, int len);
int StrTrim (epicsInt8 *dst, epicsInt8 *src, int len, int *trim_len);

#ifdef __cplusplus
}
#endif /*__cplusplus */
