/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libvisio project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "VSDParagraphList.h"
#include "VSDCollector.h"
#include "libvisio_utils.h"

namespace libvisio
{

class VSDParagraphListElement
{
public:
  VSDParagraphListElement(unsigned id, unsigned level) : m_id(id), m_level(level) {}
  virtual ~VSDParagraphListElement() {}
  virtual void handle(VSDCollector *collector) const = 0;
  virtual VSDParagraphListElement *clone() = 0;

  unsigned m_id, m_level;
};

} // namespace libvisio


libvisio::VSDParagraphList::VSDParagraphList() :
  m_elements(),
  m_elementsOrder()
{
}

libvisio::VSDParagraphList::VSDParagraphList(const libvisio::VSDParagraphList &paraList) :
  m_elements(),
  m_elementsOrder(paraList.m_elementsOrder)
{
  for (auto iter = paraList.m_elements.begin(); iter != paraList.m_elements.end(); ++iter)
    m_elements[iter->first] = clone(iter->second);
}

libvisio::VSDParagraphList &libvisio::VSDParagraphList::operator=(const libvisio::VSDParagraphList &paraList)
{
  if (this != &paraList)
  {
    clear();
    for (auto iter = paraList.m_elements.begin(); iter != paraList.m_elements.end(); ++iter)
      m_elements[iter->first] = clone(iter->second);
    m_elementsOrder = paraList.m_elementsOrder;
  }
  return *this;
}

libvisio::VSDParagraphList::~VSDParagraphList()
{
}

void libvisio::VSDParagraphList::handle(VSDCollector *collector) const
{
  if (empty())
    return;
  if (!m_elementsOrder.empty())
  {
    for (size_t i = 0; i < m_elementsOrder.size(); i++)
    {
      auto iter = m_elements.find(m_elementsOrder[i]);
      if (iter != m_elements.end() && (0 == i /*|| iter->second->getCharCount()*/))
        iter->second->handle(collector);
    }
  }
  else
  {
    for (auto iter = m_elements.begin(); iter != m_elements.end(); ++iter)
      if (m_elements.begin() == iter /*|| iter->second->getCharCount()*/)
        iter->second->handle(collector);
  }
}

void libvisio::VSDParagraphList::clear()
{
  m_elements.clear();
  m_elementsOrder.clear();
}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
