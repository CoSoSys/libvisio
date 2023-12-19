/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libvisio project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "VSDParser.h"

#include "RVNGDirectoryStream.h"
#include "RVNGStream.h"
#include "RVNGStreamImplementation.h"
#include "libvisio_utils.h"
#include "VSDInternalStream.h"
#include "VSDDocumentStructure.h"
#include "VSDContentCollector.h"
#include "VSDStylesCollector.h"

#include <locale.h>
#include <cassert>
#include <sstream>
#include <string>
#include <cmath>
#include <set>
#include <algorithm>

libvisio::VSDParser::VSDParser(librevenge::RVNGInputStream *input, librevenge::RVNGDrawingInterface *painter, librevenge::RVNGInputStream *container)
  : m_input(input), m_painter(painter), m_container(container), m_header(), m_collector(nullptr), m_shapeList(), m_currentLevel(0),
    m_stencils(), m_currentStencil(nullptr), m_shape(), m_isStencilStarted(false), m_isInStyles(false),
    m_currentShapeLevel(0), m_currentShapeID(MINUS_ONE), m_currentLayerListLevel(0), m_extractStencils(false), m_colours(),
    m_isBackgroundPage(false), m_isShapeStarted(false), m_shadowOffsetX(0.0), m_shadowOffsetY(0.0),
    m_currentGeomListCount(0), m_fonts(), m_names(), m_namesMapMap(),
    m_currentPageName(), m_currentTabSet()
{}

libvisio::VSDParser::~VSDParser()
{
}

void libvisio::VSDParser::_nameFromId(VSDName &name, unsigned id, unsigned level)
{
  name = VSDName();
  std::map<unsigned, std::map<unsigned, VSDName> >::const_iterator iter1 = m_namesMapMap.find(level);
  if (iter1 != m_namesMapMap.end())
  {
    auto iter = iter1->second.find(id);
    if (iter != iter1->second.end())
      name = iter->second;
  }
}

bool libvisio::VSDParser::getChunkHeader(librevenge::RVNGInputStream *input)
{
  unsigned char tmpChar = 0;
  while (!input->isEnd() && !tmpChar)
    tmpChar = readU8(input);

  if (input->isEnd())
    return false;
  else
    input->seek(-1, librevenge::RVNG_SEEK_CUR);

  m_header.chunkType = readU32(input);
  m_header.id = readU32(input);
  m_header.list = readU32(input);

  // Certain chunk types seem to always have a trailer
  m_header.trailer = 0;
  if (m_header.list != 0 || m_header.chunkType == 0x71 || m_header.chunkType == 0x70 ||
      m_header.chunkType == 0x6b || m_header.chunkType == 0x6a || m_header.chunkType == 0x69 ||
      m_header.chunkType == 0x66 || m_header.chunkType == 0x65 || m_header.chunkType == 0x2c)
    m_header.trailer += 8; // 8 byte trailer

  m_header.dataLength = readU32(input);
  m_header.level = readU16(input);
  m_header.unknown = readU8(input);

  unsigned trailerChunks [14] = {0x64, 0x65, 0x66, 0x69, 0x6a, 0x6b, 0x6f, 0x71,
                                 0x92, 0xa9, 0xb4, 0xb6, 0xb9, 0xc7
                                };
  // Add word separator under certain circumstances for v11
  // Below are known conditions, may be more or a simpler pattern
  if (m_header.list != 0 || (m_header.level == 2 && m_header.unknown == 0x55) ||
      (m_header.level == 2 && m_header.unknown == 0x54 && m_header.chunkType == 0xaa)
      || (m_header.level == 3 && m_header.unknown != 0x50 && m_header.unknown != 0x54))
  {
    m_header.trailer += 4;
  }

  for (unsigned int trailerChunk : trailerChunks)
  {
    if (m_header.chunkType == trailerChunk && m_header.trailer != 12 && m_header.trailer != 4)
    {
      m_header.trailer += 4;
      break;
    }
  }

  // Some chunks never have a trailer
  if (m_header.chunkType == 0x1f || m_header.chunkType == 0xc9 ||
      m_header.chunkType == 0x2d || m_header.chunkType == 0xd1)
  {
    m_header.trailer = 0;
  }
  return true;
}

