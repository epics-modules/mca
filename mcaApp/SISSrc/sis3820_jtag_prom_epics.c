/***************************************************************************/
/*                                                                         */
/*  Filename: sis3820_jtag_xilinx_prom.c                                   */
/*                                                                         */
/*  Funktion:                                                              */
/*                                                                         */
/*  Autor:                TH                                               */
/*  date:                 14.11.2003                                       */
/*  last modification:    03.12.2003                                       */
/*                                                                         */
/* ----------------------------------------------------------------------- */
/*                                                                         */
/*  SIS  Struck Innovative Systeme GmbH                                    */
/*                                                                         */
/*  Harksheider Str. 102A                                                  */
/*  22399 Hamburg                                                          */
/*                                                                         */
/*  Tel. +49 (0)40 60 87 305 0                                             */
/*  Fax  +49 (0)40 60 87 305 20                                            */
/*                                                                         */
/*  http://www.struck.de                                                   */
/*                                                                         */
/*  © 2003                                                                 */
/*                                                                         */
/***************************************************************************/

/***************************************************************************/
/* Modified by Mark Rivers, Dec. 8, 2014                                   */ 
/* Changes to run inside an EPICS on VME processors                        */
/* Changes to work with big-endian architectures                           */
/* Renamed to sis3820_jtag_prom_epics                                      */
/***************************************************************************/



/*===========================================================================*/
/* Headers                                     */
/*===========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>

#include <epicsThread.h>
#include <epicsExport.h>
#include <epicsEndian.h>
#include <iocsh.h>
#include <devLib.h>
    
#include "sis3820.h" // Header for sis3820

/*===========================================================================*/
/* Globals                                       */
/*===========================================================================*/

#define SIS3820_JTAG_TEST       0x310
#define SIS3820_JTAG_CONTROL    0x314
#define SIS3820_JTAG_DATA_IN    0x310

#define CE_PROMPT       1     // display no command menu, only prompt
#define CE_PARAM_ERR    2
#define CE_NO_DMA_BUF   3
#define CE_FORMAT       4




#define BYPASS    0xFF
#define IDCODE    0xFE
#define ISPEN     0xE8
#define FPGM      0xEA
#define FADDR     0xEB
#define FVFY1     0xF8
#define NORMRST   0xF0
#define FERASE    0xEC
#define SERASE    0x0A
#define FDATA0    0xED
#define CONFIG    0xEE


#define MAX_NOF_JTG_DEVS   6

#define SIS3820_ADDRESS_TYPE atVMEA32

static unsigned int gl_jtg_nof_devices ;
static unsigned int gl_jtg_select ;
static int gl_jtg_state ;
static unsigned int gl_jtg_dev_idcodes[MAX_NOF_JTG_DEVS] ;
static unsigned int gl_jtg_ir_len[MAX_NOF_JTG_DEVS] ;
static int gl_sis1100_device;
static unsigned char *gl_wbuf; /* 1MByte ;  min. 512 KByte for one 18v04 */
static unsigned char *gl_rbuf; /* 1MByte ;  min. 512 KByte for one 18v04 */


#define MAX_NUMBER_OF_PRINTS 0x10      
#define MAX_NUMBER_LWORDS    0x1000000       /* 64MByte */


/*===========================================================================*/
/* Prototypes                                       */
/*===========================================================================*/
static int mcs_file_unpack (char* fname, char* data_buffer, unsigned int* mcs_data_byte_length) ;
static int jtag_tap_reset (unsigned int base_addr) ;
static int jtag_reset (unsigned int base_addr) ;
static int jtag_instruction(unsigned int base_addr, unsigned int icode) ;
static unsigned int jtag_data(unsigned int base_addr, unsigned int din, unsigned int len) ;
static int jtag_rd_data(unsigned int base_addr, void *buf, unsigned int len, int state) ;
static int jtag_wr_data(unsigned int base_addr, void  *buf, unsigned int len, int   state) ;
static int jtag_program_verifier (unsigned int base_addr, char* fname, int program) ;
static int jtag_erase (unsigned int base_addr);
static void print_jtag_chain (void ) ;

static int vme_A32D32_read(void *unused, int addr, int *data);
static int vme_A32D32_write(void *unused, int addr, int data);

/****************************************************************************/


