#ifndef __STDDEF_H
#define __STDDEF_H

#include <vcruntime.h>

#define offsetof(type, member) ((size_t)&(((type *)0)->member))

#endif