bool libvisio::VSDParser::parseMain()
{
  if (!m_input)
  {
    return false;
  }

  // we currently we support only Visio 11 (upgrade the below statement with further versions when it is available )
  m_input->seek(0x1A, librevenge::RVNG_SEEK_SET);
  const unsigned char version = libvisio::readU8(m_input);
  if (version != 11) {
	  return false;
  }

  // Seek to trailer stream pointer
  m_input->seek(0x24, librevenge::RVNG_SEEK_SET);

  Pointer trailerPointer;
  readPointer(m_input, trailerPointer);
  bool compressed = ((trailerPointer.Format & 2) == 2);
  unsigned shift = 0;
  if (compressed)
    shift = 4;

  m_input->seek(trailerPointer.Offset, librevenge::RVNG_SEEK_SET);
  VSDInternalStream trailerStream(m_input, trailerPointer.Length, compressed);

  std::vector<std::map<unsigned, XForm> > groupXFormsSequence;
  std::vector<std::map<unsigned, unsigned> > groupMembershipsSequence;
  std::vector<std::list<unsigned> > documentPageShapeOrders;

  VSDStylesCollector stylesCollector(groupXFormsSequence, groupMembershipsSequence, documentPageShapeOrders);
  m_collector = &stylesCollector;
  if (!parseDocument(&trailerStream, shift))
    return false;

  _handleLevelChange(0);

  VSDContentCollector contentCollector(m_painter, groupXFormsSequence, groupMembershipsSequence, documentPageShapeOrders, m_stencils);
  m_collector = &contentCollector;

  if (!parseDocument(&trailerStream, shift))
  {
    m_collector = nullptr;
    return false;
  }
  m_collector = nullptr;
  return true;
}

bool libvisio::VSDParser::parseDocument(librevenge::RVNGInputStream *input, unsigned shift)
{
  std::set<unsigned> visited;
  try
  {
    handleStreams(input, VSD_TRAILER_STREAM, shift, 0, visited);
    assert(visited.empty());
    return true;
  }
  catch (...)
  {
    assert(visited.empty());
    return false;
  }
}

void libvisio::VSDParser::readPointer(librevenge::RVNGInputStream *input, Pointer &ptr)
{
  ptr.Type = readU32(input);
  input->seek(4, librevenge::RVNG_SEEK_CUR); // Skip dword
  ptr.Offset = readU32(input);
  ptr.Length = readU32(input);
  ptr.Format = readU16(input);
}

void libvisio::VSDParser::readPointerInfo(librevenge::RVNGInputStream *input, unsigned /* ptrType */, unsigned shift, unsigned &listSize, int &pointerCount)
{
  input->seek(shift, librevenge::RVNG_SEEK_SET);
  unsigned offset = readU32(input);
  input->seek(offset+shift-4, librevenge::RVNG_SEEK_SET);
  listSize = readU32(input);
  pointerCount = readS32(input);
  input->seek(4, librevenge::RVNG_SEEK_CUR);
}

void libvisio::VSDParser::handleStreams(librevenge::RVNGInputStream *input, unsigned ptrType, unsigned shift, unsigned level, std::set<unsigned> &visited)
{
  std::vector<unsigned> pointerOrder;
  std::map<unsigned, libvisio::Pointer> PtrList;
  std::map<unsigned, libvisio::Pointer> FontFaces;
  std::map<unsigned, libvisio::Pointer> NameList;
  std::map<unsigned, libvisio::Pointer> NameIDX;

  try
  {
    // Parse out pointers to streams
    unsigned listSize = 0;
    int pointerCount = 0;
    readPointerInfo(input, ptrType, shift, listSize, pointerCount);
    for (int i = 0; i < pointerCount; i++)
    {
      Pointer ptr;
      readPointer(input, ptr);
      if (ptr.Type == 0)
        continue;

      if (ptr.Type == VSD_FONTFACES)
        FontFaces[(unsigned int) i] = ptr;
      else if (ptr.Type == VSD_NAME_LIST2)
        NameList[(unsigned int) i] = ptr;
      else if (ptr.Type == VSD_NAMEIDX || ptr.Type == VSD_NAMEIDX123)
        NameIDX[(unsigned int) i] = ptr;
      else if (ptr.Type)
        PtrList[(unsigned int) i] = ptr;
    }
    if (listSize <= 1)
      listSize = 0;
    while (listSize--)
      pointerOrder.push_back(readU32(input));
  }
  catch (const EndOfStreamException &)
  {
    pointerOrder.clear();
    PtrList.clear();
    FontFaces.clear();
    NameList.clear();
  }

  std::map<unsigned, libvisio::Pointer>::iterator iter;
  for (iter = NameList.begin(); iter != NameList.end(); ++iter)
    handleStream(iter->second, iter->first, level+1, visited);

  for (iter = NameIDX.begin(); iter != NameIDX.end(); ++iter)
    handleStream(iter->second, iter->first, level+1, visited);

  for (iter = FontFaces.begin(); iter != FontFaces.end(); ++iter)
    handleStream(iter->second, iter->first, level+1, visited);

  if (!pointerOrder.empty())
  {
    for (unsigned int j : pointerOrder)
    {
      iter = PtrList.find(j);
      if (iter != PtrList.end())
      {
        handleStream(iter->second, iter->first, level+1, visited);
        PtrList.erase(iter);
      }
    }
  }
  for (iter = PtrList.begin(); iter != PtrList.end(); ++iter)
    handleStream(iter->second, iter->first, level+1, visited);

}