int sis3820_jtag_prom_epics(int baseAddress, const char *cmd_parameter, char *mcs_file_name)
{
   /* command_parameter = "R" (Read IDCode)
    *                     "V" (Verify Prom against file)
    *                     "P" (Program Prom with file and load FPGA)
    */

    int return_code;
    int mod_base;
    unsigned int addr, data  ;

    devBusToLocalAddr(SIS3820_ADDRESS_TYPE,
                     (size_t)baseAddress,
                     (volatile void**)&mod_base);

    gl_wbuf = (unsigned char *)malloc(0x100000); /* 1MByte ;  min. 512 KByte for one 18v04 */
    gl_rbuf = (unsigned char *)malloc(0x100000); /* 1MByte ;  min. 512 KByte for one 18v04 */


    addr=mod_base + SIS3820_MODID ;
    if (vme_A32D32_read(&gl_sis1100_device, addr, &data)) {
        printf("Error reading SIS3820 module ID register at address 0x%x\n", addr);
        goto done;
    }
    printf("\n\nSIS3820_MODID =  0x%08x    \n", data);
    if ((data & 0x38200100) != 0x38200100) {
        printf("module is not SIS3820 scaler\n");
        goto done;
    }
    if ((data & 0xff) < 2) {
        printf("minor revision < 2, JTAG over VME not supported\n");
        goto done;
    } 

    switch (*cmd_parameter) {

        case 'R' : { /* data record */
            printf("\n\n\n\n");
            gl_jtg_select = 0 ;
            jtag_reset (mod_base) ;
            print_jtag_chain();
            jtag_tap_reset (mod_base) ;
        }
        break;

        case 'V' : { /* data record */
            printf("\n\n\n\n");
            return_code = jtag_program_verifier (mod_base, mcs_file_name,0) ;
            if (return_code != 0) {
                printf("Abort  \n");
            }
            jtag_tap_reset (mod_base) ;
        }
        break;

        case 'P' : { /* data record */
            printf("\n\n\n\n");
            return_code = jtag_program_verifier (mod_base, mcs_file_name,1) ;
            if (return_code == 0) {
                jtag_instruction(mod_base, CONFIG);
                printf("Load FPGA   \n");
                epicsThreadSleep(2.) ;
            }
            else {
                printf("Abort  \n");
                goto done;
            }
        }
        break;

        default : {
            printf("not valid CMD-Paramter\n");
            goto done ;
        }
        break;
    }  /* switch */

    done:
    free(gl_rbuf);
    free(gl_wbuf);
  
    return 0;
}

static int vme_A32D32_read(void *unused, int addr, int *data)
{
  return devReadProbe(4, (char *) addr, (char *) data);
}

static int vme_A32D32_write(void *unused, int addr, int data)
{
  return devWriteProbe(4, (char *) addr, (char *) &data);
}

int correct_byte_order(int data)
{
  union {
    int i; 
    char c[4];
  } in, out;
  
  if (EPICS_BYTE_ORDER == EPICS_ENDIAN_LITTLE) return data;
  in.i = data;
  out.c[0] = in.c[3];
  out.c[1] = in.c[2];
  out.c[2] = in.c[1];
  out.c[3] = in.c[0];
  return out.i;
}

/* iocsh config function */
static const iocshArg configArg0 = { "Base address",     iocshArgInt};
static const iocshArg configArg1 = { "Command", iocshArgString};
static const iocshArg configArg2 = { "MCS file name",  iocshArgString};

static const iocshArg * const configArgs[] = 
{
    &configArg0,
    &configArg1,
    &configArg2,
};

static const iocshFuncDef configFuncDef = 
    {"sis3820_jtag_prom_epics",3,configArgs};

static void configCallFunc(const iocshArgBuf *args)
{
    sis3820_jtag_prom_epics(args[0].ival, args[1].sval, args[2].sval);
}

void sis3820_jtag_promRegister(void)
{
    iocshRegister(&configFuncDef,configCallFunc);
}

epicsExportRegistrar(sis3820_jtag_promRegister);




/***************************************************************************************************************/
/***************************************************************************************************************/

int jtag_tap_reset (unsigned int base_addr)
{
  int return_code ;
  unsigned int addr ;
  unsigned int ux,uy;


/* enable      */
  addr = base_addr + SIS3820_JTAG_CONTROL ;
  return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
  if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}

  addr = base_addr + SIS3820_JTAG_TEST ;

  ux = 0 ;
  do {
     uy=0;
     do { 
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x2) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
     } while (++uy < 5);
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x0) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
   } while (++ux < 3);


  return 0 ;
}




int jtag_reset (unsigned int base_addr)
{
  int return_code ;
  unsigned int data ;
  unsigned int addr ;
  unsigned int ux,uy;


/* enable      */
  addr = base_addr + SIS3820_JTAG_CONTROL ;
  return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
  if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}

  ux = 0 ;
  addr = base_addr + SIS3820_JTAG_TEST ;
  do {
     uy=0;
     do { 
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x2) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
     } while (++uy < 5);
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x0) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
   } while (++ux < 3);

    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x2) ;
    if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}

    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x0) ;
    if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}

    gl_jtg_nof_devices = 0 ;    
    gl_jtg_state = -1 ;
    ux=MAX_NOF_JTG_DEVS;

   while (1) {
      uy=0;
      do {
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x0) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
      } while (++uy < 32);

      addr = base_addr + SIS3820_JTAG_DATA_IN ;
      return_code =  vme_A32D32_read(&gl_sis1100_device, addr, &data) ;
      if (return_code != 0) {printf("vme_A32D32_read:ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}

      if ((data == 0) || (data == 0xffffffff) || (ux == 0)) break;
      gl_jtg_nof_devices = gl_jtg_nof_devices+ 1 ;
       gl_jtg_dev_idcodes[MAX_NOF_JTG_DEVS-ux] = data ;
       //gl_jtg_ir_len[MAX_NOF_JTG_DEVS-ux] = 8 ; /* valid only for 18v0x proms */
      --ux;
      gl_jtg_state = 0 ;
   }


  addr = base_addr + SIS3820_JTAG_TEST ;
  return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x2) ;
  if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}

  return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x2) ;
  if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}

  return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x0) ;
  if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}


  return 0 ;
}




