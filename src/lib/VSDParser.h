/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libvisio project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __VSDPARSER_H__
#define __VSDPARSER_H__

#include <cstdio>
#include <iostream>
#include <vector>
#include <stack>
#include <map>
#include <set>
#include "librevenge.h"
#include "VSDTypes.h"
#include "VSDFieldList.h"
#include "VSDCharacterList.h"
#include "VSDParagraphList.h"
#include "VSDShapeList.h"
#include "VSDStencils.h"

namespace libvisio
{

class VSDCollector;

struct Pointer
{
  Pointer()
    : Type(0), Offset(0), Length(0), Format(0), ListSize(0) {}
  Pointer(const Pointer &ptr)
    : Type(ptr.Type), Offset(ptr.Offset), Length(ptr.Length), Format(ptr.Format), ListSize(ptr.ListSize) {}
  Pointer &operator=(const Pointer&) = default;
  unsigned Type;
  unsigned Offset;
  unsigned Length;
  unsigned short Format;
  unsigned ListSize;
};

class VSDParser
{
public:
  explicit VSDParser(librevenge::RVNGInputStream *input, librevenge::RVNGDrawingInterface *painter, librevenge::RVNGInputStream *container = nullptr);
  virtual ~VSDParser();
  bool parseMain();
  bool extractStencils();

protected:

	void readNameIDX(librevenge::RVNGInputStream *input);
  void readPageProps(librevenge::RVNGInputStream *input);

  // reader functions
  void readShapeId(librevenge::RVNGInputStream *input);
  virtual void readShape(librevenge::RVNGInputStream *input);
  virtual void readPage(librevenge::RVNGInputStream *input);
  virtual void readText(librevenge::RVNGInputStream *input);
  void readNameList(librevenge::RVNGInputStream *input);
  virtual void readNameList2(librevenge::RVNGInputStream *input);
  void readPageSheet(librevenge::RVNGInputStream *input);

  // parser of one pass
  bool parseDocument(librevenge::RVNGInputStream *input, unsigned shift);

  // Stream handlers
  void handleStreams(librevenge::RVNGInputStream *input, unsigned ptrType, unsigned shift, unsigned level, std::set<unsigned> &visited);
  void handleStream(const Pointer &ptr, unsigned idx, unsigned level, std::set<unsigned> &visited);
  void handleChunks(librevenge::RVNGInputStream *input, unsigned level);
  void handleChunk(librevenge::RVNGInputStream *input);
  void handleBlob(librevenge::RVNGInputStream *input, unsigned shift, unsigned level);

  virtual void readPointer(librevenge::RVNGInputStream *input, Pointer &ptr);
  virtual void readPointerInfo(librevenge::RVNGInputStream *input, unsigned ptrType, unsigned shift, unsigned &listSize, int &pointerCount);
  virtual bool getChunkHeader(librevenge::RVNGInputStream *input);
  void _handleLevelChange(unsigned level);
  void _flushShape();
  void _nameFromId(VSDName &name, unsigned id, unsigned level);

  virtual unsigned getUInt(librevenge::RVNGInputStream *input);
  virtual int getInt(librevenge::RVNGInputStream *input);

  librevenge::RVNGInputStream *m_input;
  librevenge::RVNGDrawingInterface *m_painter;
  librevenge::RVNGInputStream *m_container;
  ChunkHeader m_header;
  VSDCollector *m_collector;
  VSDShapeList m_shapeList;
  unsigned m_currentLevel;

  VSDStencils m_stencils;
  VSDStencil *m_currentStencil;
  VSDShape m_shape;
  bool m_isStencilStarted;
  bool m_isInStyles;
  unsigned m_currentShapeLevel;
  unsigned m_currentShapeID;

  unsigned m_currentLayerListLevel;

  bool m_extractStencils;
  std::vector<Colour> m_colours;

  bool m_isBackgroundPage;
  bool m_isShapeStarted;

  double m_shadowOffsetX;
  double m_shadowOffsetY;

  unsigned m_currentGeomListCount;

  std::map<unsigned, VSDName> m_fonts;
  std::map<unsigned, VSDName> m_names;
  std::map<unsigned, std::map<unsigned, VSDName> > m_namesMapMap;
  VSDName m_currentPageName;

  std::map<unsigned, VSDTabStop> *m_currentTabSet;

private:
  VSDParser();
  VSDParser(const VSDParser &);
  VSDParser &operator=(const VSDParser &);

};

} // namespace libvisio

#endif // __VSDPARSER_H__
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
