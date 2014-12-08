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



#if !defined _DEBUG 
  #define _DEBUG
#endif
#if !defined PLX_9054 
  #define PLX_9054
#endif
#if !defined PCI_CODE 
  #define PCI_CODE
#endif
#if !defined LITTLE_ENDIAN 
  #define LITTLE_ENDIAN
#endif



#include "C:\sis1100_3100_win2k\win2000_driver\sis1100_win2k_utils\PLX\PlxApi.h"
#include "C:\sis1100_3100_win2k\win2000_driver\sis1100_win2k_utils\PLX\Reg9054.h"


#include <conio.h>

/*===========================================================================*/
/* Headers								     */
/*===========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
    

#include "c:\sis1100_3100_win2k\win2000_driver\sis1100_win2k_utils\sis1100_lib\sis3100_vme_calls.h"
#include "c:\sis1100_3100_win2k\win2000_driver\sis1100_win2k_utils\sis1100_lib\sis9200.h"


#include "c:\sis1100_3100_win2k\win2000_driver\sis1100_win2k_utils\sis1100_lib\sis1100w.h" // Header sis1100w.dll

#include "..\header\sis3820.h" // Header for sis3820




/*===========================================================================*/
/* Globals					  			     */
/*===========================================================================*/

#define SIS3820_JTAG_TEST		0x310
#define SIS3820_JTAG_CONTROL	0x314
#define SIS3820_JTAG_DATA_IN	0x310

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

unsigned int gl_jtg_nof_devices ;
unsigned int gl_jtg_select ;
int gl_jtg_state ;
unsigned int gl_jtg_dev_idcodes[MAX_NOF_JTG_DEVS] ;
unsigned int gl_jtg_ir_len[MAX_NOF_JTG_DEVS] ;


unsigned char gl_wbuf[0x100000]; /* 1MByte ;  min. 512 KByte for one 18v04 */
unsigned char gl_rbuf[0x100000]; /* 1MByte ;  min. 512 KByte for one 18v04 */





#define MAX_NUMBER_OF_PRINTS 0x10      
#define MAX_NUMBER_LWORDS 0x1000000       /* 64MByte */

u_int32_t wblt_data[MAX_NUMBER_LWORDS] ;
u_int32_t rblt_data[MAX_NUMBER_LWORDS] ;

u_int32_t test1_data[16] ;



struct SIS1100_Device_Struct  sis1100_device1 ;
struct SIS1100_Device_Struct  gl_sis1100_device ;




/*===========================================================================*/
/* Prototypes					  			     */
/*===========================================================================*/
int mcs_file_unpack (char* fname, char* data_buffer, unsigned int* mcs_data_byte_length) ;

int jtag_tap_reset (unsigned int base_addr) ;

int jtag_reset (unsigned int base_addr) ;
int jtag_instruction(unsigned int base_addr, unsigned int icode) ;
unsigned int jtag_data(unsigned int base_addr, unsigned int din, unsigned int len) ;
int jtag_rd_data(unsigned int base_addr, void *buf, unsigned int len, int state) ;
int jtag_wr_data(unsigned int base_addr, void  *buf, unsigned int len, int   state) ;


int jtag_program_verifier (unsigned int base_addr, char* fname, int program) ;
int jtag_erase (unsigned int base_addr);

void print_jtag_chain (void ) ;