/*--------------------------- jtag_instruction ------------------------------*/
 
int jtag_instruction(unsigned int base_addr, unsigned int icode)
{
  int return_code ;
  unsigned int data ;
  unsigned int addr ;
   u_int    ux,uy;
   u_int    tdo;
   u_char   ms;

    addr = base_addr + SIS3820_JTAG_TEST ;
    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x3) ;
    if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x3) ;
    if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
    if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
    if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
   
    ux=gl_jtg_nof_devices;
    tdo=0; 
    ms=0;   

#ifdef falsch_rum
    while (ux) {
      ux--; uy=0;
      do {
         if ((ux == 0) && (uy == (gl_jtg_ir_len[ux] -1))) ms=2;

         if (ux != gl_jtg_select) {
            addr = base_addr + SIS3820_JTAG_TEST ;
            return_code = vme_A32D32_write(&gl_sis1100_device, addr, ms |1) ;
            if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
         } else {
            addr = base_addr + SIS3820_JTAG_DATA_IN ;
            return_code =  vme_A32D32_read(&gl_sis1100_device, addr, &data) ;
            if (return_code != 0) {printf("vme_A32D32_read:ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
            tdo |=((data >>31) &1) <<uy;
            addr = base_addr + SIS3820_JTAG_TEST ;
            return_code = vme_A32D32_write(&gl_sis1100_device, addr, ms |(icode &1)) ;
            if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
            icode >>=1;
         }
      } while (++uy < gl_jtg_ir_len[ux]);
   }
#endif
// es muss mit Device ux=0 angefangen werden
#define richtig_rum
#ifdef richtig_rum
    ux=0;
    do {
      uy=0;
      do {
         if ((ux == gl_jtg_nof_devices-1) && (uy == (gl_jtg_ir_len[ux] -1))) ms=2;

         if (ux != (gl_jtg_nof_devices-1 - gl_jtg_select) ) {
            addr = base_addr + SIS3820_JTAG_TEST ;
            return_code = vme_A32D32_write(&gl_sis1100_device, addr, ms |1) ;
            if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
         } else {
            addr = base_addr + SIS3820_JTAG_DATA_IN ;
            return_code =  vme_A32D32_read(&gl_sis1100_device, addr, &data) ;
            if (return_code != 0) {printf("vme_A32D32_read:ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
            tdo |=((data >>31) &1) <<uy;
            addr = base_addr + SIS3820_JTAG_TEST ;
            return_code = vme_A32D32_write(&gl_sis1100_device, addr, ms |(icode &1)) ;
            if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
            icode >>=1;
         }
      } while (++uy < gl_jtg_ir_len[ux]);
    } while (++ux<gl_jtg_nof_devices)  ;
#endif

    addr = base_addr + SIS3820_JTAG_TEST ;
    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x3) ;
    if (return_code != 0) { printf("1vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
    return tdo;
}



/*--------------------------- jtag_data -------------------------------------*/
//
unsigned int jtag_data(unsigned int base_addr, unsigned int din, unsigned int len)
{
  int return_code ;
  unsigned int data ;
  unsigned int addr ;
   u_int    dev,uy;
   u_long   tdo;
   u_char   ms;

   errno=0;

    addr = base_addr + SIS3820_JTAG_TEST ;
    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x3) ;
    if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
    if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
    if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
   
    dev=gl_jtg_nof_devices ;   
    tdo=0; 
    ms=0;

    while (dev) {
      dev--;
      if (dev != gl_jtg_select) {
        data = (dev) ? 1 : 3;
        addr = base_addr + SIS3820_JTAG_TEST ;
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, data) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
      } else {
         uy=0;
         while (len) {
            len--;
            if ((dev == 0) && (len == 0)) ms=2;
                addr = base_addr + SIS3820_JTAG_DATA_IN ;
                return_code =  vme_A32D32_read(&gl_sis1100_device, addr, &data) ;
                if (return_code != 0) {printf("vme_A32D32_read:ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
                tdo |=((data >>31) &1) <<uy;
                uy++;

                data=ms |((u_char)din &1);
                din >>=1;
                addr = base_addr + SIS3820_JTAG_TEST ;
                return_code = vme_A32D32_write(&gl_sis1100_device, addr, data) ;
                if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        
         }
      }
   }

    addr = base_addr + SIS3820_JTAG_TEST ;
    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x3) ;
    if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
    if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}

   return tdo;
}












/*-------------------------- jtag_rd_data ---------------------------------- */
 
int jtag_rd_data(unsigned int base_addr, 
   void  *buf,
   unsigned int len,     /*  0: end */
   int   state)   /*  1: start */
                  /*  0: continue */
                  /* -1: end */
{
  int return_code ;
  unsigned int data ;
  unsigned int addr ;
   u_long   *bf;
   u_int    ux;

   if (gl_jtg_state < 0) return CE_PROMPT;

   if (len == 0) {
      if (gl_jtg_state == 1) { 
        addr = base_addr + SIS3820_JTAG_TEST ;
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x3) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x3) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        gl_jtg_state=0; 
      }
      return 0;
   }


   if ((state == 1) && (gl_jtg_state > 0)) {
        addr = base_addr + SIS3820_JTAG_TEST ;
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x3) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x3) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        gl_jtg_state=0;
   }



   if (gl_jtg_state == 0) {
        addr = base_addr + SIS3820_JTAG_TEST ;
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x3) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        ux=gl_jtg_nof_devices;
        while (--ux != gl_jtg_select) {
            addr = base_addr + SIS3820_JTAG_TEST ;
            return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
            if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        }
        gl_jtg_state=1;
   }


   bf=(u_long*)buf;
   while (len > 4) {

//#define VERSION_VME_WRITE_DMA

#ifndef VERSION_VME_WRITE_DMA
      ux=0;
      addr = base_addr + SIS3820_JTAG_TEST ;
      do {
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
      } while (++ux < 32);
#endif

#ifdef VERSION_VME_WRITE_DMA
{      unsigned int data_array[0x20] ;
      unsigned int i_ux ;
      unsigned int req_nof_data, put_nof_data ;
      for (i_ux=0;i_ux<0x20;i_ux++) {data_array[i_ux] = 0 ;}
      req_nof_data = 0x20 ;
      addr = base_addr + SIS3820_JTAG_TEST ;
      return_code = vme_A32DMA_D32FIFO_write(&gl_sis1100_device, addr, data_array, req_nof_data, &put_nof_data) ;
      if (return_code != 0) { printf("vme_A32DMA_D32FIFO_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
}
#endif


      addr = base_addr + SIS3820_JTAG_DATA_IN ;
      return_code =  vme_A32D32_read(&gl_sis1100_device, addr, &data) ;
      if (return_code != 0) {printf("vme_A32D32_read:ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
      *bf++ = correct_byte_order(data) ;
      len -=4;
   }


    ux=0;
    addr = base_addr + SIS3820_JTAG_TEST ;
    do {
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
    } while (++ux < 32);



      addr = base_addr + SIS3820_JTAG_DATA_IN ;
      return_code =  vme_A32D32_read(&gl_sis1100_device, addr, &data) ;
      if (return_code != 0) {printf("vme_A32D32_read:ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
      *bf++ = correct_byte_order(data) ;

   
   if (state < 0) {
        addr = base_addr + SIS3820_JTAG_TEST ;
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x3) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x3) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        gl_jtg_state=0;
   }

   return 0;
}




 
/*--------------------------- jtag_wr_data ----------------------------------*/

int jtag_wr_data(unsigned int base_addr, 
   void  *buf,
   unsigned int len,     /* Anzahl Byte */
   int   state)          /* -1: end     */
{
  int return_code ;
  unsigned int data ;
  unsigned int addr ;
   u_long   *bf;
   u_long   din;
   u_int    dev;
   u_int    ux;
   u_char   ms;

   if (gl_jtg_state < 0) return CE_PROMPT;

   bf=(u_long*)buf; 
   len >>=2;
   if (len == 0) return 0;

    addr = base_addr + SIS3820_JTAG_TEST ;
    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x3) ;
    if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
    if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
    return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
    if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}

    dev=gl_jtg_nof_devices ;   
    ms=0;

   
   
   
   while (dev) {
      dev--;
      if (dev != gl_jtg_select) {
        data = (dev) ? 1 : 3;
        addr = base_addr + SIS3820_JTAG_TEST ;
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, data) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
      } else {
         while (len) {
            len--;
            din=correct_byte_order(*bf++); ux=32;
            while (ux) {
               ux--;
               if ((dev == 0) && (len == 0) && (ux == 0)) ms=2;
                 data = ms |((u_char)din &1);
                 addr = base_addr + SIS3820_JTAG_TEST ;
                 return_code = vme_A32D32_write(&gl_sis1100_device, addr, data) ;
                 if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
                 din >>=1;
            }
         }
      }
   }





   if (state < 0) {
        addr = base_addr + SIS3820_JTAG_TEST ;
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x3) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x1) ;
        if (return_code != 0) { printf("vme_A32D32_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        gl_jtg_state=0;
   }

   return 0;
}














