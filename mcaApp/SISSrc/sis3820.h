/***************************************************************************/
/*                                                                         */
/*  Filename: sis3820.h                                                    */
/*                                                                         */
/*  Funktion: headerfile for SIS3820                                       */
/*                                                                         */
/*  Autor:                TH                                               */
/*  date:                 13.06.2003                                       */
/*  last modification:    08.12.2003                                       */
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
/***************************************************************************/


/* addresses  */ 

#define SIS3820_CONTROL_STATUS                  0x0                        /* read/write; D32 */
#define SIS3820_MODID                           0x4                        /* read only; D32 */
#define SIS3820_IRQ_CONFIG                      0x8                        /* read/write; D32 */
#define SIS3820_IRQ_CONTROL                     0xC                        /* read/write; D32 */

#define SIS3820_ACQUISITION_PRESET              0x10                /* read/write; D32 */
#define SIS3820_ACQUISITION_COUNT               0x14                /* read  D32 */

#define SIS3820_LNE_PRESCALE                    0x18                /* read/write; D32 */


#define SIS3820_PRESET_GROUP1                   0x20                /* read/write; D32 */
#define SIS3820_PRESET_GROUP2                   0x24                /* read/write; D32 */
#define SIS3820_PRESET_ENABLE_HIT               0x28                /* read/write; D32 */

#define SIS3820_CBLT_BROADCAST_SETUP            0x30                /* read/write; D32 */
#define SIS3820_SDRAM_PAGE                      0x34                /* read/write; D32 */
#define SIS3820_FIFO_WORDCOUNTER                0x38                /* read; D32 */
#define SIS3820_FIFO_WORDCOUNT_THRESHOLD        0x3C                /* read/write; D32 */

#define SIS3820_HISCAL_START_PRESET             0x40                /* read/write; D32 */
#define SIS3820_HISCAL_COUNT                    0x44                /* read  D32 */
#define SIS3820_HISCAL_LAST_ACQ_COUNT           0x48                /* read  D32 */


#define SIS3820_OPERATION_MODE                  0x100                /* read/write; D32 */
#define SIS3820_COPY_DISABLE                    0x104                /* read/write; D32 */
#define SIS3820_LNE_CHANNEL_SELECT              0x108                /* read/write; D32 */
#define SIS3820_PRESET_CHANNEL_SELECT           0x10C                /* read/write; D32 */

#define SIS3820_COUNTER_INHIBIT                 0x200                /* read/write; D32 */        
#define SIS3820_COUNTER_CLEAR                   0x204                /* write only; D32 */
#define SIS3820_COUNTER_OVERFLOW                0x208                /* read/write; D32 */


#define SIS3820_COUNTER_OVERFLOW                0x208                /* read/write; D32 */


#define SIS3820_SDRAM_EEPROM_CTRL_STAT          0x300                /* read/write; D32 */

#define SIS3820_JTAG_TEST                       0x310
#define SIS3820_JTAG_CONTROL                    0x314
#define SIS3820_JTAG_DATA_IN                    0x310




#define SIS3820_KEY_RESET                       0x400
#define SIS3820_KEY_SDRAM_FIFO_RESET            0x404
#define SIS3820_KEY_TEST_PULSE                  0x408
#define SIS3820_KEY_COUNTER_CLEAR               0x40C

#define SIS3820_KEY_LNE_PULSE                   0x410
#define SIS3820_KEY_OPERATION_ARM               0x414
#define SIS3820_KEY_OPERATION_ENABLE            0x418
#define SIS3820_KEY_OPERATION_DISABLE           0x41C

#define SIS3820_KEY_HISCAL_START_PULSE          0x420
#define SIS3820_KEY_HISCAL_ENABLE_LNE_ARM       0x424
#define SIS3820_KEY_HISCAL_ENABLE_LNE_ENABLE    0x428
#define SIS3820_KEY_HISCAL_DISABLE              0x42C


