/* Definitions for Canberra 9660 DSP */

/* Author:  Mark Rivers 
 * Date:    July 23, 2000 
 */

/* Maximum retries for done reponse from module.  Each poll waits DELAY_9660
 *   seconds */
# define MAX_9660_POLLS 2 
# define DELAY_9660 0.05

/* Bit definitions in R6 */
#define R6_FAIL  0x01
#define R6_MERR  0x02
#define R6_MBUSY 0x04
#define R6_ABUSY 0x08
#define R6_WDONE 0x40
#define R6_RDAV  0x40
#define R6_DXIP  0x80
 
/* Parameter opcodes */
#define ID_AMP_CGAIN    0x01 
    /* R/W Amplifier Coarse Gain Index, 0-5 */
#define ID_AMP_FGAIN   0x2A 
    /* R/W Amplifier Fine Gain DAC value, 000-FFFh */
#define ID_AMP_SFGAIN   0x2B
    /* R/W Amplifier Super Fine Gain DAC value, 000-FFFh */

#define ID_ADC_CGAIN    0x31 
    /* R/W ADC Conversion Gain index, 0-6 representing 256-16k channels */
#define ID_ADC_CRANGE   0x32 
    /* R/W ADC Conversion Range index, 0-6 representing 256-16k channels */
#define ID_ADC_OFFS 0x33 
    /* R/W ADC Offset, 0 through 126 representing 0 through 16128 channels  */
#define ID_ADC_TYPE 0x17 
    /* R/W MCA Type index, 0-2 */
#define ID_ADC_LLD  0x30 
    /* R/W ADC LLD DAC value, 000-7FFFh */
#define ID_ADC_ZERO 0x2C 
    /* R/W ADC Zero DAC value 0000-186Ah */
            
#define ID_FILTER_RT    0x35 
    /* R/W Digital Filter Rise Time index, 0 through 28 */
#define ID_FILTER_FT    0x36 
    /* R/W Digital Filter Flat Top index, 0 through 20  */
#define ID_FILTER_MDEX  0x3C 
    /* R/W M-term Index 0-2 for decay time selection, where 0= 2. 
           0us, 1 and 2 are reserved */
#define ID_FILTER_MFACT 0x2F 
    /* R/W M-Factor value for selected M-term  */
#define ID_FILTER_BLRM  0x06 
    /* R/W BLR mode index, 0-3  */
#define ID_FILTER_PZM   0x08 
    /* R/W Pole Zero mode, 0-2 */
#define ID_FILTER_PZ    0x07 
    /* R/W Pole Zero DAC setting, 000-FFFh */
#define ID_FILTER_THR   0x39 
    /* R/W Filter Threshold mode and and DAC value, 0000-FFFFh */
#define ID_FILTER_FRQ   0x3A 
    /* R/W Filter Cutoff Frequency index, 0 through 20 */
            
#define ID_STABLZ_GCOR  0x22 
    /* R/W Stabilizer Gain Correction Range, 0-1 */
#define ID_STABLZ_ZCOR  0x2D 
    /* R/W Stabilizer Zero Correction Range, 0-1 */
#define ID_STABLZ_GMOD  0x20 
    /* R/W Stabilizer Gain Mode index, 0-2 */
#define ID_STABLZ_ZMOD  0x21 
    /* R/W Stabilizer Zero Mode index, 0-2 */
#define ID_STABLZ_GDIV  0x37 
    /* R/W Stabilizer Gain Correction Divider, 0-9 representing 2^N divider */
#define ID_STABLZ_ZDIV  0x38 
    /* R/W Stabilizer Zero Correction Divider, 0-9 representing 2^N divider */
#define ID_STABLZ_GSPAC 0x1C 
    /* R/W Stabilizer Gain Spacing, 2-512 channels */
#define ID_STABLZ_ZSPAC 0x1D 
    /* R/W Stabilizer Zero Spacing, 2-512 channels */
#define ID_STABLZ_GWIN  0x1A 
    /* R/W Stabilizer Gain Window, 1-128 channels */
#define ID_STABLZ_ZWIN  0x1B 
    /* R/W Stabilizer Zero Window, 1-128 channels */
#define ID_STABLZ_GCENT 0x18 
    /* R/W Stabilizer Gain Centroid, 10 through 16376 channels */
#define ID_STABLZ_ZCENT 0x19 
    /* R/W Stabilizer Zero Centroid, 10 through 16376 channels */
#define ID_STABLZ_GRAT  0x28 
    /* R/W Stabilizer Gain Ratio value 1-10000 as x100, representing 
           0.01 to 100.00 */
