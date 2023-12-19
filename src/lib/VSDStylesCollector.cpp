/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libvisio project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "VSDStylesCollector.h"

#include <vector>
#include <map>

libvisio::VSDStylesCollector::VSDStylesCollector(
  std::vector<std::map<unsigned, XForm> > &groupXFormsSequence,
  std::vector<std::map<unsigned, unsigned> > &groupMembershipsSequence,
  std::vector<std::list<unsigned> > &documentPageShapeOrders
) :
  m_currentLevel(0), m_isShapeStarted(false),
  m_shadowOffsetX(0.0), m_shadowOffsetY(0.0),
  m_currentShapeId(0), m_groupXForms(), m_groupMemberships(),
  m_groupXFormsSequence(groupXFormsSequence),
  m_groupMembershipsSequence(groupMembershipsSequence),
  m_pageShapeOrder(), m_documentPageShapeOrders(documentPageShapeOrders),
  m_groupShapeOrder(), m_shapeList(), m_currentStyleSheet(0),
  m_currentShapeLevel(0)
{
  m_groupXFormsSequence.clear();
  m_groupMembershipsSequence.clear();
  m_documentPageShapeOrders.clear();
}

void libvisio::VSDStylesCollector::collectForeignData(unsigned level, const librevenge::RVNGBinaryData & /* binaryData */)
{
  _handleLevelChange(level);
}

void libvisio::VSDStylesCollector::collectTxtXForm(unsigned level, const XForm & /* txtxform */)
{
  _handleLevelChange(level);
}

void libvisio::VSDStylesCollector::collectShapesOrder(unsigned /* id */, unsigned level, const std::vector<unsigned> &shapeIds)
{
  _handleLevelChange(level);
  m_shapeList.clear();
  for (unsigned int shapeId : shapeIds)
    m_shapeList.push_back(shapeId);
  _flushShapeList();
}

void libvisio::VSDStylesCollector::collectForeignDataType(unsigned level, unsigned /* foreignType */, unsigned /* foreignFormat */,
                                                          double /* offsetX */, double /* offsetY */, double /* width */, double /* height */)
{
  _handleLevelChange(level);
}

void libvisio::VSDStylesCollector::collectPage(unsigned /* id */, unsigned level, unsigned /* backgroundPageID */, bool /* isBackgroundPage */, const VSDName & /* pageName */)
{
  _handleLevelChange(level);
}

void libvisio::VSDStylesCollector::collectShape(unsigned id, unsigned level, unsigned parent, unsigned /*masterPage*/, unsigned /*masterShape*/,
                                                unsigned /* lineStyle */, unsigned /* fillStyle */, unsigned /* textStyle */)
{
  _handleLevelChange(level);
  m_currentShapeLevel = level;
  m_currentShapeId = id;
  m_isShapeStarted = true;
  if (parent && parent != MINUS_ONE)
    m_groupMemberships[m_currentShapeId] = parent;
}

void libvisio::VSDStylesCollector::collectText(unsigned level, const librevenge::RVNGBinaryData & /*textStream*/, TextFormat /*format*/)
{
  _handleLevelChange(level);
}

void libvisio::VSDStylesCollector::collectName(unsigned /*id*/, unsigned level, const librevenge::RVNGBinaryData & /*name*/, TextFormat /*format*/)
{
  _handleLevelChange(level);
}

void libvisio::VSDStylesCollector::collectPageSheet(unsigned /* id */, unsigned level)
{
  _handleLevelChange(level);
  m_currentShapeLevel = level;
}

void libvisio::VSDStylesCollector::collectFieldList(unsigned /* id */, unsigned level)
{
  _handleLevelChange(level);
}

void libvisio::VSDStylesCollector::collectTextField(unsigned /* id */, unsigned level, int /* nameId */, int /* formatStringId */)
{
  _handleLevelChange(level);
}

void libvisio::VSDStylesCollector::collectNumericField(unsigned /* id */, unsigned level, unsigned short /* format */, double /* number */, int /* formatStringId */)
{
  _handleLevelChange(level);
}

void libvisio::VSDStylesCollector::startPage(unsigned /* pageId */)
{
  m_groupXForms.clear();
  m_groupMemberships.clear();
  m_pageShapeOrder.clear();
  m_groupShapeOrder.clear();
}

void libvisio::VSDStylesCollector::endPage()
{
  _handleLevelChange(0);
  m_groupXFormsSequence.push_back(m_groupXForms);
  m_groupMembershipsSequence.push_back(m_groupMemberships);

  bool changed = true;
  while (!m_groupShapeOrder.empty() && changed)
  {
    changed = false;
    for (auto j = m_pageShapeOrder.begin(); j != m_pageShapeOrder.end();)
    {
      auto iter = m_groupShapeOrder.find(*j++);
      if (m_groupShapeOrder.end() != iter)
      {
        m_pageShapeOrder.splice(j, iter->second, iter->second.begin(), iter->second.end());
        m_groupShapeOrder.erase(iter);
        changed = true;
      }
    }
  }
  m_documentPageShapeOrders.push_back(m_pageShapeOrder);
}

void libvisio::VSDStylesCollector::_handleLevelChange(unsigned level)
{
  if (m_currentLevel == level)
    return;
  if (level <= m_currentShapeLevel)
    m_isShapeStarted = false;

  m_currentLevel = level;
}

void libvisio::VSDStylesCollector::_flushShapeList()
{
  if (m_shapeList.empty())
    return;

  if (m_isShapeStarted)
    m_groupShapeOrder[m_currentShapeId] = m_shapeList;
  else
    m_pageShapeOrder = m_shapeList;

  m_shapeList.clear();
}
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
