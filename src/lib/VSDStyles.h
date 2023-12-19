/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libvisio project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __VSDSTYLES_H__
#define __VSDSTYLES_H__

#include <map>
#include <vector>
#include "VSDTypes.h"
// #include "libvisio_utils.h"

namespace libvisio
{

struct VSDCharStyle
{
  VSDCharStyle()
    : charCount(0), font(), colour(), size(12.0/72.0), bold(false),
      italic(false), underline(false), doubleunderline(false),
      strikeout(false), doublestrikeout(false), allcaps(false),
      initcaps(false), smallcaps(false), superscript(false),
      subscript(false), scaleWidth(1.0) {}
  VSDCharStyle(unsigned cc, const VSDName &ft, const Colour &c, double s,
               bool b, bool i, bool u, bool du, bool so, bool dso, bool ac,
               bool ic, bool sc, bool super, bool sub, double sw) :
    charCount(cc), font(ft), colour(c), size(s), bold(b), italic(i),
    underline(u), doubleunderline(du), strikeout(so), doublestrikeout(dso),
    allcaps(ac), initcaps(ic), smallcaps(sc), superscript(super),
    subscript(sub), scaleWidth(sw) {}
  VSDCharStyle(const VSDCharStyle &style) :
    charCount(style.charCount), font(style.font), colour(style.colour),
    size(style.size), bold(style.bold), italic(style.italic),
    underline(style.underline), doubleunderline(style.doubleunderline),
    strikeout(style.strikeout), doublestrikeout(style.doublestrikeout),
    allcaps(style.allcaps), initcaps(style.initcaps),
    smallcaps(style.smallcaps), superscript(style.superscript),
    subscript(style.subscript), scaleWidth(style.scaleWidth) {}
  ~VSDCharStyle() {}
  VSDCharStyle &operator=(const VSDCharStyle&) = default;

  unsigned charCount;
  VSDName font;
  Colour colour;
  double size;
  bool bold;
  bool italic;
  bool underline;
  bool doubleunderline;
  bool strikeout;
  bool doublestrikeout;
  bool allcaps;
  bool initcaps;
  bool smallcaps;
  bool superscript;
  bool subscript;
  double scaleWidth;
};

} // namespace libvisio

#endif // __VSDSTYLES_H__
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