#define SIS3820_COUNTER_SHADOW_CH1              0x800
#define SIS3820_COUNTER_CH1                     0xA00

#define SIS3820_FIFO_BASE                       0x800000
#define SIS3820_SDRAM_BASE                      0x800000



/* version and default values */
#define SIS3820_ACTUAL_VERSION                  0x38200101                
#define SIS3820_ACTUAL_VERSION_38200102         0x38200102                

/* bit definitions  */
                

#define SIS3820_IRQ_SOURCE0_ENABLE              0x1
#define SIS3820_IRQ_SOURCE1_ENABLE              0x2
#define SIS3820_IRQ_SOURCE2_ENABLE              0x4
#define SIS3820_IRQ_SOURCE3_ENABLE              0x8
#define SIS3820_IRQ_SOURCE4_ENABLE              0x10
#define SIS3820_IRQ_SOURCE5_ENABLE              0x20
#define SIS3820_IRQ_SOURCE6_ENABLE              0x40
#define SIS3820_IRQ_SOURCE7_ENABLE              0x80

#define SIS3820_IRQ_SOURCE0_DISABLE             0x100
#define SIS3820_IRQ_SOURCE1_DISABLE             0x200
#define SIS3820_IRQ_SOURCE2_DISABLE             0x400
#define SIS3820_IRQ_SOURCE3_DISABLE             0x800
#define SIS3820_IRQ_SOURCE4_DISABLE             0x1000
#define SIS3820_IRQ_SOURCE5_DISABLE             0x2000
#define SIS3820_IRQ_SOURCE6_DISABLE             0x4000
#define SIS3820_IRQ_SOURCE7_DISABLE             0x8000

#define SIS3820_IRQ_SOURCE0_CLEAR               0x10000
#define SIS3820_IRQ_SOURCE1_CLEAR               0x20000
#define SIS3820_IRQ_SOURCE2_CLEAR               0x40000
#define SIS3820_IRQ_SOURCE3_CLEAR               0x80000
#define SIS3820_IRQ_SOURCE4_CLEAR               0x100000
#define SIS3820_IRQ_SOURCE5_CLEAR               0x200000
#define SIS3820_IRQ_SOURCE6_CLEAR               0x400000
#define SIS3820_IRQ_SOURCE7_CLEAR               0x800000

#define SIS3820_IRQ_SOURCE0_FLAG                0x1000000
#define SIS3820_IRQ_SOURCE1_FLAG                0x2000000
#define SIS3820_IRQ_SOURCE2_FLAG                0x4000000
#define SIS3820_IRQ_SOURCE3_FLAG                0x8000000
#define SIS3820_IRQ_SOURCE4_FLAG                0x10000000
#define SIS3820_IRQ_SOURCE5_FLAG                0x20000000
#define SIS3820_IRQ_SOURCE6_FLAG                0x40000000
#define SIS3820_IRQ_SOURCE7_FLAG                0x80000000

#define SIS3820_FLAG_SOURCE0                    0x10000
#define SIS3820_FLAG_SOURCE1                    0x20000
#define SIS3820_FLAG_SOURCE2                    0x40000
#define SIS3820_FLAG_SOURCE3                    0x80000
#define SIS3820_FLAG_SOURCE4                    0x100000
#define SIS3820_FLAG_SOURCE5                    0x200000
#define SIS3820_FLAG_SOURCE6                    0x400000
#define SIS3820_FLAG_SOURCE7                    0x800000

/* Control register bit defintions */

#define CTRL_USER_LED_OFF                       0x10000           /* default after Reset */
#define CTRL_USER_LED_ON                        0x1

#define CTRL_COUNTER_TEST_25MHZ_DISABLE         0x100000
#define CTRL_COUNTER_TEST_25MHZ_ENABLE          0x10

#define CTRL_COUNTER_TEST_MODE_DISABLE          0x200000
#define CTRL_COUNTER_TEST_MODE_ENABLE           0x20

