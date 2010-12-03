#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ndtypes.h"

#define OK 0
	
/*******************************************************************************
*
* ICB_DSC2STR converts a descriptor into a string.
*
* Its calling format is:
*
*	status = ICB_DSC2STR (str, dsc, max_len);
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "str"  (CHAR *) the destination zero terminated string.
*
*  "dsc"  (struct dsc$descriptor *) is the source descriptor
*
*  "str"  (LONG) is the max length of the string.
*
*******************************************************************************/

int icb_dsc2str (CHAR *str,
	     CHAR *dsc,
	     LONG max_len)
{

/*
* If the length of the descriptor is less than the max length of the string,
* use the length of the descriptor.
*/
	if (max_len > strlen(dsc)) max_len = strlen(dsc);
	strncpy (str, dsc, max_len);

	return OK;
}


/*******************************************************************************
*
* ICB_STR2DSC converts a string into a descriptor.
*
* Its calling format is:
*
*	status = ICB_STR2DSC (dsc, str);
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "dsc"  (struct dsc$descriptor *) is the destination descriptor
*
*  "str"  (CHAR *) the source zero terminated string.
*
*******************************************************************************/

int icb_str2dsc (CHAR *dsc,
	     CHAR *str)
{

/*
* If the length of the descriptor is less than the length of the string,
* use the length of the descriptor.
  NOTE - THIS TEST IS NOT POSSIBLE HERE. BE CAREFUL THAT THE DESTINATION
         STRING IS LONG ENOUGH TO COPY THE SOURCE STRING
*/
	strcpy (dsc, str);

/*
* If space remains, blank fill the rest of the descriptor
*/
/*	
  	for (i = len; i < dsc->dsc$w_length; i++)
	    dsc->dsc$a_pointer[i] = 0x20; 
*/

	return OK;
}

/*******************************************************************************
*
* StrNCmp compares two strings
*
* Its calling format is:
*
*	status = StrNCmp (s1, s2, len);
* 
* where
*
*  "status" is the status of the operation. != 0 if strings are the same
*
*  "s1"  (CHAR *) string 1
*
*  "s2"  (CHAR *) string 2
*
*  "len" (int)    Length of strings to compare
*
*******************************************************************************/

int StrNCmp (CHAR *s1,
	     CHAR *s2,
		 int len)
{
  LONG i;

	if (strlen(s1) < len) len = strlen(s1);
	if (strlen(s2) < len) len = strlen(s2);
	for(i=0; i<len; i++) {
		if (s1[i] != s2[i]) return 1;
	}
	return 0;
}

/*******************************************************************************
*
* StrUpCase - converts a string to upper case
*
* Its calling format is:
*
*	status = StrUpCase (dst, src, len);
* 
* where
*
*  "status" is the status of the operation.
*
*  "dst"  (CHAR *) destination string
*
*  "src"  (CHAR *) source string
*
*  "len" (int)    Maximum length
*
*******************************************************************************/

int StrUpCase (CHAR *dst,
	     CHAR *src,
		 int len)
{
  LONG i;

	if (strlen(src) < len) len = strlen(src);
	strncpy(dst, src, len);
	for (i=0; i<len; i++) {
		dst[i] = toupper(dst[i]);
	}
	return OK;
}


/*******************************************************************************
*
* StrTrim - Trims a string and returns the length
*
* Its calling format is:
*
*	status = StrTrim (dst, src, len, trim_len);
* 
* where
*
*  "status" is the status of the operation.
*
*  "dst"  (CHAR *) destination string
*
*  "src"  (CHAR *) source string
*
*  "len" (int)    Maximum length
*
*  "trim_len" (int *) Actual length after trimming
*
*******************************************************************************/

int StrTrim (CHAR *dst,
	     CHAR *src,
		 int len,
		 int *trim_len)
{
  LONG i;

	if (strlen(src) < len) len = strlen(src);
	*trim_len = 0;
	for(i=0; i<len; i++) {
		if (src[i] != ' ') dst[(*trim_len)++] = src[i];
	}
	dst[(*trim_len)+1] = 0;
	return OK;
}