void libvisio::VSDParser::handleStream(const Pointer &ptr, unsigned idx, unsigned level, std::set<unsigned> &visited)
{
  m_header.level = (unsigned short) level;
  m_header.id = idx;
  m_header.chunkType = ptr.Type;
  _handleLevelChange(level);
  VSDStencil tmpStencil;
  bool compressed = ((ptr.Format & 2) == 2);
  m_input->seek(ptr.Offset, librevenge::RVNG_SEEK_SET);
  VSDInternalStream tmpInput(m_input, ptr.Length, compressed);
  m_header.dataLength = (unsigned) tmpInput.getSize();
  unsigned shift = compressed ? 4 : 0;
  switch (ptr.Type)
  {
  case VSD_STYLES:
    m_isInStyles = true;
    break;
  case VSD_PAGES:
    if (m_extractStencils)
      return;
    break;
  case VSD_PAGE:
    if (m_extractStencils)
      return;
    if (!(ptr.Format&0x1))
      m_isBackgroundPage = true;
    else
      m_isBackgroundPage = false;
    _nameFromId(m_currentPageName, idx, level+1);
    m_collector->startPage(idx);
    break;
  case VSD_STENCILS:
    if (m_extractStencils)
      break;
    if (m_stencils.count())
      return;
    m_isStencilStarted = true;
    break;
  case VSD_STENCIL_PAGE:
    if (m_extractStencils)
    {
      m_isBackgroundPage = false;
      _nameFromId(m_currentPageName, idx, level+1);
      m_collector->startPage(idx);
    }
    else
      m_currentStencil = &tmpStencil;
    break;
  case VSD_SHAPE_GROUP:
  case VSD_SHAPE_SHAPE:
  case VSD_SHAPE_FOREIGN:
    m_currentShapeID = idx;
    break;
  case VSD_OLE_LIST:
    if (!m_shape.m_foreign)
      m_shape.m_foreign = make_unique<ForeignData>();
    m_shape.m_foreign->dataId = idx;
    break;
  default:
    break;
  }

  if ((ptr.Format >> 4) == 0x4 || (ptr.Format >> 4) == 0x5 || (ptr.Format >> 4) == 0x0)
  {
    handleBlob(&tmpInput, shift, level+1);
    if ((ptr.Format >> 4) == 0x5 && ptr.Type != VSD_COLORS)
    {
      const auto it = visited.insert(ptr.Offset);
      if (it.second)
      {
        try
        {
          handleStreams(&tmpInput, ptr.Type, shift, level+1, visited);
        }
        catch (...)
        {
          visited.erase(it.first);
          throw;
        }
        visited.erase(it.first);
      }
    }
  }
  else if ((ptr.Format >> 4) == 0xd || (ptr.Format >> 4) == 0xc || (ptr.Format >> 4) == 0x8)
    handleChunks(&tmpInput, level+1);

  switch (ptr.Type)
  {
  case VSD_STYLES:
    _handleLevelChange(0);
    m_isInStyles = false;
    break;
  case VSD_PAGE:
    _handleLevelChange(0);
    m_collector->endPage();
    break;
  case VSD_PAGES:
    _handleLevelChange(0);
    m_collector->endPages();
    break;
  case VSD_STENCILS:
    _handleLevelChange(0);
    if (m_extractStencils)
      m_collector->endPages();
    else
      m_isStencilStarted = false;
    break;
  case VSD_STENCIL_PAGE:
    _handleLevelChange(0);
    if (m_extractStencils)
      m_collector->endPage();
    else if (m_currentStencil)
    {
      m_stencils.addStencil(idx, *m_currentStencil);
      m_currentStencil = nullptr;
    }
    break;
  case VSD_SHAPE_GROUP:
  case VSD_SHAPE_SHAPE:
  case VSD_SHAPE_FOREIGN:
    if (m_isStencilStarted)
    {
      _handleLevelChange(0);
      if (m_currentStencil)
        m_currentStencil->addStencilShape(m_shape.m_shapeId, m_shape);
    }
    break;
  default:
    break;
  }

}

