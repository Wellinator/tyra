/*
# ______       ____   ___
#   |     \/   ____| |___|    
#   |     |   |   \  |   |       
#-----------------------------------------------------------------------
# Copyright 2020 - 2022, tyra - https://github.com/h4570/tyra
# Licenced under Apache License 2.0
# Sandro Sobczyński <sandro.sobczynski@gmail.com>
# Wellington Carvalho <wellcoj@gmail.com> and André Guilherme <andregui17@outlook.com>
*/

#ifndef _TYRA_STRING_
#define _TYRA_STRING_

#include <tamtypes.h>

class String
{

public:
    static char *createU32ToString(const u32 a);
    static char *createWithLeadingZeros(const char *a);
    static char *createCopy(const char *source);
    static u32 getLength(const char *a);
    //Wolf3s: New concatenateds.
    static char *createConcatenated(const char *a, const char *b);
    static char *createConcatenated(const char *a, const char *b, const char *c);
    static char *createConcatenated(const char *a, const char *b, const char *c, const char *d);
    static char *createWithoutExtension(const char *source);
};

#endif