int mcs_file_unpack (char* fname, char* data_buffer, unsigned int* mcs_data_byte_length)
{
    int retcode ;
    char line_in[0x100]  ;
    FILE *loaderfile;


    unsigned int line_length  ;
    unsigned int nof_byte_in_record    ;    
    unsigned int byte_address_in_record     ;    
    unsigned int type_in_record ;
    unsigned int extended_address_in_record ;
/*    unsigned char databyte_in_record ; */
    unsigned int databyte_in_record ;
    unsigned int wr_buf_index ;

    unsigned int i  ;

    for (i=0;i<0x100000;i++) {
        data_buffer[i]  = (char)0xff ;
    } 


    *mcs_data_byte_length = 0 ;
    wr_buf_index = 0 ;

    loaderfile=fopen(fname,"r");
    if ((int)loaderfile>0) {
        printf("\nloader file %s opened\n",fname);
    }
     else {
        printf("\n\nERROR:   loader file %s not found\n\n",fname);
        return -1;
    }

    retcode=fscanf(loaderfile,"%s\n",line_in);           
    while (retcode>0) {
        /* printf("%s\n",line_in); */
        /* check 1. character */
        retcode = strncmp(line_in,":",1) ;
        if (retcode != 0) {
            printf("mcs-file error: 1.character is not ':' \n");
            return -1 ;
        }


        /* read in_line length */
        line_length = strlen(line_in) ;
        if (line_length <= 10) {
            printf("mcs-file error: number of character in recordline is less than 11 \n");
            return -1 ;
        }


        retcode=sscanf(&line_in[1],"%2x%4x%2x",&nof_byte_in_record, 
                                                     &byte_address_in_record, &type_in_record) ;

        if(retcode != 3) {
            printf("mcs-file error: format error1 \n");
            return -1 ;
        }
        /* printf("nof_byte_in_record     = 0x%08x\n",nof_byte_in_record);     */
         /* printf("byte_address_in_record = 0x%08x\n",byte_address_in_record); */
         /* printf("type_in_record         = 0x%08x\n",type_in_record);         */

        switch (type_in_record) {

            case 0x0 : { /* data record */
                /* address check */
                if(wr_buf_index >= 0x80000) {
                    printf("mcs-file error: file is too long \n");
                    printf("wr_buf_index     = 0x%08x\n",wr_buf_index);  /*   */
                    return -1 ;
                }

                if (byte_address_in_record  != (wr_buf_index & 0xffff)) {
                    printf("mcs-file error: address error! \n");
                    printf("byte_address_in_record   =  0x%04x! \n",byte_address_in_record);
                    printf("(wr_buf_index && 0xffff) =  0x%04x! \n\n",(wr_buf_index & 0xffff));
                    return -1 ;
                }    ;

                
                for (i=0;i<nof_byte_in_record;i++) {
                    retcode=sscanf(&line_in[9+(2*i)],"%2x",&databyte_in_record) ;
                    if(retcode != 1) {
                        printf("mcs-file error: data_in_record \n");
                        return -1 ;
                    }
                    /* printf("wr_buf_index     = 0x%08x\n",wr_buf_index);     */
                    data_buffer[wr_buf_index]  = databyte_in_record ;
                    wr_buf_index++ ;
                } 
            }  
            break;
            
            case 0x1 : { /* end-of_record */
                printf("end of mcs-file  \n");
                /* printf("wr_buf_index                  = 0x%08x\n",wr_buf_index);    */
                /* printf("data_buffer[wr_buf_index-1]   = 0x%08x\n",data_buffer[wr_buf_index-1]);    */
                /* printf("data_buffer[wr_buf_index]     = 0x%08x\n",data_buffer[wr_buf_index]);    */
                /* printf("data_buffer[wr_buf_index+1]   = 0x%08x\n",data_buffer[wr_buf_index+1]);    */
                if(wr_buf_index <= 0x70000) {
                    printf("mcs-file error: not enough data \n");
                    printf("wr_buf_index     = 0x%08x\n",wr_buf_index);  /*   */
                    return -1 ;
                }
                *mcs_data_byte_length = wr_buf_index ;
                return 0 ;
            }  
            break;
            
            case 0x2 : { /* extended segment address record */
                printf("mcs-file error: not supported  TYPE of data \n");
                return -1 ;
            }  
            break;
            
            case 0x4 : { /* extended linear address record */
                /* check address */
                retcode=sscanf(&line_in[9],"%4x",&extended_address_in_record) ;
                if(retcode != 1) {
                    printf("mcs-file error: extended_address_in_record \n");
                    return -1 ;
                }

                if (extended_address_in_record  != (wr_buf_index >> 16)) {
                    printf("mcs-file error: extended linear address ! \n");
                    printf("extended_address_in_record =  0x%04x! \n",extended_address_in_record);
                    printf("(wr_buf_index >> 16)   =  0x%04x! \n\n",(wr_buf_index >> 16));
                    return -1 ;
                }    ;
            }  
            break;
            
            default : {
                printf("mcs-file error: wrong TYPE of data \n");
                return -1 ;
            }  
            break;
        }  /* switch */
        retcode=fscanf(loaderfile,"%s\n",line_in);           
    } 

  return 0 ;
}









