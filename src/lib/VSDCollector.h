/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libvisio project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef VSDCOLLECTOR_H
#define VSDCOLLECTOR_H

#include <vector>
#include "VSDParser.h"

namespace libvisio
{

class VSDCollector
{
public:
  VSDCollector() {}
  virtual ~VSDCollector() {}

  virtual void collectForeignData(unsigned level, const librevenge::RVNGBinaryData &binaryData) = 0;
  virtual void collectTxtXForm(unsigned level, const XForm &txtxform) = 0;
  virtual void collectShapesOrder(unsigned id, unsigned level, const std::vector<unsigned> &shapeIds) = 0;
  virtual void collectForeignDataType(unsigned level, unsigned foreignType, unsigned foreignFormat, double offsetX, double offsetY, double width, double height) = 0;
  virtual void collectPage(unsigned id, unsigned level, unsigned backgroundPageID, bool isBackgroundPage, const VSDName &pageName) = 0;
  virtual void collectShape(unsigned id, unsigned level, unsigned parent, unsigned masterPage, unsigned masterShape, unsigned lineStyle, unsigned fillStyle, unsigned textStyle) = 0;

  virtual void collectText(unsigned level, const librevenge::RVNGBinaryData &textStream, TextFormat format) = 0;
  virtual void collectName(unsigned id, unsigned level,  const librevenge::RVNGBinaryData &name, TextFormat format) = 0;
  virtual void collectPageSheet(unsigned id, unsigned level) = 0;

  // Field list
  virtual void collectFieldList(unsigned id, unsigned level) = 0;
  virtual void collectTextField(unsigned id, unsigned level, int nameId, int formatStringId) = 0;
  virtual void collectNumericField(unsigned id, unsigned level, unsigned short format, double number, int formatStringId) = 0;

  // Temporary hack
  virtual void startPage(unsigned pageId) = 0;
  virtual void endPage() = 0;
  virtual void endPages() = 0;

private:
  VSDCollector(const VSDCollector &);
  VSDCollector &operator=(const VSDCollector &);
};

} // namespace libvisio

#endif /* VSDCOLLECTOR_H */
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
