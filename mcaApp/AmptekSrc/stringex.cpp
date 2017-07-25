#include "stringex.h"

#ifdef _MSC_VER
	#pragma warning(disable:4996)
	#define vsnprintf _vsnprintf
#endif

stringex::stringex(void)
{
}

stringex::~stringex(void)
{
}

std::string stringex::vformat (const char *fmt, va_list ap)
{
    size_t size = 1024;
    char stackbuf[1024];
    std::vector<char> dynamicbuf;
    char *buf = &stackbuf[0];
    while (1) {
        int needed = vsnprintf (buf, size, fmt, ap);
        if (needed <= (int)size && needed >= 0) {
            return std::string (buf, (size_t) needed);
        }
        size = (needed > 0) ? (needed+1) : (size*2);
        dynamicbuf.resize (size);
        buf = &dynamicbuf[0];
    }
}

std::string stringex::Format (const char *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    std::string buf = vformat (fmt, ap);
    va_end (ap);
    return buf;
}

std::string stringex::MakeUpper(string StdString)
{
  const int length = (int)StdString.length();
  for(int i=0; i<length ; ++i)
  {
	  StdString[i] = std::toupper(StdString[i]);
  }
  return StdString;
}

std::string stringex::MakeLower(string StdString)
{
  const int length = (int)StdString.length();
  for(int i=0; i<length ; ++i)
  {
	  StdString[i] = std::tolower(StdString[i]);
  }
  return StdString;
}