void libvisio::VSDParser::handleBlob(librevenge::RVNGInputStream *input, unsigned shift, unsigned level)
{
  try
  {
    m_header.level = (unsigned short) level;
    input->seek(shift, librevenge::RVNG_SEEK_SET);
    m_header.dataLength -= shift;
    _handleLevelChange(m_header.level);
    handleChunk(input);
  }
  catch (EndOfStreamException &) { }
}

void libvisio::VSDParser::handleChunks(librevenge::RVNGInputStream *input, unsigned level)
{
  while (!input->isEnd())
  {
    if (!getChunkHeader(input))
      return;
    m_header.level += (unsigned short) level;
	long endPos = m_header.dataLength+m_header.trailer+input->tell();

    _handleLevelChange(m_header.level);
    handleChunk(input);
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }
}

void libvisio::VSDParser::handleChunk(librevenge::RVNGInputStream *input)
{
	// Comment by Dani:
	// we are interested just in extraction of textual data so for minimising source code size and boosting performance 
	// we skip progressing every other data (e.g geometry, font type, colors, etc.. )
	// original file: https://github.com/LibreOffice/libvisio/blob/master/src/lib/VSDParser.cpp

  switch (m_header.chunkType)
  {
  case VSD_SHAPE_GROUP:
  case VSD_SHAPE_SHAPE:
  case VSD_SHAPE_FOREIGN:
     readShape(input);
    break;
  case VSD_SHAPE_ID:
    readShapeId(input);
    break;
  case VSD_TEXT:
    readText(input);
    break;
  case VSD_PAGE:
    readPage(input);
    break;
  case VSD_STENCIL_PAGE:
    if (m_extractStencils)
      readPage(input);
    break;
  case VSD_NAME_LIST:
    readNameList(input);
    break;
  case VSD_NAME_LIST2:
    readNameList2(input);
    break;
  case VSD_PAGE_SHEET:
    readPageSheet(input);
    break;


 /* case VSD_NAME:
	  readPageProps(input);
	  break;*/


  case VSD_NAMEIDX:
	  readNameIDX(input);
	  break;

	// is Page-1 found here
  case VSD_NAME2:
	  readPageProps(input);
	  break;

	// skipped from progressing
	/*
		VSD_FONTFACE | VSD_COLORS | VSD_LAYER| VSD_GEOMETRY | VSD_MOVE_TO | VSD_LINE_TO | VSD_ARC_TO
		VSD_ELLIPSE | VSD_ELLIPTICAL_ARC_TO | VSD_NURBS_TO | VSD_POLYLINE_TO | VSD_INFINITE_LINE | VSD_SHAPE_DATA
		VSD_FOREIGN_DATA_TYPE | VSD_FONT_IX | VSD_SPLINE_START | VSD_CHAR_IX | VSD_PAGE_PROPS | VSD_FILL_AND_SHADOW
		VSD_TEXT_XFORM | VSD_XFORM_1D | VSD_XFORM_DATA | VSD_PARA_IX | VSD_NAME | VSD_STYLE_SHEET | VSD_SPLINE_KNOT
		VSD_LINE | VSD_TEXT_BLOCK | VSD_LAYER_MEMBERSHIP | VSD_NAME2 | VSD_SHAPE_LIST | VSD_LAYER_LIST | VSD_NAMEIDX
		VSD_NAMEIDX123 | VSD_GEOM_LIST | VSD_CHAR_LIST | VSD_PROP_LIST | VSD_PARA_LIST | VSD_FOREIGN_DATA 
		VSD_TEXT_FIELD | VSD_FIELD_LIST | VSD_OLE_LIST | VSD_OLE_DATA | VSD_TABS_DATA_LIST | VSD_TABS_DATA_1
		VSD_TABS_DATA_2 | VSD_TABS_DATA_3 | VSD_MISC | 
	*/
  }
}

