/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libvisio project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __VSDCHARACTERLIST_H__
#define __VSDCHARACTERLIST_H__

#include <memory>
#include <vector>
#include <map>
#include "VSDTypes.h"
#include "VSDStyles.h"

namespace libvisio
{

class VSDCharacterListElement;
class VSDCollector;

class VSDCharacterList
{
public:
  VSDCharacterList();
  VSDCharacterList(const VSDCharacterList &charList);
  ~VSDCharacterList();
  VSDCharacterList &operator=(const VSDCharacterList &charList);
  void handle(VSDCollector *collector) const;
  void clear();
  bool empty() const
  {
    return (m_elements.empty());
  }
private:
  std::map<unsigned, std::unique_ptr<VSDCharacterListElement>> m_elements;
  std::vector<unsigned> m_elementsOrder;
};

} // namespace libvisio

#endif // __VSDCHARACTERLIST_H__
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
