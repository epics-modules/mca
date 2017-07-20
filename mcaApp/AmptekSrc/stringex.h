#pragma once
#include <stdio.h>
#include <stdarg.h>
#include <cstdarg>
#include <vector>
#include <string>
#include <cctype> // std::toupper, std::tolower
using namespace std;

class stringex
{
public:
	stringex(void);
	~stringex(void);
	std::string Format (const char *fmt, ...);
	std::string MakeUpper(string StdString);
	std::string MakeLower(string StdString);

private:
	std::string vformat (const char *fmt, va_list ap);

};


