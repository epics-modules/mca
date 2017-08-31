#ifndef DPPCONFIG_H
#define DPPCONFIG_H

#if defined(DLL_AMPTEK) /* define when library is a DLL */
#  define GLOBAL(type)         __declspec(dllexport) type
#  if defined(DLL_EXPORT) /* define when building the library */
#    define EXTERN               __declspec(dllexport)
#  else
#    define EXTERN               __declspec(dllimport)
#  endif
#else
#define GLOBAL
#define EXTERN
#endif /* defined(DLL_AMPTEK) */

#endif /* DPPCONFIG_H */