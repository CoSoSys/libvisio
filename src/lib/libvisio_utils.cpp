/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libvisio project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "libvisio_utils.h"

#include <cstdarg>
#include <cstdio>
#include "VSDInternalStream.h"

uint8_t libvisio::readU8(librevenge::RVNGInputStream *input)
{
  if (!input || input->isEnd())
  {
    throw EndOfStreamException();
  }
  unsigned long numBytesRead;
  uint8_t const *p = input->read(sizeof(uint8_t), numBytesRead);

  if (p && numBytesRead == sizeof(uint8_t))
    return *(uint8_t const *)(p);
  throw EndOfStreamException();
}

uint16_t libvisio::readU16(librevenge::RVNGInputStream *input)
{
  if (!input || input->isEnd())
  {
    throw EndOfStreamException();
  }
  unsigned long numBytesRead;
  uint8_t const *p = input->read(sizeof(uint16_t), numBytesRead);

  if (p && numBytesRead == sizeof(uint16_t))
    return (uint16_t)(p[0]|((uint16_t)p[1]<<8));
  throw EndOfStreamException();
}

uint32_t libvisio::readU32(librevenge::RVNGInputStream *input)
{
  if (!input || input->isEnd())
  {
    throw EndOfStreamException();
  }
  unsigned long numBytesRead;
  uint8_t const *p = input->read(sizeof(uint32_t), numBytesRead);

  if (p && numBytesRead == sizeof(uint32_t))
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
  throw EndOfStreamException();
}

int32_t libvisio::readS32(librevenge::RVNGInputStream *input)
{
  return (int32_t)readU32(input);
}

uint64_t libvisio::readU64(librevenge::RVNGInputStream *input)
{
  if (!input || input->isEnd())
  {
    throw EndOfStreamException();
  }
  unsigned long numBytesRead;
  uint8_t const *p = input->read(sizeof(uint64_t), numBytesRead);

  if (p && numBytesRead == sizeof(uint64_t))
    return (uint64_t)p[0]|((uint64_t)p[1]<<8)|((uint64_t)p[2]<<16)|((uint64_t)p[3]<<24)|((uint64_t)p[4]<<32)|((uint64_t)p[5]<<40)|((uint64_t)p[6]<<48)|((uint64_t)p[7]<<56);
  throw EndOfStreamException();
}

unsigned long libvisio::getRemainingLength(librevenge::RVNGInputStream *const input)
{
  if (!input)
    throw EndOfStreamException();

  const long begin = input->tell();

  if (input->seek(0, librevenge::RVNG_SEEK_END) != 0)
  {
    // librevenge::RVNG_SEEK_END does not work. Use the harder way.
    while (!input->isEnd())
      readU8(input);
  }
  const long end = input->tell();

  input->seek(begin, librevenge::RVNG_SEEK_SET);

  if (end < begin)
    throw EndOfStreamException();
  return static_cast<unsigned long>(end - begin);
}

void libvisio::appendUCS4(librevenge::RVNGString &text, UChar32 ucs4Character)
{
  // Convert carriage returns to new line characters
  if (ucs4Character == (UChar32) 0x0d || ucs4Character == (UChar32) 0x0e)
    ucs4Character = (UChar32) '\n';

  unsigned char outbuf[U8_MAX_LENGTH+1];
  int i = 0;
  U8_APPEND_UNSAFE(&outbuf[0], i, ucs4Character);
  outbuf[i] = 0;

  text.append((char *)outbuf);
}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