#define CTRL_REFERENCE_CH1_DISABLE              0x400000
#define CTRL_REFERENCE_CH1_ENABLE               0x40


/* Status register bit defintions */

#define STAT_OPERATION_SCALER_ENABLED           0x10000
#define STAT_OPERATION_MCS_ENABLED              0x40000
#define STAT_OPERATION_VME_WRITE_ENABLED        0x800000



/* Acqusition / Mode register bit defintions */
#define SIS3820_CLEARING_MODE                   0x0
#define SIS3820_NON_CLEARING_MODE               0x1

#define SIS3820_MCS_DATA_FORMAT_32BIT           0x0
#define SIS3820_MCS_DATA_FORMAT_24BIT           0x4
#define SIS3820_MCS_DATA_FORMAT_16BIT           0x8
#define SIS3820_MCS_DATA_FORMAT_8BIT            0xC

#define SIS3820_SCALER_DATA_FORMAT_32BIT        0x0
#define SIS3820_SCALER_DATA_FORMAT_24BIT        0x4

#define SIS3820_LNE_SOURCE_VME                  0x0
#define SIS3820_LNE_SOURCE_CONTROL_SIGNAL       0x10
#define SIS3820_LNE_SOURCE_INTERNAL_10MHZ       0x20
#define SIS3820_LNE_SOURCE_CHANNEL_N            0x30
#define SIS3820_LNE_SOURCE_PRESET               0x40

#define SIS3820_ARM_ENABLE_CONTROL_SIGNAL       0x000
#define SIS3820_ARM_ENABLE_CHANNEL_N            0x100

#define SIS3820_FIFO_MODE                       0x0000
#define SIS3820_SDRAM_MODE                      0x1000
#define SIS3820_SDRAM_ADD_MODE                  0x2000
#define SIS3820_HISCAL_START_SOURCE_VME         0x0000
#define SIS3820_HISCAL_START_SOURCE_EXTERN      0x4000

#define SIS3820_CONTROL_INPUT_MODE0             0x00000
#define SIS3820_CONTROL_INPUT_MODE1             0x10000
#define SIS3820_CONTROL_INPUT_MODE2             0x20000
#define SIS3820_CONTROL_INPUT_MODE3             0x30000
#define SIS3820_CONTROL_INPUT_MODE4             0x40000
#define SIS3820_CONTROL_INPUT_MODE5             0x50000

#define SIS3820_CONTROL_INPUTS_INVERT           0x80000

#define SIS3820_CONTROL_OUTPUT_MODE0            0x000000
#define SIS3820_CONTROL_OUTPUT_MODE1            0x100000

#define SIS3820_CONTROL_OUTPUTS_INVERT          0x800000


#define SIS3820_OP_MODE_SCALER                  0x00000000
#define SIS3820_OP_MODE_MULTI_CHANNEL_SCALER    0x20000000
#define SIS3820_OP_MODE_VME_FIFO_WRITE          0x70000000




/* preset enable/hit register */
#define SIS3820_PRESET_STATUS_ENABLE_GROUP1       0x1
#define SIS3820_PRESET_REACHED_GROUP1             0x2
#define SIS3820_PRESET_LNELATCHED_REACHED_GROUP1  0x4
#define SIS3820_PRESET_STATUS_ENABLE_GROUP2       0x10000
#define SIS3820_PRESET_REACHED_GROUP2             0x20000
#define SIS3820_PRESET_LNELATCHED_REACHED_GROUP2  0x40000



/* preset enable/hit register */

#define SIS3820_SDRAM_EEPROM_SCL                0x1
#define SIS3820_SDRAM_EEPROM_SDA_OUT            0x2
#define SIS3820_SDRAM_EEPROM_SDA_OE             0x4

#define SIS3820_SDRAM_EEPROM_SDA_IN             0x100

