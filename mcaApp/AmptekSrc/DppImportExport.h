#ifndef DPPIMPORTEXPORT_H
#define DPPIMPORTEXPORT_H

#if defined(DLL_AMPTEK) /* define when library is a DLL */
#  if defined(DLL_EXPORT) /* define when building the library */
#    define IMPORT_EXPORT __declspec(dllexport)
#  else
#    define IMPORT_EXPORT __declspec(dllimport)
#  endif
#else
#define IMPORT_EXPORT
#endif /* defined(DLL_AMPTEK) */

#endif /* DPPIMPORTEXPORT_H */