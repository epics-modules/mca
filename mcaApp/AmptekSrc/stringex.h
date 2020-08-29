#pragma once
#ifndef STRINGEX_H
#define STRINGEX_H
#ifdef _MSC_VER
	#pragma warning(disable:4996)  // uses _vsnprintf, disable warning, can use _CRT_SECURE_NO_WARNINGS
//	#define _CRT_SECURE_NO_WARNINGS
#endif
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

#endif
