/* drvIcbAsyn.h */

#ifndef drvIcbAsynH
#define drvIcbAsynH

typedef enum {
    icbAdcType,
    icbAmpType,
    icbHvpsType,
    icbTcaType, 
    icbDspType
} icbModuleType;

/* NOTE: the following values must match the ADDR values in the database 
 * (.db) files */
typedef enum {
    icbAdcGain,   /* Conversion gain */
    icbAdcRange,  /* Conversion range */
    icbAdcOffset, /* Digital offset */
    icbAdcLld,    /* Lower-level discriminator */
    icbAdcUld,    /* Upper-level discriminator */
    icbAdcZero,   /* Zero offset */
    icbAdcGmod,   /* Gate mode */
    icbAdcCmod,   /* Coincidence mode */
    icbAdcPmod,   /* Peak-detect mode */
    icbAdcAmod,   /* Acquisition mode */
    icbAdcTmod,   /* Data transfer mode */
    icbAdcZeroRbv /* Zero offset readback */
} icbAdcCommand;

typedef enum {
    icbAmpCgain,  /* Coarse gain */
    icbAmpFgain,  /* Fine gain */
    icbAmpSfgain, /* Super-fine gain */
    icbAmpInpp,   /* Input polarity */
    icbAmpInhp,   /* Inhibit polarity */
    icbAmpDmod,   /* Differential mode */
    icbAmpSmod,   /* Shaping mode */
    icbAmpPtyp,   /* Preamp type */
    icbAmpPurmod, /* Pileup reject mode */
    icbAmpBlmod,  /* Baseline restore mode */
    icbAmpDtmod,  /* Deadtime mode */
    icbAmpAutoPz, /* Start auto-PZ */
    icbAmpPz,     /* Requested PZ */
    icbAmpShaping,/* Shaping time */
    icbAmpPzRbv   /* Actual PZ */
} icbAmpCommand;

typedef enum {
    icbHvpsVolt,   /* Output voltage */
    icbHvpsVlim,   /* Output voltage limit */
    icbHvpsInhl,   /* Inhibit level */
    icbHvpsLati,   /* Latch inhibit */
    icbHvpsLato,   /* Latch overload */
    icbHvpsReset,  /* Reset overload/inihibit */
    icbHvpsStatus, /* Status (off/on) */
    icbHvpsFramp,  /* Fast ramp */
    icbHvpsVpol,   /* Output voltage polarity */
    icbHvpsInh,    /* Inhibit */
    icbHvpsOvl,    /* Overload */
    icbHvpsStatRbv,/* Status readback */
    icbHvpsBusy,   /* Busy with ramp */
    icbHvpsVoltRbv /* Voltage readback */
} icbHvpsCommand;

typedef enum {
    icbTcaPolarity, /* Output polarity */
    icbTcaThresh,   /* ICR threshold */
    icbTcaScaEn,    /* SCA enable */
    icbTcaGate1,    /* SCA 1 gate */
    icbTcaGate2,    /* SCA 2 gate */
    icbTcaGate3,    /* SCA 3 gate */
    icbTcaSelect,   /* TCA select */
    icbTcaPurEn,    /* PUR enable */
    icbTcaPur1,     /* SCA 1 PUR */
    icbTcaPur2,     /* SCA 2 PUR */
    icbTcaPur3,     /* SCA 3 PUR */
    icbTcaPurAmp,   /* PUR AMP */
    icbTcaLow1,     /* SCA 1 low */
    icbTcaHi1,      /* SCA 1 hi */
    icbTcaLow2,     /* SCA 2 low */
    icbTcaHi2,      /* SCA 2 hi */
    icbTcaLow3,     /* SCA 3 low */
    icbTcaHi3,      /* SCA 3 hi */
    icbTcaStatus    /* Status readback */
} icbTcaCommand;

#endif /* drvMcaAsynH */