/*===========================================================================*/
/* Main      					  	                     */
/*===========================================================================*/
int main(int argc, char* argv[]) {
unsigned int i;

unsigned int mod_base   ;
unsigned int result ;
unsigned int loc_jtg_dev_idcode ;


int return_code ;

unsigned int error  ;
char line_in[128];
char new_mcs_file_name[128];


	int sis_ret ;
	U32 found_no_sis1100 ;
	U32 use_sis1100_no ;
	U32 sis1100_PciBus ;
	U32 sis1100_PciSlot ;


	U32 error_counter = 0;
	U32 data ;
	U32 addr ;

	int choose ;
 	char* mcs_file_name[128] ;


   
/***********************************************************************************************/
/***********************************************************************************************/
/********************                                                     **********************/
/********************                                                     **********************/
/********************                                                     **********************/
/********************    SIS3100 setup                                    **********************/
/********************                                                     **********************/
/********************                                                     **********************/
/********************                                                     **********************/
/***********************************************************************************************/
/***********************************************************************************************/


	sis_ret = sis1100w_Find_No_Of_sis1100(&found_no_sis1100) ;
    printf("sis_ret = 0x%08x     sis1100w_Find_No_of_sis1100 = %d  \n",sis_ret, found_no_sis1100) ;

    if (found_no_sis1100 == 0) exit(-1) ;

	for (i=1;i<=found_no_sis1100;i++) {
		sis_ret = sis1100w_Get_PciSlot_Information(i, &sis1100_PciBus, &sis1100_PciSlot) ;
    	printf("sis_ret = 0x%08x     sis1100w_Get_PciSlot_Information:   i = %d  \n",sis_ret, i) ;
    	printf("sis1100_PciBus = 0x%08x \n",sis1100_PciBus) ;
    	printf("sis1100_PciSlot = 0x%08x \n",sis1100_PciSlot) ;
    	printf("\n") ;
	}


	use_sis1100_no = found_no_sis1100 ; // use last one which was founded
    if ((sis1100w_Get_Handle_And_Open(use_sis1100_no, &sis1100_device1))!=0) exit(-1);


   	printf("\n") ;
    if ((sis1100w_Init(&sis1100_device1, 1 /* enable_logfile */)) !=0) {
    	printf("failed to open SIS1100/3100 \n") ;
 		/*  exit(-1); */
 	}
 	 else
 	 {
    	printf("open and init SIS1100  OK  \n") ;
 	  /*	exit(-1);*/
  	 }

    sis1100w_Init_sis3100(&sis1100_device1, 1 /* enable_logfile */) ; 


    gl_sis1100_device = sis1100_device1 ;



/***********************************************************************************************/
/***********************************************************************************************/
/********************                                                     **********************/
/********************                                                     **********************/
/********************                                                     **********************/
/********************    TEST setup                                       **********************/
/********************                                                     **********************/
/********************                                                     **********************/
/********************                                                     **********************/
/***********************************************************************************************/
/***********************************************************************************************/





	
	mod_base = 0x38000000 ;
	printf("\n\n");
	printf("\n\n");
	printf("SIS3820 VME Baseaddress (default)     = 0x38000000   \n");

	*mcs_file_name = "prom_v010a.mcs" ;
	printf("SIS3820 JTAG MCS-file name (default)  = %s \n\n",*mcs_file_name);

	
	addr=mod_base + SIS3820_MODID ;
	return_code = sis1100w_Vme_Single_Read(&sis1100_device1, addr, 0x9, 4, &data) ;
	if (return_code != 0) {
		printf("sis1100w_Vme_Single_Read: return_code = 0x%08x  at address = 0x%08x\n", return_code, addr );
		return -1 ;
	  }
	printf("\n\nSIS3820_MODID =  0x%08x    \n", data);   
	
    
	do { /* key hit */

	  printf("\n");
	  printf("\n");
	  printf("SIS3820 VME Baseaddress       = 0x%08x   \n",mod_base);

	  printf("SIS3820 JTAG MCS-file name    = %s \n\n",*mcs_file_name);

	
	  addr=mod_base + SIS3820_MODID ;
	  return_code = sis1100w_Vme_Single_Read(&sis1100_device1, addr, 0x9, 4, &data) ;
	  if (return_code != 0) {
		printf("sis1100w_Vme_Single_Read: return_code = 0x%08x  at address = 0x%08x\n", return_code, addr );
		return -1 ;
	  }
	  printf("\n");
	  printf("SIS3820_MODID                 = 0x%08x    \n", data);   


	do {
		printf( "  \n\n\n\n");
		printf("SIS3820_XILINX_JTAG              05.01.2006  \n");   
		printf("-------------------------------------------  \n\n");   
		printf( "Quit Program  ........................   0  \n");
		printf( "Change VME Base Address ..............   1  \n");
		printf( "Change Default MCS-file ..............   2  \n");
		printf( "Read IDCODEs .........................   3  \n");
		printf( "Verify ...............................   4  \n");
		printf( "Program,Verify Prom and Load FPGA ....   5  \n");
		printf( "\n\nEnter your Selection : ");
		result = scanf( "%x", &choose );
	} while ((choose > 5) || (choose < 0)) ;



    
	if (choose == 1) {
		printf("\n\n\n\n");
		printf( "\n\nEnter  SIS3820 VME Base Address     0x");
		result = scanf( "%x", &mod_base );
		mod_base = mod_base & 0xff000000 ;
		printf("\n\nNew SIS3820 VME Baseaddress   = 0x%08x\n",mod_base);

		printf( "\n\nEnter character");
		result = scanf( "%c%c", line_in );
	}

    

	if (choose == 2) {
		printf("\n\n\n\n");
		printf( "\n\nEnter  SIS3820 MCS-File name (name.mcs): ");
		result = scanf("%s", new_mcs_file_name  );
		*mcs_file_name = new_mcs_file_name ;
		printf("\n\nNew SIS3820 JTAG MCS-file name = %s \n\n",*mcs_file_name);
		printf( "\n\nEnter character");
		result = scanf( "%c%c", line_in );
	}

	

	
	if (choose == 3) {
		printf("\n\n\n\n");

		gl_jtg_select = 0 ;
		jtag_reset (mod_base) ;
		print_jtag_chain();
		jtag_tap_reset (mod_base) ;

		printf( "\n\nEnter character");
 		result = scanf( "%c%c", line_in );
	}


    
	if (choose == 4) {
		printf("\n\n\n\n");
		return_code = jtag_program_verifier (mod_base, *mcs_file_name,0) ;
		if (return_code != 0) {
			printf("Abort  \n");
		}
		jtag_tap_reset (mod_base) ;
		printf( "\n\nEnter character");
		result = scanf( "%c%c", line_in );
	}


	if (choose == 5) {
		printf("\n\n\n\n");
		return_code = jtag_program_verifier (mod_base, *mcs_file_name,1) ;
		if (return_code == 0) {
			jtag_instruction(mod_base, CONFIG);
			printf("Load FPGA   \n");
		}
		else {
			printf("Abort  \n");
		}
		printf( "\n\nEnter character");
		result = scanf( "%c%c", line_in );
	}


	
	} while (choose!=0) ;// key hit




/******************************************************************/
/*                                                                */
/*                                                                */
/*                                                                */
/******************************************************************/

	
/* reset  */
	addr = mod_base + SIS3820_KEY_RESET ;
	return_code = vme_A32D32_write(&gl_sis1100_device, addr, 0x0) ;
	if (return_code != 0) {
      printf("vme_A32D32_write:  return_code = 0x%08x  at address = 0x%08x\n", return_code, addr );
      return -1 ;
    }




   /* close VME environment */
	if(error != 0) {
		printf( "\n\nEnter character to leave program");
		result = scanf( "%s", line_in );
	}
	sis1100w_Close(&sis1100_device1) ;

   return 0;
}

