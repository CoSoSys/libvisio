/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libvisio project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef VSDSTYLESCOLLECTOR_H
#define VSDSTYLESCOLLECTOR_H

#include <map>
#include <vector>
#include <list>
#include "VSDCollector.h"
#include "VSDParser.h"
#include "libvisio_utils.h"
#include "VSDStyles.h"

namespace libvisio
{

class VSDStylesCollector : public VSDCollector
{
public:
  VSDStylesCollector(
    std::vector<std::map<unsigned, XForm> > &groupXFormsSequence,
    std::vector<std::map<unsigned, unsigned> > &groupMembershipsSequence,
    std::vector<std::list<unsigned> > &documentPageShapeOrders
  );
  ~VSDStylesCollector() override {}

  void collectForeignData(unsigned level, const librevenge::RVNGBinaryData &binaryData) override;
  void collectTxtXForm(unsigned level, const XForm &txtxform) override;
  void collectShapesOrder(unsigned id, unsigned level, const std::vector<unsigned> &shapeIds) override;
  void collectForeignDataType(unsigned level, unsigned foreignType, unsigned foreignFormat, double offsetX, double offsetY, double width, double height) override;
  void collectPage(unsigned id, unsigned level, unsigned backgroundPageID, bool isBackgroundPage, const VSDName &pageName) override;
  void collectShape(unsigned id, unsigned level, unsigned parent, unsigned masterPage, unsigned masterShape, unsigned lineStyle, unsigned fillStyle, unsigned textStyle) override;

  void collectText(unsigned level, const librevenge::RVNGBinaryData &textStream, TextFormat format) override;

  void collectName(unsigned id, unsigned level, const librevenge::RVNGBinaryData &name, TextFormat format) override;
  void collectPageSheet(unsigned id, unsigned level) override;


  // Field list
  void collectFieldList(unsigned id, unsigned level) override;
  void collectTextField(unsigned id, unsigned level, int nameId, int formatStringId) override;
  void collectNumericField(unsigned id, unsigned level, unsigned short format, double number, int formatStringId) override;

  // Temporary hack
  void startPage(unsigned pageID) override;
  void endPage() override;
  void endPages() override {}

private:
  VSDStylesCollector(const VSDStylesCollector &);
  VSDStylesCollector &operator=(const VSDStylesCollector &);

  void _handleLevelChange(unsigned level);
  void _flushShapeList();

  unsigned m_currentLevel;
  bool m_isShapeStarted;

  double m_shadowOffsetX;
  double m_shadowOffsetY;

  unsigned m_currentShapeId;
  std::map<unsigned, XForm> m_groupXForms;
  std::map<unsigned, unsigned> m_groupMemberships;
  std::vector<std::map<unsigned, XForm> > &m_groupXFormsSequence;
  std::vector<std::map<unsigned, unsigned> > &m_groupMembershipsSequence;
  std::list<unsigned> m_pageShapeOrder;
  std::vector<std::list<unsigned> > &m_documentPageShapeOrders;
  std::map<unsigned, std::list<unsigned> > m_groupShapeOrder;
  std::list<unsigned> m_shapeList;

  unsigned m_currentStyleSheet;

  unsigned m_currentShapeLevel;
};

}

#endif /* VSDSTYLESCOLLECTOR_H */
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
