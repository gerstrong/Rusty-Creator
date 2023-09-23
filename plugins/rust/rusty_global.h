#ifndef RUSTY_GLOBAL_H
#define RUSTY_GLOBAL_H

#include <qglobal.h>

#if defined(RUSTY_LIBRARY)
#  define RUSTY_EXPORT Q_DECL_EXPORT
#else
#  define RUSTY_EXPORT Q_DECL_IMPORT
#endif

#endif // RUSTY_GLOBAL_H