/*--------------------------- jtag_prom -------------------------------------*/
    
int jtag_program_verifier (unsigned int base_addr, char* fname, int program)
{
    int return_code ;
    unsigned int data ;
    unsigned int addr ;
    unsigned int i ;
    unsigned int mcs_buffer_length ;

    int rest_write_length  ;    
    unsigned int max_write_length  ; 
    unsigned int write_byte_index  ;

    unsigned int rest_read_length ;
    unsigned int max_read_length  ;
    unsigned int read_length      ;
    unsigned int read_byte_index ;
    unsigned int max_error_value ;
    unsigned int error_counter ;
    int      ret;
    int retcode=1;

    
   u_long   lx;

   
    jtag_reset (base_addr) ;
    print_jtag_chain();


#ifdef raus
    /* only one 18v04 is allowed */
    if (gl_jtg_nof_devices != 1)  { 
        printf("\n\nERROR:  more or less one Device (only one 18v04)   \n\n\n");
        return -1 ;
    }
#endif
    /* exit if no devices */
    if ((gl_jtg_nof_devices != 1) && (gl_jtg_nof_devices != 4) ) { 
        printf("\n\nno Device \n\n\n");
        return -1 ;
    }

    if (gl_jtg_nof_devices == 1)  { 
        printf("\n\nprogram 1. Device \n\n\n");
        gl_jtg_select = 1 ;
    }

    if (gl_jtg_nof_devices == 2)  { 
        printf("\n\nprogram 2. Device \n\n\n");
        gl_jtg_select = 0 ;
    }

        gl_jtg_select = 0 ;

    

    retcode=mcs_file_unpack (fname, gl_wbuf, &mcs_buffer_length) ;
    if (retcode != 0) {
        printf("\n\nERROR:  mcs-file unpack    \n\n\n");
        return -1 ;
    }

    




   
   
   
   if (program == 1 ) {
 
/*--- ISP PROM programmieren */
 
        jtag_instruction(base_addr, ISPEN);
        jtag_data(base_addr, 0x04, 6);

        ret=jtag_instruction(base_addr, FADDR);
        if (ret != 0x11) printf("FADDR.0 %02X\n", ret);
        jtag_data(base_addr, 1, 16);
        jtag_instruction(base_addr, FERASE);

        printf("\nErase prom \n");

        /*  ux=0; do dout +=gigal_regs->jtag_data; while (++ux < 1000); // mindestens 500 */
        i=0; 
        do {
            //t   dout +=gigal_regs->jtag_data; 
            addr = base_addr + SIS3820_JTAG_DATA_IN ;
            return_code =  vme_A32D32_read(&gl_sis1100_device, addr, &data) ;
            if (return_code != 0) {printf("vme_A32D32_read:ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
            i++ ;
        } while (i < 1000);   /* min  500 */
        epicsThreadSleep(5.0);


        printf("\nErase finished \n");

/*        jtag_instruction(base_addr, NORMRST); */

/*        ret=jtag_instruction(base_addr, BYPASS);      */    
/*        if (ret != 0x01) printf("BYPASS.0 %02X\n", ret);   */

         jtag_instruction(base_addr, ISPEN);
         jtag_data(base_addr, 0x04, 6);

        lx=0;

        
        rest_write_length = mcs_buffer_length ;    
        rest_write_length = (rest_write_length + 0x200) & 0xfffe00 ; /*  */    

        max_write_length = 0x200 ; /* 18v04  */
        write_byte_index = 0 ;
        
        printf("\n\nWrite data to prom \n");
        
        do {
            jtag_instruction(base_addr, FDATA0);
            ret=jtag_wr_data(base_addr, &gl_wbuf[write_byte_index], max_write_length, -1) ;            
            if (ret != 0) {
                return -1 ;
            }

            if (write_byte_index == 0) { /* ony first time */
                ret=jtag_instruction(base_addr, FADDR);
                if (ret != 0x11) {
                    printf("FADDR.1 %02X\n", ret);
                }
                jtag_data(base_addr, 0, 16);
            }
            jtag_instruction(base_addr, FPGM);
            write_byte_index  = write_byte_index + max_write_length ;
            rest_write_length = rest_write_length - max_write_length ;
            i=0; 
            do { /* delay */
                addr = base_addr + SIS3820_JTAG_DATA_IN ;
                return_code =  vme_A32D32_read(&gl_sis1100_device, addr, &data) ;
                if (return_code != 0) {printf("vme_A32D32_read:ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
                i++ ;
            } while (i < 1000);   /* min  500 */
            epicsThreadSleep(2*epicsThreadSleepQuantum());

            printf("rest_write_length = %08X            %03d%%  \r",rest_write_length, ((100*write_byte_index)/((mcs_buffer_length + 0x200) & 0xfffe00)));

        } while (rest_write_length > 0);


       ret=jtag_instruction(base_addr, FADDR);
       if (ret != 0x11) printf("FADDR.2 %02X\n", ret);

       jtag_data(base_addr, 1, 16);
       jtag_instruction(base_addr, SERASE);

      i=0; 
      do {
      addr = base_addr + SIS3820_JTAG_DATA_IN ;
      return_code =  vme_A32D32_read(&gl_sis1100_device, addr, &data) ;
      if (return_code != 0) {printf("vme_A32D32_read:ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
      i++ ;
      } while (i < 1000);   /* min  500 */
      printf("\nWrite finished \n");

/*      jtag_instruction(base_addr, NORMRST); */

/*       ret=jtag_instruction(base_addr, BYPASS);*/
/*        if (ret != 0x01) printf("BYPASS.1 %02X\n", ret);*/

   }

  

 
   
/*    retcode=mcs_file_unpack (fname, gl_wbuf, mcs_buffer_length) ; */
   

/* --- read ISP PROM and compare with file-data-buffer */

   jtag_instruction(base_addr, ISPEN);
   jtag_data(base_addr, 0x34, 6);
   ret=jtag_instruction(base_addr, FADDR);
   if (ret != 0x11) printf("FADDR.3 %02X\n", ret);
   jtag_data(base_addr, 0, 16);
   jtag_instruction(base_addr, FVFY1);


   i=0; 
   do {
        addr = base_addr + SIS3820_JTAG_DATA_IN ;
        return_code =  vme_A32D32_read(&gl_sis1100_device, addr, &data) ;
        if (return_code != 0) {printf("vme_A32D32_read:ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
        i++ ;
   } while (i < 100);   /* min  20 */
   
   
    max_error_value = 0x10 ;
    rest_read_length = mcs_buffer_length ;    
    max_read_length = 0x1000 ;
    read_byte_index = 0 ;

    printf("\n\nRead data from prom \n");

    while (rest_read_length != 0) {    
        if (rest_read_length >= max_read_length) {
            read_length = max_read_length ;
        }
        else {
            read_length = rest_read_length ;
        }
        rest_read_length = rest_read_length - read_length ;

        if ((ret=jtag_rd_data(base_addr, &gl_rbuf[read_byte_index], read_length, 0)) != 0) return ret;
        read_byte_index = read_byte_index + read_length ;
        printf("rest_read_length  = %08X            %03d%%  \r",rest_read_length, ((100*read_byte_index)/mcs_buffer_length));
        epicsThreadSleep(2*epicsThreadSleepQuantum());

    }   
    printf("\nRead finished \n");

    /* compare */

    printf("\nVerifier started \n");
    error_counter=0;
    for (i=0;i<mcs_buffer_length;i++) {
         if(gl_rbuf[i] != gl_wbuf[i]) {
            printf("i = %08X, prom = %02X  file = %02X\n",i, gl_rbuf[i], gl_wbuf[i]);
            error_counter++ ;
        }
    //    if(error_counter >= max_error_value) {break; } 
        if(error_counter >= 100) {break; } 
    }



    jtag_rd_data(base_addr, 0, 0, 0) ;    
    jtag_instruction(base_addr, NORMRST);
    
    if(error_counter == 0) { 
        printf("\nVerifier OK \n\n");
        return 0 ;
    }
    else {
        printf("\nVerifier failed !!!!!!!!!!!!!!!!!!!!!!!!!! \n\n");
        return -1 ;
    }






}







int jtag_erase (unsigned int base_addr)
{
    int return_code ;
    unsigned int data ;
    unsigned int addr ;
    unsigned int i ;

    unsigned int error_counter ;
    int      ret;

    
   u_long   lx;

   
    jtag_reset (base_addr) ;
    printf("number of devices  = %d   \n\n",gl_jtg_nof_devices);
    for (i=0;i<gl_jtg_nof_devices;i++)  {
        printf("device %d :  dev_idcodes = 0x%08x       ",i+1,gl_jtg_dev_idcodes[i]);
        if ((gl_jtg_dev_idcodes[i] & 0x0fffffff) == 0x05026093) {
            printf("device = 18V04 \n");
        }    
        else {
            if ((gl_jtg_dev_idcodes[i] & 0x0fffffff) == 0x05036093) {
                printf("device = 18V04 (STM)\n");
            }    
            else {
                printf("unknown device  \n");
                printf("\n\nERROR:   not 18V04   \n\n\n");
                return -1 ;
            }    
        }    
    }


    /* only one 18v04 is allowed */
    if (gl_jtg_nof_devices != 1)  { 
        printf("\n\nERROR:  more or less one Device (only one 18v04)   \n\n\n");
        return -1 ;
    }

    

 
/*--- ISP PROM   */
 
        jtag_instruction(base_addr, ISPEN);
        jtag_data(base_addr, 0x04, 6);

        ret=jtag_instruction(base_addr, FADDR);
        if (ret != 0x11) printf("FADDR.0 %02X\n", ret);
        jtag_data(base_addr, 1, 16);
        jtag_instruction(base_addr, FERASE);

        printf("\nErase prom \n");
        /*  ux=0; do dout +=gigal_regs->jtag_data; while (++ux < 1000); // mindestens 500 */
        i=0; 
        do {
            //t   dout +=gigal_regs->jtag_data; 
            addr = base_addr + SIS3820_JTAG_DATA_IN ;
            return_code =  vme_A32D32_read(&gl_sis1100_device, addr, &data) ;
            if (return_code != 0) {printf("vme_A32D32_read:ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
            i++ ;
        //} while (i < 1000);   /* min  500 */
        //} while (i < 1000);   /* ca. 700 us */
        //} while (i < 20000);   /* ca. 14000 us */
        } while (i < 2000000);   /* ca. 5 Sec */
        printf("\nErase finished \n");

/*        jtag_instruction(base_addr, NORMRST); */

/*        ret=jtag_instruction(base_addr, BYPASS);  */        
/*        if (ret != 0x01) printf("BYPASS.0 %02X\n", ret);   */

         jtag_instruction(base_addr, ISPEN);
         jtag_data(base_addr, 0x04, 6);

        lx=0;

        

  

    error_counter = 0 ;
   
    jtag_rd_data(base_addr, 0, 0, 0) ;    
    jtag_instruction(base_addr, NORMRST);
    
    if(error_counter == 0) { 
        printf("\nErase OK \n\n");
        return 0 ;
    }
    else {
        printf("\nErase failed !!!!!!!!!!!!!!!!!!!!!!!!!! \n\n");
        return -1 ;
    }






}

#ifdef raus

void print_jtag_chain (void )
{
    unsigned int i ;
    unsigned int loc_jtg_dev_idcode ;

    printf("number of devices  = %d   \n\n",gl_jtg_nof_devices);
    for (i=0;i<gl_jtg_nof_devices;i++)  {
        loc_jtg_dev_idcode = gl_jtg_dev_idcodes[gl_jtg_nof_devices-i-1];
        printf("device %d :  dev_idcodes = 0x%08x       ",i+1,loc_jtg_dev_idcode);
        if ((loc_jtg_dev_idcode & 0x0fffffff) == 0x05026093) {
            printf("device = 18V04 \n");
        }    
        else {
          if ((loc_jtg_dev_idcode & 0x0fffffff) == 0x05036093) {
            printf("device = 18V04 (STM)\n");
          }    
          else {
            if ((loc_jtg_dev_idcode & 0x0fffffff) == 0x0061C093) {
              printf("device = XC2S200\n");
            }    
            else {
              if ((gl_jtg_dev_idcodes[i] & 0xFfffffff) == 0xa0a30093) {
                printf("device = XC2S600 \n");
              }    
              else {
                printf("unknown device  \n");
              }    
            }    
          }    
        }    
    }
}
#endif

void print_jtag_chain (void )
{
    unsigned int i, i_ux ;

    printf("number of devices  = %d   \n\n",gl_jtg_nof_devices);
    if (gl_jtg_nof_devices == 0) {
        printf("no device found  \n");
        printf("check Jumper JP570  (insert on 2-3)  \n");
    }
    else {
      for (i=0;i<gl_jtg_nof_devices;i++)  {
        i_ux = (gl_jtg_nof_devices - 1) - i ;
        //  loc_jtg_dev_idcode = gl_jtg_dev_idcodes[gl_jtg_nof_devices-i-1];
        printf("device %d :  dev_idcodes = 0x%08x       ",i+1,gl_jtg_dev_idcodes[i_ux]);

        if ((gl_jtg_dev_idcodes[i_ux] & 0x0fffffff) == 0x05026093) {
            printf("device = 18V04 \n");
            gl_jtg_ir_len[i_ux] = 8 ; /* valid only for 18v0x proms */
        }    
        else {
          if ((gl_jtg_dev_idcodes[i_ux] & 0x0fffffff) == 0x05036093) {
            printf("device = 18V04 (STM)\n");
            gl_jtg_ir_len[i_ux] = 8 ; /* valid only for 18v0x proms */
          }    
          else {
            if ((gl_jtg_dev_idcodes[i_ux] & 0x0fffffff) == 0x0061C093) {
              printf("device = XC2S200\n");
              gl_jtg_ir_len[i_ux] = 5 ; /* valid  for XC2S200 */
            }    
            else {
              if ((gl_jtg_dev_idcodes[i_ux] & 0x0fffffff) == 0x00a30093) {
                printf("device = XC2S600 \n");
                gl_jtg_ir_len[i_ux] = 5 ; /* valid XC2S600 */
              }    
              else {
                printf("unknown device  \n");
              }    
            }    
          }    
        }    
      }    
    }
}
   


























