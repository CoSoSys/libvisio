/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libvisio project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __LIBVISIO_UTILS_H__
#define __LIBVISIO_UTILS_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <memory>

//#include <boost/cstdint.hpp>
//#include <cstdint>

#include "VSDTypes.h"

#define VSD_EPSILON 1E-6
#define VSD_ALMOST_ZERO(m) (fabs(m) <= VSD_EPSILON)
#define VSD_APPROX_EQUAL(x, y) VSD_ALMOST_ZERO((x) - (y))

#include "librevenge.h"
#include "RVNGDirectoryStream.h"
#include "RVNGStream.h"
#include "RVNGStreamImplementation.h"

#if defined(HAVE_FUNC_ATTRIBUTE_FORMAT)
#define VSD_ATTRIBUTE_PRINTF(fmt, arg) __attribute__((format(printf, fmt, arg)))
#else
#define VSD_ATTRIBUTE_PRINTF(fmt, arg)
#endif

#define VSD_NUM_ELEMENTS(array) (sizeof(array)/sizeof((array)[0]))


typedef int32_t UChar32;

// The maximum number of UTF-8 code units (bytes) per Unicode code point (U+0000..U+10ffff).
#define U8_MAX_LENGTH 4

/**
* Append a code point to a string, overwriting 1 to 4 bytes.
* The offset points to the current end of the string contents
* and is advanced (post-increment).
* "Unsafe" macro, assumes a valid code point and sufficient space in the string.
* Otherwise, the result is undefined.
*
* @param s const uint8_t * string buffer
* @param i string offset
* @param c code point to append
*/
#define U8_APPEND_UNSAFE(s, i, c) { \
    uint32_t __uc=(c); \
    if(__uc<=0x7f) { \
        (s)[(i)++]=(uint8_t)__uc; \
    } else { \
        if(__uc<=0x7ff) { \
            (s)[(i)++]=(uint8_t)((__uc>>6)|0xc0); \
        } else { \
            if(__uc<=0xffff) { \
                (s)[(i)++]=(uint8_t)((__uc>>12)|0xe0); \
            } else { \
                (s)[(i)++]=(uint8_t)((__uc>>18)|0xf0); \
                (s)[(i)++]=(uint8_t)(((__uc>>12)&0x3f)|0x80); \
            } \
            (s)[(i)++]=(uint8_t)(((__uc>>6)&0x3f)|0x80); \
        } \
        (s)[(i)++]=(uint8_t)((__uc&0x3f)|0x80); \
    } \
}

namespace libvisio
{

typedef std::shared_ptr<librevenge::RVNGInputStream> RVNGInputStreamPtr_t;

struct VSDDummyDeleter
{
  void operator()(void *) {}
};

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&... args)
{
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<typename T>
std::unique_ptr<T> clone(const std::unique_ptr<T> &other)
{
  return std::unique_ptr<T>(other->clone());
}

uint8_t readU8(librevenge::RVNGInputStream *input);
uint16_t readU16(librevenge::RVNGInputStream *input);
uint32_t readU32(librevenge::RVNGInputStream *input);
int32_t readS32(librevenge::RVNGInputStream *input);
uint64_t readU64(librevenge::RVNGInputStream *input);

unsigned long getRemainingLength(librevenge::RVNGInputStream *input);

void appendUCS4(librevenge::RVNGString &text, UChar32 ucs4Character);

class EndOfStreamException
{
};

class GenericException
{
};

} // namespace libvisio

#endif // __LIBVISIO_UTILS_H__
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