void libvisio::VSDParser::_flushShape()
{
  if (!m_isShapeStarted)
    return;

   m_collector->collectShape(m_shape.m_shapeId, m_currentShapeLevel, m_shape.m_parent, m_shape.m_masterPage, m_shape.m_masterShape, m_shape.m_lineStyleId, m_shape.m_fillStyleId, m_shape.m_textStyleId);

   m_collector->collectShapesOrder(0, m_currentShapeLevel+2, m_shape.m_shapeList.getShapesOrder());

  if (m_shape.m_txtxform)
    m_collector->collectTxtXForm(m_currentShapeLevel+2, *(m_shape.m_txtxform));

  if (m_shape.m_foreign)
    m_collector->collectForeignDataType(m_currentShapeLevel+2, m_shape.m_foreign->type, m_shape.m_foreign->format,
                                        m_shape.m_foreign->offsetX, m_shape.m_foreign->offsetY, m_shape.m_foreign->width, m_shape.m_foreign->height);

  for (std::map<unsigned, VSDName>::const_iterator iterName = m_shape.m_names.begin(); iterName != m_shape.m_names.end(); ++iterName)
    m_collector->collectName(iterName->first, m_currentShapeLevel+2, iterName->second.m_data, iterName->second.m_format);

  if (m_shape.m_foreign && m_shape.m_foreign->data.size())
    m_collector->collectForeignData(m_currentShapeLevel+1, m_shape.m_foreign->data);

  if (!m_shape.m_fields.empty())
    m_shape.m_fields.handle(m_collector);

  if (m_shape.m_text.size())
    m_collector->collectText(m_currentShapeLevel+1, m_shape.m_text, m_shape.m_textFormat);


  m_shape.m_charList.handle(m_collector);


  m_shape.m_paraList.handle(m_collector);
}

void libvisio::VSDParser::_handleLevelChange(unsigned level)
{
  if (level == m_currentLevel)
    return;
  if (level <= m_currentShapeLevel+1)
  {
    m_collector->collectShapesOrder(0, m_currentShapeLevel+2, m_shapeList.getShapesOrder());
    m_shapeList.clear();

  }
  if (level <= m_currentShapeLevel)
  {
    if (!m_isStencilStarted)
    {
      _flushShape();
      m_shape.clear();
    }
    m_isShapeStarted = false;
    m_currentShapeLevel = 0;
  }
  m_currentLevel = level;
}

// --- READERS ---

void libvisio::VSDParser::readPage(librevenge::RVNGInputStream *input)
{
  input->seek(8, librevenge::RVNG_SEEK_CUR); //sub header length and children list length
  unsigned backgroundPageID = readU32(input);
  m_collector->collectPage(m_header.id, m_header.level, backgroundPageID, m_isBackgroundPage, m_currentPageName);
}

void libvisio::VSDParser::readShapeId(librevenge::RVNGInputStream *input)
{
	// (!) we need this, do not delete (!)

	if (!m_isShapeStarted)
		m_shapeList.addShapeId(m_header.id, getUInt(input));
	else
		m_shape.m_shapeList.addShapeId(m_header.id, getUInt(input));
}

