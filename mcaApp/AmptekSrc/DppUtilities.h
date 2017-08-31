#pragma once
#include <stdlib.h>
#include <math.h>
#include <string>
#include "DppConfig.h"
using namespace std; 

class EXTERN CDppUtilities
{
public:
	CDppUtilities(void);
	~CDppUtilities(void);
	/// Covert a long word into a double.
	double LongWordToDouble(int lwStart, unsigned char buffer[]);
	/// Convert a byte version value into a double.
	double BYTEVersionToDouble(unsigned char Version);
	/// Convert a byte version value into a string.
	string BYTEVersionToString(unsigned char Version);
};
