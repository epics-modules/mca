#include "DppUtilities.h"
#include "stringex.h"

CDppUtilities::CDppUtilities(void)
{
}

CDppUtilities::~CDppUtilities(void)
{
}

// convert a 4 byte long word to a double
// lwStart - starting index of longword
// buffer - byte buffer
double CDppUtilities::LongWordToDouble(int lwStart, unsigned char buffer[])
{
	double dblVal;
	int idx;
	double ByteMask;
	dblVal = 0;
	for(idx=0;idx<4;idx++) {		// build 4 bytes (lwStart-lwStart+3) into double 
		ByteMask = pow(2.0,(8 * idx));
		dblVal = dblVal + (buffer[(lwStart + idx)] * ByteMask);
	}
	return dblVal;
}

// calculate major.minor version from unsigned char, convert/save to double
double CDppUtilities::BYTEVersionToDouble(unsigned char Version)
{
	double dblVersion = 0.0;
	stringex strfn;
	string strTemp;
	strTemp = strfn.Format("%d.%02d",((Version & 240) / 16),(Version & 15));
	dblVersion = atof(strTemp.c_str());
	return dblVersion;
}

// calculate major.minor version from unsigned char, convert/save to string
string CDppUtilities::BYTEVersionToString(unsigned char Version)
{
	string strTemp;
	stringex strfn;
	strTemp = strfn.Format("%d.%02d",((Version & 240) / 16),(Version & 15));
	return strTemp;
}