void libvisio::VSDParser::readShape(librevenge::RVNGInputStream *input)
{
	// (!) we need this function (!)
	// TODO: study what else can be removed safely in this function

  m_currentGeomListCount = 0;
  m_isShapeStarted = true;
  m_shapeList.clear();
  if (m_header.id != MINUS_ONE)
    m_currentShapeID = m_header.id;
  m_currentShapeLevel = m_header.level;
  unsigned parent = 0;
  auto masterPage = MINUS_ONE;
  auto masterShape = MINUS_ONE;

  try {
    input->seek(10, librevenge::RVNG_SEEK_CUR);
    parent = readU32(input);
    input->seek(4, librevenge::RVNG_SEEK_CUR);
    masterPage = readU32(input);
    input->seek(4, librevenge::RVNG_SEEK_CUR);
    masterShape = readU32(input);
    input->seek(24, librevenge::RVNG_SEEK_CUR);
  }
  catch (const EndOfStreamException &) { }

  m_shape.clear();
  const VSDShape *tmpShape = m_stencils.getStencilShape(masterPage, masterShape);
  if (tmpShape)
  {
    if (tmpShape->m_foreign)
      m_shape.m_foreign = make_unique<ForeignData>(*(tmpShape->m_foreign));
    m_shape.m_xform = tmpShape->m_xform;
    if (tmpShape->m_txtxform)
      m_shape.m_txtxform = make_unique<XForm>(*(tmpShape->m_txtxform));
    m_shape.m_tabSets = tmpShape->m_tabSets;
    m_shape.m_text = tmpShape->m_text;
    m_shape.m_textFormat = tmpShape->m_textFormat;
    m_shape.m_misc = tmpShape->m_misc;
  }

  m_shape.m_parent = parent;
  m_shape.m_masterPage = masterPage;
  m_shape.m_masterShape = masterShape;
  m_shape.m_shapeId = m_currentShapeID;
  m_currentShapeID = MINUS_ONE;
}


void libvisio::VSDParser::readPageProps(librevenge::RVNGInputStream *input)
{
	// TODO: append page title to buffer

	unsigned short unicharacter = 0;
	librevenge::RVNGBinaryData name;
	input->seek(4, librevenge::RVNG_SEEK_CUR); // skip a dword that seems to be always 1
	while ((unicharacter = readU16(input)))
	{
		name.append(unicharacter & 0xff);
		name.append((unicharacter & 0xff00) >> 8);
	}
	name.append(unicharacter & 0xff);
	name.append((unicharacter & 0xff00) >> 8);
	m_names[m_header.id] = VSDName(name, libvisio::VSD_TEXT_UTF16);
}


void libvisio::VSDParser::readNameIDX(librevenge::RVNGInputStream *input)
{
	std::map<unsigned, VSDName> names;
	unsigned recordCount = readU32(input);
	if (recordCount > (unsigned int) getRemainingLength(input) / 13u)
		recordCount = (unsigned int) getRemainingLength(input) / 13u;
	for (unsigned i = 0; i < recordCount; ++i)
	{
		unsigned nameId = readU32(input);
		input->seek(4, librevenge::RVNG_SEEK_CUR);
		unsigned elementId = readU32(input);
		input->seek(1, librevenge::RVNG_SEEK_CUR);
		std::map<unsigned, VSDName>::const_iterator iter = m_names.find(nameId);
		if (iter != m_names.end())
			names[elementId] = iter->second;
	}
	m_namesMapMap[m_header.level] = names;
}


void libvisio::VSDParser::readNameList(librevenge::RVNGInputStream * /* input */)
{
  m_shape.m_names.clear();
}

void libvisio::VSDParser::readNameList2(librevenge::RVNGInputStream * /* input */)
{
  m_names.clear();
}

/* StyleSheet readers */


void libvisio::VSDParser::readPageSheet(librevenge::RVNGInputStream * /* input */)
{
  m_currentShapeLevel = m_header.level;
  m_collector->collectPageSheet(m_header.id, m_header.level);
}

void libvisio::VSDParser::readText(librevenge::RVNGInputStream *input)
{
	// (!) We need this function (!)

  input->seek(8, librevenge::RVNG_SEEK_CUR);
  librevenge::RVNGBinaryData textStream;

  // Read up to end of chunk in byte pairs (except from last 2 bytes)
  unsigned long numBytesRead = 0;
  const unsigned char *tmpBuffer = input->read(m_header.dataLength - 8, numBytesRead);
  if (numBytesRead)
  {
    textStream.append(tmpBuffer, numBytesRead);
    m_shape.m_text = textStream;
  }
  else
    m_shape.m_text.clear();
  m_shape.m_textFormat = libvisio::VSD_TEXT_UTF16;
}

unsigned libvisio::VSDParser::getUInt(librevenge::RVNGInputStream *input)
{
  return readU32(input);
}

int libvisio::VSDParser::getInt(librevenge::RVNGInputStream *input)
{
  return readS32(input);
}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