#define ID_STABLZ_ZRAT  0x29 
    /* R/W Stabilizer Zero Ratio value 1-10000 as x100, representing 
           0.01 to 100.00 */
#define ID_STABLZ_RESET 0x23 
    /* W   Reset stabilizer  */
#define ID_STABLZ_GAIN  0x3E 
    /* R/W Stabilizer Gain correction value, 0000-1FFFh */
#define ID_STABLZ_ZERO  0x3F 
    /* R/W Stabilizer Zero correction value, 000-FFFh */
            
#define ID_MISC_FDM 0x13 
    /* R/W Fast Discriminator Mode, 0-1 */
#define ID_MISC_FD  0x34 
    /* R/W Fast Discriminator DAC setting, 0 through 1000 representing 
           0.0% through 100.0% */
#define ID_MISC_INPP    0x0E 
    /* R/W Input Polarity mode, 0-1 */
#define ID_MISC_INHP    0x0F 
    /* R/W Inhibit Polarity mode, 0-1 */
#define ID_MISC_PURM    0x10 
    /* R/W PUR Mode, 0-2 */
#define ID_MISC_GATM    0x11 
    /* R/W Gate Mode, 0-1 */
#define ID_MISC_OUTM    0x12 
    /* R/W Output Mode, 0-1 */
#define ID_MISC_GD  0x16 /* R/W Guard Setting, 11  through 25 representing 
    x1.1 through x2.5  */
#define ID_MISC_TINH    0x3D 
    /* R/W TRP Inhibit mode, 0-1 */
#define ID_MISC_LTRIM   0x14 
    /* R/W Live-Time Trim, 0-1000 */
#define ID_MISC_BBRN    0x15 
    /* R/W Burr-Brown Delay, 0-1 */
            
#define ID_INFO_TDAC    0x3B 
    /* R/W Test DAC index, 0-3 */
#define ID_INFO_THRP    0x24 
    /* R   Throughput register value for current index */
#define ID_INFO_THRI    0x25 
    /* R/W Throughput register index, 0-3 */
#define ID_INFO_PZ  0x26 
    /* R   Last result from Auto-PZ process */
#define ID_INFO_BDC 0x27 
    /* R   Last result from Auto-BDC process */
            
#define ID_STATUS_FLGS  0x46 
    /* R   P/Z Busy, BDC Busy, spares */

#define ID_SYSOP_CMD    0x54 
    /* W   System command to activate system-level commands, such
           as: 
            · Abort AutoBDC
            · Start AutoPZ process
            · Start AutoBDC process
            · Save flash parameters
            · Lock/Unlock front panel functions
            · LEDs on/off
            · Update hardware from table #0
            · Initialize
            · Write to any memory-mapped location
            · Write FPGA Digital Filter registers (G, H, etc)
            · Execute specific diagnositc
            · AutoBDC data calculations
            · Reading of Y[0-19] bus value
            · Reading of calculated threshold value
            · Reading of self diagnostic result
            · Reading of front-panel switch settings
            · Reading of any memory-mapped location
            · Reading of module's program version & other info
            · etc */
#define ID_SYSOP_RDATL  0x55 
    /* R   System command to read system data LSB for test purposes */
#define ID_SYSOP_RDATH  0x56 
    /* R   System command to read system data MSB for test purposes */
#define ID_SYSOP_LCD    0x57 
    /* W   System command to write data to LCD display */
#define ID_SYSOP_DAC    0x58 
    /* W   System command to write data to hardware DACs */


/* These commands are added to make things simpler for sub-commands of
 * ID_SYSOP_CMD.  These values must be different from any above.
 * These are not commands which are recognized by the DSP 9660.
 */
#define ID_START_PZ     0xF0
#define ID_START_BDC    0xF1
#define ID_CLEAR_ERRORS 0xF2

/* These commands are for individual bits or other items. 
 * These are not commands which are recognized by the DSP 9660 
 */
#define ID_STABLZ_GOVR  0xF4
#define ID_STABLZ_GOVF  0xF5
#define ID_STABLZ_ZOVR  0xF6
#define ID_STABLZ_ZOVF  0xF7
#define ID_INFO_PZAER   0xF8
#define ID_INFO_BDCAER  0xF9
#define ID_STATUS_PZBSY 0xFA
#define ID_STATUS_BDBSY 0xFB
#define ID_STATUS_MINT  0xFC
#define ID_STATUS_DGBSY 0xFD
#define ID_STATUS_MERR  0xFE
#define ID_INFO_DT      0xFF
#define ID_INFO_ICR     0x100
#define ID_INFO_DSPRATE 0x101
#define ID_INFO_MCARATE 0x102
