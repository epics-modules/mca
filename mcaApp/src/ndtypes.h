/*******************************************************************************
*
* This module contains typedefs to declare standardized data types for
* use in portable applications. 
*
* Symbols which define the platform must be defined upon entry:
*
*	VAX for VMS or ULTRIX VAX systems
*	M_I86 for Intel 86 family systems
*	__ALPHA for Alpha systems
*
* If none of these is defined, types will be defined for a 32 bit processor.
*
********************************************************************************
*
* Revision History:
*
*	18-Apr-90	RJH	Original, starting with CISTD.H by Don Reichler
*	25-Jun-90	RJH	Removed definition of GENIE
*	28-Jun-90	RJH	Changed M86 SHORT and USHORT to match OS/2 typedefs
*	20-Mar-91	RJH	Move IFDEFs around so that there is a basic set
*				of types defined for unknown processors.
*	03-Mar-92	RJH	Added definitions for Meriden types 
*	02-Jul-92	RJH	Added section for Alpha
*
*******************************************************************************/

/*
* Declare standard data types for VAX (VMS and ULTRIX) systems
*/

#if defined(VAX)

typedef int LONG;			/* 32 bit signed integer */
typedef unsigned int ULONG;		/* 32 bit unsigned integer */
typedef short int SHORT;		/* 16 bit signed integer */
typedef unsigned short int USHORT;	/* 16 bit unsigned integer */
typedef char CHAR;			/* 8 bit signed integer */
typedef unsigned char UCHAR;		/* 8 bit unsigned integer */
typedef unsigned char UBYTE;		/* 8 bit unsigned integer */
typedef float REAL;			/* 32 bit floating point (DEC format) */
typedef double DOUBLE;			/* 64 bit floating point (IEEE format) */

typedef unsigned short FLAG;		/* Meriden types... */
typedef unsigned char FLAGB;
typedef unsigned short FLAGS;
typedef unsigned long FLAGL;
typedef unsigned short BOOL;
typedef int INT;
typedef long HMEM;


/*
* Declare standard data types for 86 family processors
*/

#elif defined(M_I86)

typedef long LONG;			/* 32 bit signed integer */
typedef unsigned long ULONG;		/* 32 bit unsigned integer */
typedef short SHORT;			/* 16 bit signed integer */
typedef unsigned short USHORT;		/* 16 bit unsigned integer */
typedef char CHAR;			/* 8 bit signed integer */
typedef unsigned char UCHAR;		/* 8 bit unsigned integer */
typedef unsigned char UBYTE;		/* 8 bit unsigned integer */
typedef float REAL;			/* 32 bit floating point (IEEE format) */
typedef double DOUBLE;			/* 64 bit floating point (IEEE format) */
#define HMEM SEL			/* DosAllocSeg handle */
typedef int INT;

/*
* Declare standard data types for Alpha (VMS and UNIX) systems
*/

#elif defined(__ALPHA)

typedef int LONG;			/* 32 bit signed integer */
typedef unsigned int ULONG;		/* 32 bit unsigned integer */
typedef short int SHORT;		/* 16 bit signed integer */
typedef unsigned short int USHORT;	/* 16 bit unsigned integer */
typedef char CHAR;			/* 8 bit signed integer */
typedef unsigned char UCHAR;		/* 8 bit unsigned integer */
typedef unsigned char UBYTE;		/* 8 bit unsigned integer */
typedef float REAL;			/* 32 bit floating point (DEC format) */
typedef double DOUBLE;			/* 64 bit floating point (IEEE format) */

typedef unsigned short FLAG;		/* Meriden types... */
typedef unsigned char FLAGB;
typedef unsigned short FLAGS;
typedef unsigned long FLAGL;
typedef unsigned short BOOL;
typedef int INT;
typedef long HMEM;

/*
* If the datatype definition falls thru to here, assume a 32 bit processor
*/

#else

typedef int LONG;			/* 32 bit signed integer */
/* typedef unsigned int ULONG;*/		/* 32 bit unsigned integer */
typedef short int SHORT;		/* 16 bit signed integer */
/* typedef unsigned short int USHORT; */	/* 16 bit unsigned integer */
typedef char CHAR;			/* 8 bit signed integer */
/* typedef unsigned char UCHAR; */		/* 8 bit unsigned integer */
typedef unsigned char UBYTE;		/* 8 bit unsigned integer */
typedef float REAL;			/* 32 bit floating point */
typedef double DOUBLE;			/* 64 bit floating point */

typedef unsigned short FLAG;		/* Meriden types... */
typedef unsigned char FLAGB;
typedef unsigned short FLAGS;
typedef unsigned long FLAGL;
/* typedef unsigned short BOOL; */
typedef int INT;
typedef long HMEM;

#endif


/*
* Put other, miscellaneous definitions here
*/

/*
* Define wierd stuff for Intel platforms here:
*
* HUGE: use this keyword when declaring a pointer to an object which could be
* larger then 65K bytes (i.e., spectral data arrays). It will generate the
* "huge" keyword on I86XXX platforms and nothing on the others.
*
* FAR, FARPASCAL, PASCAL: use when declaring "far" pointers and functions in 
* medium model.
*/

#ifdef M_I86

#define HUGE huge
#define FAR far
#define FARPASCAL far pascal
#define PASCAL pascal

#else

#define HUGE
#define FAR
#define FARPASCAL
#define PASCAL

#endif