/* end main */














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
  unsigned int data_array[0x20] ;
  unsigned int i_ux ;
  unsigned int req_nof_data, put_nof_data ;

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
	  for (i_ux=0;i_ux<0x20;i_ux++) {data_array[i_ux] = 0 ;}
	  req_nof_data = 0x20 ;
	  addr = base_addr + SIS3820_JTAG_TEST ;
	  return_code = vme_A32DMA_D32FIFO_write(&gl_sis1100_device, addr, data_array, req_nof_data, &put_nof_data) ;
	  if (return_code != 0) { printf("vme_A32DMA_D32FIFO_write: ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}

#endif


      addr = base_addr + SIS3820_JTAG_DATA_IN ;
      return_code =  vme_A32D32_read(&gl_sis1100_device, addr, &data) ;
      if (return_code != 0) {printf("vme_A32D32_read:ret_code=0x%08x at addr=0x%08x\n",return_code,addr);return -1;}
	  *bf++ = data ;
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
	  *bf++ = data ;

   
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
            din=*bf++; ux=32;
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
	unsigned int nof_byte_in_record	;	
	unsigned int byte_address_in_record	 ;	
	unsigned int type_in_record ;
	unsigned int extended_address_in_record ;
	unsigned char databyte_in_record ;
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
				}	;

				
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
				/* printf("wr_buf_index                  = 0x%08x\n",wr_buf_index);  /*   */
				/* printf("data_buffer[wr_buf_index-1]   = 0x%08x\n",data_buffer[wr_buf_index-1]);  /*   */
				/* printf("data_buffer[wr_buf_index]     = 0x%08x\n",data_buffer[wr_buf_index]);  /*   */
				/* printf("data_buffer[wr_buf_index+1]   = 0x%08x\n",data_buffer[wr_buf_index+1]);  /*   */
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
				}	;
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
		//} while (i < 1000);   /* min  500 */
		//} while (i < 200000);   /* ca. 0.140.000 us   --> Fehler*/
		} while (i < 2000000);   /* ca. 5 Sec */

		printf("\nErase finished \n");

/*		jtag_instruction(base_addr, NORMRST); */

/*		ret=jtag_instruction(base_addr, BYPASS);          
/*		if (ret != 0x01) printf("BYPASS.0 %02X\n", ret);   */

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

  

 
   
/*	retcode=mcs_file_unpack (fname, gl_wbuf, mcs_buffer_length) ; */
   

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
	//	if(error_counter >= max_error_value) {break; } 
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
    int retcode=1;

	
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

/*		jtag_instruction(base_addr, NORMRST); */

/*		ret=jtag_instruction(base_addr, BYPASS);          
/*		if (ret != 0x01) printf("BYPASS.0 %02X\n", ret);   */

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
   


























