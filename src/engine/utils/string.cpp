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

#include "../include/utils/string.hpp"
#include "../include/utils/debug.hpp"
#include <stdio.h>
#define CHAR_BIT 8

char *String::createU32ToString(const u32 a)
{
    char *result = new char[(((sizeof a) * CHAR_BIT) + 2) / 3 + 2];
    sprintf(result, "%d", a);
    return result;
}

/** Converts "123" to "000123". 6 digits length */
char *String::createWithLeadingZeros(const char *a)
{
    const u8 digitsAmount = 6;
    char *part = createConcatenated("00000000", a);
    u16 partLength = getLength(part);
    u16 insert = 0;
    char *result = new char[digitsAmount + 1];
    for (u16 i = partLength - digitsAmount; i < partLength; i++)
        result[insert++] = part[i];
    delete[] part;
    result[digitsAmount] = '\0';
    return result;
}

char *String::createWithoutExtension(const char *source)
{
    u32 length = 0;
    for (;; length++)
        if (source[length] == '.')
            break;
    char *res = new char[length + 1];
    for (u32 i = 0; i < length; i++)
        res[i] = source[i];
    res[length] = '\0';
    return res;
}

char *String::createCopy(const char *source)
{
    u32 srcLength = getLength(source);
    char *res = new char[srcLength + 1];
    for (u32 i = 0; i < srcLength; i++)
        res[i] = source[i];
    res[srcLength] = '\0';
    return res;
}

// For test\0 will return 4
u32 String::getLength(const char *a)
{
    if (a == NULL)
        return 0;
    for (u32 i = 0;; i++)
        if (a[i] == '\0')
            return i;
}

char *String::createConcatenated(const char *a, const char *b)
{
    u32 aLength = getLength(a);
    u32 bLength = getLength(b);
    
    char *res = new char[aLength + bLength + 1]; // + '\0'
    for (u32 i = 0; i < aLength; i++)
        res[i] = a[i];
    for (u32 i = 0; i < bLength; i++)
        res[aLength + i] = b[i];
    res[aLength + bLength] = '\0';
    return res;
}


char *String::createConcatenated(const char *a, const char *b, const char *c)
{
    u32 aLength = getLength(a);
    u32 bLength = getLength(b);
    u32 cLength = getLength(c);

    char *res = new char[aLength + bLength + cLength + 1]; // + '\0'
    for (u32 i = 0; i < aLength; i++)
        res[i] = a[i];
    for (u32 i = 0; i < bLength; i++)
        res[aLength + i] = b[i];
    for (u32 i = 0; i < cLength; i++)
        res[bLength + i] = c[i];
}

char *String::createConcatenated(const char *a, const char *b, const char *c, const char *d)
{
    u32 aLength = getLength(a);
    u32 bLength = getLength(b);
    u32 cLength = getLength(c);
    u32 dLength = getLength(d);
    
    char *res = new char[aLength + bLength + cLength + dLength + 1]; // + '\0'
    for (u32 i = 0; i < aLength; i++)
        res[i] = a[i];
    for (u32 i = 0; i < bLength; i++)
        res[aLength + i] = b[i];
    for (u32 i = 0; i < cLength; i++)
        res[bLength + i] = c[i];
    for (u32 i = 0; i < dLength; i++)
        res[cLength + i] = d[i];
    res[aLength + bLength + cLength + dLength] = '\0';
    return res;
}