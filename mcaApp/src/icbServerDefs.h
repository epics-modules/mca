/* Definitions for Canberra 9635 ADC, 9615 Amplifier and 9641 HVPS */

/* Author:  Mark Rivers 
 * Date:    October 4, 2000 
 */

#define ID_ADC          256
#define ID_ADC_GAIN     (ID_ADC+1)  /* Conversion gain */
#define ID_ADC_RANGE    (ID_ADC+2)  /* Conversion range */
#define ID_ADC_OFFSET   (ID_ADC+3)  /* Digital offset */
#define ID_ADC_LLD      (ID_ADC+4)  /* Lower-level discriminator */
#define ID_ADC_ULD      (ID_ADC+5)  /* Upper-level discriminator */
#define ID_ADC_ZERO     (ID_ADC+6)  /* Zero offset */
#define ID_ADC_GMOD     (ID_ADC+7)  /* Gate mode */
#define ID_ADC_CMOD     (ID_ADC+8)  /* Coincidence mode */
#define ID_ADC_PMOD     (ID_ADC+9)  /* Peak-detect mode */
#define ID_ADC_AMOD     (ID_ADC+10) /* Acquisition mode */
#define ID_ADC_TMOD     (ID_ADC+11) /* Data transfer mode */
#define ID_ADC_ZERORBV  (ID_ADC+12) /* Zero offset readback */

#define ID_AMP          512
#define ID_AMP_CGAIN    (ID_AMP+1)  /* Coarse gain */
#define ID_AMP_FGAIN    (ID_AMP+2)  /* Fine gain */
#define ID_AMP_SFGAIN   (ID_AMP+3)  /* Super-fine gain */
#define ID_AMP_INPP     (ID_AMP+4)  /* Input polarity */
#define ID_AMP_INHP     (ID_AMP+5)  /* Inhibit polarity */
#define ID_AMP_DMOD     (ID_AMP+6)  /* Differential mode */
#define ID_AMP_SMOD     (ID_AMP+7)  /* Shaping mode */
#define ID_AMP_PTYP     (ID_AMP+8)  /* Preamp type */
#define ID_AMP_PURMOD   (ID_AMP+9)  /* Pileup reject mode */
#define ID_AMP_BLMOD    (ID_AMP+10) /* Baseline restore mode */
#define ID_AMP_DTMOD    (ID_AMP+11) /* Deadtime mode */
#define ID_AMP_AUTO_PZ  (ID_AMP+12) /* Start auto-PZ */
#define ID_AMP_PZ       (ID_AMP+13) /* Requested PZ */
#define ID_AMP_SHAPING  (ID_AMP+14) /* Shaping time */
#define ID_AMP_PZRBV    (ID_AMP+15) /* Actual PZ */

#define ID_HVPS         1024
#define ID_HVPS_VOLT    (ID_HVPS+1)  /* Output voltage */
#define ID_HVPS_VLIM    (ID_HVPS+2)  /* Output voltage limit */
#define ID_HVPS_INHL    (ID_HVPS+3)  /* Inhibit level */
#define ID_HVPS_LATI    (ID_HVPS+4)  /* Latch inhibit */
#define ID_HVPS_LATO    (ID_HVPS+5)  /* Latch overload */
#define ID_HVPS_RESET   (ID_HVPS+6)  /* Reset overload/inihibit */
#define ID_HVPS_STATUS  (ID_HVPS+7)  /* Status (off/on) */
#define ID_HVPS_FRAMP   (ID_HVPS+8)  /* Fast ramp */
#define ID_HVPS_VPOL    (ID_HVPS+9)  /* Output voltage polarity */
#define ID_HVPS_INH     (ID_HVPS+10) /* Inhibit */
#define ID_HVPS_OVL     (ID_HVPS+11) /* Overload */
#define ID_HVPS_STATRBV (ID_HVPS+12) /* Status readback */
#define ID_HVPS_BUSY    (ID_HVPS+13) /* Busy with ramp */
#define ID_HVPS_VOLTRBV (ID_HVPS+14) /* Voltage readback */
