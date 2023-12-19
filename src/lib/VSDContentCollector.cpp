/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libvisio project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "VSDContentCollector.h"

#include <algorithm>
#include <cassert>
#include <cstring> // for memcpy
#include <limits>
#include <set>
#include <stack>
// #include <boost/spirit/include/qi.hpp>

#include "VSDParser.h"
#include "VSDInternalStream.h"

// imported from EPP
#include "../CodepageUtils.h"
using namespace docfb::cputil;

#ifndef DUMP_BITMAP
#define DUMP_BITMAP 0
#endif

#if DUMP_BITMAP
static unsigned bitmapId = 0;
#include <sstream>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{

unsigned computeBMPDataOffset(librevenge::RVNGInputStream *const input, const unsigned long maxLength)
{
  assert(input);

  using namespace libvisio;

  // determine header size
  unsigned headerSize = readU32(input);
  if (headerSize > maxLength)
    headerSize = 40; // assume v.3 bitmap header size
  unsigned off = headerSize;

  // determine palette size
  input->seek(10, librevenge::RVNG_SEEK_CUR);
  unsigned bpp = readU16(input);
  // sanitize bpp - limit to the allowed range and then round up to one
  // of the allowed values
  if (bpp > 32)
    bpp = 32;
  const unsigned allowedBpp[] = {1, 4, 8, 16, 24, 32};
  size_t bppIdx = 0;
  while (bppIdx < VSD_NUM_ELEMENTS(allowedBpp) && bpp > allowedBpp[bppIdx])
    ++bppIdx;
  if (bpp < allowedBpp[bppIdx])
    bpp = allowedBpp[bppIdx];
  input->seek(16, librevenge::RVNG_SEEK_CUR);
  unsigned paletteColors = readU32(input);
  if (bpp < 16 && paletteColors == 0)
    paletteColors = 1 << bpp;
  assert(maxLength >= off);
  if (paletteColors > 0 && (paletteColors < (maxLength - off) / 4))
    off += 4 * paletteColors;

  off += 14; // file header size

  return off;
}

} // anonymous namespace

libvisio::VSDContentCollector::VSDContentCollector(
  librevenge::RVNGDrawingInterface *painter,
  std::vector<std::map<unsigned, XForm> > &groupXFormsSequence,
  std::vector<std::map<unsigned, unsigned> > &groupMembershipsSequence,
  std::vector<std::list<unsigned> > &documentPageShapeOrders, VSDStencils &stencils
) :
  m_painter(painter), m_isPageStarted(false), m_pageWidth(0.0), m_pageHeight(0.0),
  m_shadowOffsetX(0.0), m_shadowOffsetY(0.0),
  m_scale(1.0), m_x(0.0), m_y(0.0), m_originalX(0.0), m_originalY(0.0), m_xform(), m_txtxform(), m_misc(),
  m_currentFillGeometry(), m_currentLineGeometry(), m_groupXForms(groupXFormsSequence.empty() ? nullptr : &groupXFormsSequence[0]),
  m_currentForeignData(), m_currentOLEData(), m_currentForeignProps(), m_currentShapeId(0), m_foreignType((unsigned)-1),
  m_foreignFormat(0), m_foreignOffsetX(0.0), m_foreignOffsetY(0.0), m_foreignWidth(0.0), m_foreignHeight(0.0),
  m_noLine(false), m_noFill(false), m_noShow(false), m_fonts(),
  m_currentLevel(0), m_isShapeStarted(false),
  m_groupXFormsSequence(groupXFormsSequence), m_groupMembershipsSequence(groupMembershipsSequence),
  m_groupMemberships(m_groupMembershipsSequence.begin()),
  m_currentPageNumber(0), m_shapeOutputDrawing(nullptr), m_shapeOutputText(nullptr),
  m_pageOutputDrawing(), m_pageOutputText(), m_documentPageShapeOrders(documentPageShapeOrders),
  m_pageShapeOrder(m_documentPageShapeOrders.begin()), m_isFirstGeometry(true), m_NURBSData(), m_polylineData(),
  m_currentText(), m_names(), m_stencilNames(), m_fields(), m_stencilFields(), m_fieldIndex(0),
  m_charFormats(), m_currentStyleSheet(0),
  m_stencils(stencils), m_stencilShape(nullptr), m_isStencilStarted(false), m_currentGeometryCount(0),
  m_backgroundPageID(MINUS_ONE), m_currentPageID(0), m_currentPage(), m_pages(),
  m_splineControlPoints(), m_splineKnotVector(), m_splineX(0.0), m_splineY(0.0),
  m_splineLastKnot(0.0), m_splineDegree(0), m_splineLevel(0), m_currentShapeLevel(0),
  m_isBackgroundPage(false), m_currentLayerMem(), m_tabSets()
{
}

void libvisio::VSDContentCollector::_flushShape()
{
  unsigned shapeId = m_currentShapeId;

  if (!m_currentText.empty())
  {
    if ((m_currentText.m_format == VSD_TEXT_UTF16
         && (m_currentText.m_data.size() >= 2 && (m_currentText.m_data.getDataBuffer()[0] || m_currentText.m_data.getDataBuffer()[1])))
        || m_currentText.m_data.getDataBuffer()[0])
    { }
  }

  _flushCurrentPath(shapeId);

  _flushText();

  m_isShapeStarted = false;
}

void libvisio::VSDContentCollector::_flushCurrentPath(unsigned /*shapeId*/)
{
//  librevenge::RVNGPropertyList styleProps;
//  librevenge::RVNGPropertyList fillPathProps(styleProps);
//  librevenge::RVNGPropertyList linePathProps(styleProps);
//  std::vector<librevenge::RVNGPropertyList> tmpPath;

  m_currentFillGeometry.clear();
//  tmpPath.clear();

  m_currentLineGeometry.clear();
}

void libvisio::VSDContentCollector::_flushText()
{
  /* Do not output empty text objects. */
  if (m_currentText.empty() || m_misc.m_hideText)
    return;
  else
    // Check whether the buffer contains only the terminating nullptr character
  {
    if (m_currentText.m_format == VSD_TEXT_UTF16)
    {
      if (m_currentText.m_data.size() < 2)
        return;
      else if (!(m_currentText.m_data.getDataBuffer()[0]) && !(m_currentText.m_data.getDataBuffer()[1]))
        return;
    }
    else if (!(m_currentText.m_data.getDataBuffer()[0]))
      return;
  }

  /* Fill the text object/frame properties */
  double xmiddle = m_txtxform ? m_txtxform->width / 2.0 : m_xform.width / 2.0;
  double ymiddle = m_txtxform ? m_txtxform->height / 2.0 : m_xform.height / 2.0;

  transformPoint(xmiddle,ymiddle, m_txtxform.get());

  // double angle = 0.0;
  // transformAngle(angle, m_txtxform.get());

  librevenge::RVNGPropertyList textBlockProps;

  // angle = std::fmod(angle, 2 * M_PI);
  // if (angle < 0)
  //   angle += 2 * M_PI;

  // _appendVisibleAndPrintable(textBlockProps);

  /* Start the text object. */
  m_shapeOutputText->addStartTextObject(textBlockProps);

  /* Assure that the different styles have at least one element,
   * corresponding to the default style. */
  if (m_charFormats.empty())
  {
    m_charFormats.push_back(m_defaultCharStyle);
    m_charFormats.back().charCount = 0;
  }

  if (m_tabSets.empty())
  {
    m_tabSets.push_back(VSDTabSet());
    m_tabSets.back().m_numChars = 0;
  }

  /* Helper variables used to iterate over the text buffer */
  std::vector<VSDCharStyle>::const_iterator charIt = m_charFormats.begin();
  std::vector<VSDTabSet>::const_iterator tabIt = m_tabSets.begin();

  bool isParagraphOpened(false);
  bool isSpanOpened(false);
  bool isParagraphWithoutSpan(false);

  VSDBullet currentBullet;

  //unsigned paraNumRemaining(paraIt->charCount);
  unsigned charNumRemaining(charIt->charCount);
  unsigned tabNumRemaining(tabIt->m_numChars);

  std::vector<unsigned char> sOutputVector;
  librevenge::RVNGString sOutputText;

  // Unfortunately, we have to handle the unicode formats differently then the 8-bit formats
  if (m_currentText.m_format == VSD_TEXT_UTF8 || m_currentText.m_format == VSD_TEXT_UTF16)
  {
    std::vector<unsigned char> tmpBuffer(m_currentText.m_data.size());
    memcpy(&tmpBuffer[0], m_currentText.m_data.getDataBuffer(), m_currentText.m_data.size());
    librevenge::RVNGString textString;
    appendCharacters(textString, tmpBuffer, m_currentText.m_format);
    /* Iterate over the text character by character */
    librevenge::RVNGString::Iter textIt(textString);
    for (textIt.rewind(); textIt.next();)
    {
      /* Any character will cause a paragraph to open if it is not yet opened. */
      if (!isParagraphOpened)
      {
        librevenge::RVNGPropertyList paraProps;
        if (!currentBullet)
          m_shapeOutputText->addOpenParagraph(paraProps);
        else
          m_shapeOutputText->addOpenListElement(paraProps);
        isParagraphOpened = true;
        isParagraphWithoutSpan = true;
      }

      /* Any character will cause a span to open if it is not yet opened.
       * The additional conditions aim to avoid superfluous empty span but
       * also a paragraph without span at all. */
      if (!isSpanOpened && ((*(textIt()) != '\n') || isParagraphWithoutSpan))
      {
        librevenge::RVNGPropertyList textProps;
        // _fillCharProperties(textProps, *charIt);

        m_shapeOutputText->addOpenSpan(textProps);
        isSpanOpened = true;
        isParagraphWithoutSpan = false;
      }

      /* Current character is a paragraph break,
       * which will cause the paragraph to close. */
      if (*(textIt()) == '\n')
      {
        if (!sOutputText.empty())
          m_shapeOutputText->addInsertText(sOutputText);
        sOutputText.clear();
        if (isSpanOpened)
        {
          m_shapeOutputText->addCloseSpan();
          isSpanOpened = false;
        }

        if (isParagraphOpened)
        {
          if (!currentBullet)
            m_shapeOutputText->addCloseParagraph();
          else
            m_shapeOutputText->addCloseListElement();
          isParagraphOpened = false;
        }
      }
      /* Current character is a tabulator. We have to output
       * the current text buffer and insert the tab. */
      else if (*(textIt()) == '\t')
      {
        if (!sOutputText.empty())
          m_shapeOutputText->addInsertText(sOutputText);
        sOutputText.clear();
        m_shapeOutputText->addInsertTab();
      }
      /* Current character is a field placeholder. We append
       * to the current text buffer a text representation
       * of the field. */
      else if (strlen(textIt()) == 3 &&
               textIt()[0] == '\xef' &&
               textIt()[1] == '\xbf' &&
               textIt()[2] == '\xbc')
        _appendField(sOutputText);
      /* We have a normal UTF8 character and we append it
       * to the current text buffer. */
      else
        sOutputText.append(textIt());

      /* Decrease the count of remaining characters in the same span,
       * if it is possible. */
      if (charNumRemaining)
        charNumRemaining--;
      /* Fetch next character style if it exists and close span, since
       * the next span will have to use the new character style.
       * If there is no more character style to fetch, just finish using
       * the last one. */
      if (!charNumRemaining)
      {
        ++charIt;
        if (charIt != m_charFormats.end())
        {
          charNumRemaining = charIt->charCount;
          if (isSpanOpened)
          {
            if (!sOutputText.empty())
              m_shapeOutputText->addInsertText(sOutputText);
            sOutputText.clear();
            m_shapeOutputText->addCloseSpan();
            isSpanOpened = false;
          }
        }
        else
          --charIt;
      }

      /* Decrease the count of remaining characters using the same
       * tab-set definition, if it is possible. */
      if (tabNumRemaining)
        tabNumRemaining--;
      /* Fetch next tab-set definition if it exists. If not, just use the
       * last one. */
      if (!tabNumRemaining)
      {
        ++tabIt;
        if (tabIt != m_tabSets.end())
          tabNumRemaining = tabIt->m_numChars;
        else
          --tabIt;
      }
    }
  }
  else // 8-bit charsets
  {
    /* Iterate over the text character by character */
    const unsigned char *tmpBuffer = m_currentText.m_data.getDataBuffer();
    unsigned long tmpBufferLength = m_currentText.m_data.size();
    // Remove the terminating \0 character from the buffer
    while (tmpBufferLength > 1 &&!tmpBuffer[tmpBufferLength-1])
    {
      --tmpBufferLength;
    }
    for (unsigned long i = 0; i < tmpBufferLength; ++i)
    {
      /* Any character will cause a paragraph to open if it is not yet opened. */
      if (!isParagraphOpened)
      {
        librevenge::RVNGPropertyList paraProps;

        if (!currentBullet)
          m_shapeOutputText->addOpenParagraph(paraProps);
        else
          m_shapeOutputText->addOpenListElement(paraProps);
        isParagraphOpened = true;
        isParagraphWithoutSpan = true;
      }

      /* Any character will cause a span to open if it is not yet opened.
       * The additional conditions aim to avoid superfluous empty span but
       * also a paragraph without span at all. */
      if (!isSpanOpened && ((tmpBuffer[i] != (unsigned char)'\n' && tmpBuffer[i] != 0x0d && tmpBuffer[i] != 0x0e) || isParagraphWithoutSpan))
      {
        librevenge::RVNGPropertyList textProps;
        // _fillCharProperties(textProps, *charIt);

        m_shapeOutputText->addOpenSpan(textProps);
        isSpanOpened = true;
        isParagraphWithoutSpan = false;
      }

      /* Current character is a paragraph break,
       * which will cause the paragraph to close. */
      if (tmpBuffer[i] == (unsigned char)'\n' || tmpBuffer[i] == 0x0d || tmpBuffer[i] == 0x0e)
      {
        if (!sOutputVector.empty())
        {
          appendCharacters(sOutputText, sOutputVector, charIt->font.m_format);
          sOutputVector.clear();
        }
        if (!sOutputText.empty())
        {
          m_shapeOutputText->addInsertText(sOutputText);
          sOutputText.clear();
        }
        if (isSpanOpened)
        {
          m_shapeOutputText->addCloseSpan();
          isSpanOpened = false;
        }

        if (isParagraphOpened)
        {
          if (!currentBullet)
            m_shapeOutputText->addCloseParagraph();
          else
            m_shapeOutputText->addCloseListElement();
          isParagraphOpened = false;
        }
      }
      /* Current character is a tabulator. We have to output
       * the current text buffer and insert the tab. */
      else if (tmpBuffer[i] == (unsigned char)'\t')
      {
        if (!sOutputVector.empty())
        {
          appendCharacters(sOutputText, sOutputVector, charIt->font.m_format);
          sOutputVector.clear();
        }
        if (!sOutputText.empty())
        {
          m_shapeOutputText->addInsertText(sOutputText);
          sOutputText.clear();
        }
        m_shapeOutputText->addInsertTab();
      }
      /* Current character is a field placeholder. We append
       * to the current text buffer a text representation
       * of the field. */
      else if (tmpBuffer[i] == 0x1e)
      {
        if (!sOutputVector.empty())
        {
          appendCharacters(sOutputText, sOutputVector, charIt->font.m_format);
          sOutputVector.clear();
        }
        _appendField(sOutputText);
      }
      /* We have a normal UTF8 character and we append it
       * to the current text buffer. */
      else
        sOutputVector.push_back(tmpBuffer[i]);

      /* Decrease the count of remaining characters in the same span,
       * if it is possible. */
      if (charNumRemaining)
        charNumRemaining--;
      /* Fetch next character style if it exists and close span, since
       * the next span will have to use the new character style.
       * If there is no more character style to fetch, just finish using
       * the last one. */
      if (!charNumRemaining)
      {
        ++charIt;
        if (charIt != m_charFormats.end())
        {
          charNumRemaining = charIt->charCount;
          if (isSpanOpened)
          {
            if (!sOutputVector.empty())
            {
              appendCharacters(sOutputText, sOutputVector, charIt->font.m_format);
              sOutputVector.clear();
            }
            if (!sOutputText.empty())
            {
              m_shapeOutputText->addInsertText(sOutputText);
              sOutputText.clear();
            }
            m_shapeOutputText->addCloseSpan();
            isSpanOpened = false;
          }
        }
        else
          --charIt;
      }

      /* Decrease the count of remaining characters using the same
       * tab-set definition, if it is possible. */
      if (tabNumRemaining)
        tabNumRemaining--;
      /* Fetch next tab-set definition if it exists. If not, just use the
       * last one. */
      if (!tabNumRemaining)
      {
        ++tabIt;
        if (tabIt != m_tabSets.end())
          tabNumRemaining = tabIt->m_numChars;
        else
          --tabIt;
      }
    }
  }

  // Clean up the elements that remained opened
  if (isParagraphOpened)
  {
    if (isSpanOpened)
    {
      if (!sOutputVector.empty())
      {
        appendCharacters(sOutputText, sOutputVector, charIt->font.m_format);
        sOutputVector.clear();
      }
      if (!sOutputText.empty())
      {
        m_shapeOutputText->addInsertText(sOutputText);
        sOutputText.clear();
      }
      m_shapeOutputText->addCloseSpan();
    }

    if (!currentBullet)
      m_shapeOutputText->addCloseParagraph();
    else
      m_shapeOutputText->addCloseListElement();
  }

  /* Last paragraph style had a bullet and we have to close
   * the corresponding list level. */
  if (!!currentBullet)
    m_shapeOutputText->addCloseUnorderedListLevel();

  m_shapeOutputText->addEndTextObject();
  m_currentText.clear();
}

void libvisio::VSDContentCollector::_flushCurrentPage()
{
  if (m_pageShapeOrder != m_documentPageShapeOrders.end() && !m_pageShapeOrder->empty() &&
      m_groupMemberships != m_groupMembershipsSequence.end())
  {
    std::stack<std::pair<unsigned, VSDOutputElementList> > groupTextStack;
    for (unsigned int &iterList : *m_pageShapeOrder)
    {
      auto iterGroup = m_groupMemberships->find(iterList);
      if (iterGroup == m_groupMemberships->end())
      {
        while (!groupTextStack.empty())
        {
          m_currentPage.append(groupTextStack.top().second);
          groupTextStack.pop();
        }
      }
      else if (!groupTextStack.empty() && iterGroup->second != groupTextStack.top().first)
      {
        while (!groupTextStack.empty() && groupTextStack.top().first != iterGroup->second)
        {
          m_currentPage.append(groupTextStack.top().second);
          groupTextStack.pop();
        }
      }

      std::map<unsigned, VSDOutputElementList>::iterator iter;
      iter = m_pageOutputDrawing.find(iterList);
      if (iter != m_pageOutputDrawing.end())
        m_currentPage.append(iter->second);
      iter = m_pageOutputText.find(iterList);
      if (iter != m_pageOutputText.end())
        groupTextStack.push(std::make_pair(iterList, iter->second));
      else
        groupTextStack.push(std::make_pair(iterList, VSDOutputElementList()));
    }
    while (!groupTextStack.empty())
    {
      m_currentPage.append(groupTextStack.top().second);
      groupTextStack.pop();
    }
  }
  m_pageOutputDrawing.clear();
  m_pageOutputText.clear();
}

// #define LIBVISIO_EPSILON 1E-10

void libvisio::VSDContentCollector::collectForeignData(unsigned level, const librevenge::RVNGBinaryData &binaryData)
{
  _handleLevelChange(level);
  _handleForeignData(binaryData);
}

void libvisio::VSDContentCollector::_handleForeignData(const librevenge::RVNGBinaryData &binaryData)
{
  if (m_foreignType == 0 || m_foreignType == 1 || m_foreignType == 4) // Image
  {
    m_currentForeignData.clear();
    // If bmp data found, reconstruct header
    if (m_foreignType == 1 && m_foreignFormat == 0)
    {
      m_currentForeignData.append(0x42);
      m_currentForeignData.append(0x4d);

      m_currentForeignData.append((unsigned char)((binaryData.size() + 14) & 0x000000ff));
      m_currentForeignData.append((unsigned char)(((binaryData.size() + 14) & 0x0000ff00) >> 8));
      m_currentForeignData.append((unsigned char)(((binaryData.size() + 14) & 0x00ff0000) >> 16));
      m_currentForeignData.append((unsigned char)(((binaryData.size() + 14) & 0xff000000) >> 24));

      m_currentForeignData.append((unsigned char)0x00);
      m_currentForeignData.append((unsigned char)0x00);
      m_currentForeignData.append((unsigned char)0x00);
      m_currentForeignData.append((unsigned char)0x00);

      const unsigned dataOff = computeBMPDataOffset(binaryData.getDataStream(), binaryData.size());
      m_currentForeignData.append((unsigned char)(dataOff & 0xff));
      m_currentForeignData.append((unsigned char)((dataOff >> 8) & 0xff));
      m_currentForeignData.append((unsigned char)((dataOff >> 16) & 0xff));
      m_currentForeignData.append((unsigned char)((dataOff >> 24) & 0xff));
    }
    m_currentForeignData.append(binaryData);

    if (m_foreignType == 1)
    {
      switch (m_foreignFormat)
      {
      case 0:
      case 255:
        m_currentForeignProps.insert("librevenge:mime-type", "image/bmp");
        break;
      case 1:
        m_currentForeignProps.insert("librevenge:mime-type", "image/jpeg");
        break;
      case 2:
        m_currentForeignProps.insert("librevenge:mime-type", "image/gif");
        break;
      case 3:
        m_currentForeignProps.insert("librevenge:mime-type", "image/tiff");
        break;
      case 4:
        m_currentForeignProps.insert("librevenge:mime-type", "image/png");
        break;
      }
    }
    else if (m_foreignType == 0 || m_foreignType == 4)
    {
      const unsigned char *tmpBinData = m_currentForeignData.getDataBuffer();
      // Check for EMF signature
      if (m_currentForeignData.size() > 0x2B && tmpBinData[0x28] == 0x20 && tmpBinData[0x29] == 0x45 && tmpBinData[0x2A] == 0x4D && tmpBinData[0x2B] == 0x46)
        m_currentForeignProps.insert("librevenge:mime-type", "image/emf");
      else
        m_currentForeignProps.insert("librevenge:mime-type", "image/wmf");
    }
  }
  else if (m_foreignType == 2)
  {
    m_currentForeignProps.insert("librevenge:mime-type", "object/ole");
    m_currentForeignData.append(binaryData);
  }

#if DUMP_BITMAP
  librevenge::RVNGString filename;
  if (m_foreignType == 1)
  {
    switch (m_foreignFormat)
    {
    case 0:
    case 255:
      filename.sprintf("binarydump%08u.bmp", bitmapId++);
      break;
    case 1:
      filename.sprintf("binarydump%08u.jpeg", bitmapId++);
      break;
    case 2:
      filename.sprintf("binarydump%08u.gif", bitmapId++);
      break;
    case 3:
      filename.sprintf("binarydump%08u.tiff", bitmapId++);
      break;
    case 4:
      filename.sprintf("binarydump%08u.png", bitmapId++);
      break;
    default:
      filename.sprintf("binarydump%08u.bin", bitmapId++);
      break;
    }
  }
  else if (m_foreignType == 0 || m_foreignType == 4)
  {
    const unsigned char *tmpBinData = m_currentForeignData.getDataBuffer();
    // Check for EMF signature
    if (m_currentForeignData.size() > 0x2B && tmpBinData[0x28] == 0x20 && tmpBinData[0x29] == 0x45 && tmpBinData[0x2A] == 0x4D && tmpBinData[0x2B] == 0x46)
      filename.sprintf("binarydump%08u.emf", bitmapId++);
    else
      filename.sprintf("binarydump%08u.wmf", bitmapId++);
  }
  else if (m_foreignType == 2)
    filename.sprintf("binarydump%08u.ole", bitmapId++);
  else
    filename.sprintf("binarydump%08u.bin", bitmapId++);

  FILE *f = fopen(filename.cstr(), "wb");
  if (f)
  {
    const unsigned char *tmpBuffer = m_currentForeignData.getDataBuffer();
    for (unsigned long k = 0; k < m_currentForeignData.size(); k++)
      fprintf(f, "%c",tmpBuffer[k]);
    fclose(f);
  }
#endif
}

// #define VSD_NUM_POLYLINES_PER_KNOT 100
// #define MAX_ALLOWED_NURBS_DEGREE 8

void libvisio::VSDContentCollector::collectTxtXForm(unsigned level, const XForm &txtxform)
{
  _handleLevelChange(level);
  m_txtxform.reset(new XForm(txtxform));
  m_txtxform->x = m_txtxform->pinX - m_txtxform->pinLocX;
  m_txtxform->y = m_txtxform->pinY - m_txtxform->pinLocY;
}

void libvisio::VSDContentCollector::transformPoint(double &/*x*/, double &y, XForm * /*txtxform*/)
{
  // We are interested for the while in shapes xforms only
  if (!m_isShapeStarted)
    return;

  if (!m_currentShapeId)
    return;

  unsigned shapeId = m_currentShapeId;

  std::set<unsigned> visitedShapes; // avoid mutually nested shapes in broken files
  visitedShapes.insert(shapeId);

  while (true && m_groupXForms)
  {
    auto iterX = m_groupXForms->find(shapeId);
    if (iterX != m_groupXForms->end())
    {
      // XForm xform = iterX->second;
      // applyXForm(x, y, xform);
    }
    else
      break;
    bool shapeFound = false;
    if (m_groupMemberships != m_groupMembershipsSequence.end())
    {
      auto iter = m_groupMemberships->find(shapeId);
      if (iter != m_groupMemberships->end() && shapeId != iter->second)
      {
        shapeId = iter->second;
        shapeFound = visitedShapes.insert(shapeId).second;
      }
    }
    if (!shapeFound)
      break;
  }
  y = m_pageHeight - y;
}


void libvisio::VSDContentCollector::collectShapesOrder(unsigned /* id */, unsigned level, const std::vector<unsigned> & /* shapeIds */)
{
  _handleLevelChange(level);
}

void libvisio::VSDContentCollector::collectForeignDataType(unsigned level, unsigned foreignType, unsigned foreignFormat, double offsetX, double offsetY, double width, double height)
{
  _handleLevelChange(level);
  m_foreignType = foreignType;
  m_foreignFormat = foreignFormat;
  m_foreignOffsetX = offsetX;
  m_foreignOffsetY = offsetY;
  m_foreignWidth = width;
  m_foreignHeight = height;
}

void libvisio::VSDContentCollector::collectPage(unsigned /* id */, unsigned level, unsigned backgroundPageID, bool isBackgroundPage, const VSDName &pageName)
{
  _handleLevelChange(level);
  m_currentPage.m_backgroundPageID = backgroundPageID;
  m_currentPage.m_pageName.clear();
  if (!pageName.empty())
    _convertDataToString(m_currentPage.m_pageName, pageName.m_data, pageName.m_format);
  m_isBackgroundPage = isBackgroundPage;
}

void libvisio::VSDContentCollector::collectShape(unsigned id, unsigned level, unsigned /*parent*/, unsigned masterPage, 
										unsigned masterShape, unsigned /*lineStyleId*/, 
										unsigned /*fillStyleId*/, unsigned /*textStyleId*/)
{
  _handleLevelChange(level);
  m_currentShapeLevel = level;

  m_foreignType = (unsigned)-1; // Tracks current foreign data type
  m_foreignFormat = 0; // Tracks foreign data format
  m_foreignOffsetX = 0.0;
  m_foreignOffsetY = 0.0;
  m_foreignWidth = 0.0;
  m_foreignHeight = 0.0;

  m_originalX = 0.0;
  m_originalY = 0.0;
  m_x = 0;
  m_y = 0;

  // Geometry flags
  m_noLine = false;
  m_noFill = false;
  m_noShow = false;
  m_isFirstGeometry = true;

  m_misc = VSDMisc();

  // Save line colour and pattern, fill type and pattern
  m_currentText.clear();
  m_charFormats.clear();
  //m_paraFormats.clear();

  m_currentShapeId = id;
  m_pageOutputDrawing[m_currentShapeId] = VSDOutputElementList();
  m_pageOutputText[m_currentShapeId] = VSDOutputElementList();
  m_shapeOutputDrawing = &m_pageOutputDrawing[m_currentShapeId];
  m_shapeOutputText = &m_pageOutputText[m_currentShapeId];
  m_isShapeStarted = true;
  m_isFirstGeometry = true;

  m_names.clear();
  m_stencilNames.clear();
  m_fields.clear();
  m_stencilFields.clear();

  // Get stencil shape
  m_stencilShape = m_stencils.getStencilShape(masterPage, masterShape);

  m_defaultCharStyle = VSDCharStyle();
  //m_defaultParaStyle = VSDParaStyle();
  if (m_stencilShape)
  {
    if (m_stencilShape->m_foreign)
    {
      m_foreignType = m_stencilShape->m_foreign->type;
      m_foreignFormat = m_stencilShape->m_foreign->format;
      m_foreignOffsetX = m_stencilShape->m_foreign->offsetX;
      m_foreignOffsetY = m_stencilShape->m_foreign->offsetY;
      m_foreignWidth = m_stencilShape->m_foreign->width;
      m_foreignHeight = m_stencilShape->m_foreign->height;
      m_currentForeignData.clear();
      _handleForeignData(m_stencilShape->m_foreign->data);
    }

    for (const auto &name : m_stencilShape->m_names)
    {
      librevenge::RVNGString nameString;
      _convertDataToString(nameString, name.second.m_data, name.second.m_format);
      m_stencilNames[name.first] = nameString;
    }

    if (m_stencilShape->m_txtxform)
      m_txtxform.reset(new XForm(*(m_stencilShape->m_txtxform)));

    m_stencilFields = m_stencilShape->m_fields;
    for (size_t i = 0; i < m_stencilFields.size(); i++)
    {
      VSDFieldListElement *elem = m_stencilFields.getElement((unsigned int) i);
      if (elem)
        m_fields.push_back(elem->getString(m_stencilNames));
      else
        m_fields.push_back(librevenge::RVNGString());
    }

  }

  m_currentGeometryCount = 0;
  m_fieldIndex = 0;
}

//void libvisio::VSDContentCollector::collectUnhandledChunk(unsigned /* id */, unsigned level)
//{
//  _handleLevelChange(level);
//}


void libvisio::VSDContentCollector::collectText(unsigned level, const librevenge::RVNGBinaryData &textStream, TextFormat format)
{
  _handleLevelChange(level);

  m_currentText.clear();
  if (!textStream.empty())
    m_currentText = libvisio::VSDName(textStream, format);
}

void libvisio::VSDContentCollector::_convertDataToString(librevenge::RVNGString &result, const librevenge::RVNGBinaryData &data, TextFormat format)
{
  if (!data.size())
    return;
  std::vector<unsigned char> tmpData(data.size());
  memcpy(&tmpData[0], data.getDataBuffer(), data.size());
  appendCharacters(result, tmpData, format);
}

void libvisio::VSDContentCollector::collectName(unsigned id, unsigned level, const librevenge::RVNGBinaryData &name, TextFormat format)
{
  _handleLevelChange(level);

  librevenge::RVNGString nameString;
  _convertDataToString(nameString, name, format);
  m_names[id] = nameString;
}

void libvisio::VSDContentCollector::collectPageSheet(unsigned /* id */, unsigned level)
{
  _handleLevelChange(level);
  m_currentShapeLevel = level;
  // m_currentLayerList.clear();
}

void libvisio::VSDContentCollector::collectFieldList(unsigned /* id */, unsigned level)
{
  _handleLevelChange(level);
  m_fields.clear();
}

void libvisio::VSDContentCollector::collectTextField(unsigned id, unsigned level, int nameId, int formatStringId)
{
  _handleLevelChange(level);
  VSDFieldListElement *element = m_stencilFields.getElement((unsigned int) m_fields.size());
  if (element)
  {
    if (nameId == -2)
      m_fields.push_back(element->getString(m_stencilNames));
    else
    {
      if (nameId >= 0)
        m_fields.push_back(m_names[(unsigned) nameId]);
      else
        m_fields.push_back(librevenge::RVNGString());
    }
  }
  else
  {
    VSDTextField tmpField(id, level, nameId, formatStringId);
    m_fields.push_back(tmpField.getString(m_names));
  }
}

void libvisio::VSDContentCollector::collectNumericField(unsigned id, unsigned level, unsigned short format, double number, int formatStringId)
{
  _handleLevelChange(level);
  VSDFieldListElement *pElement = m_stencilFields.getElement((unsigned int) m_fields.size());
  if (pElement)
  {
    std::unique_ptr<VSDFieldListElement> element{pElement->clone()};
    if (element)
    {
      element->setValue(number);
      if (format == 0xffff)
      {
        std::map<unsigned, librevenge::RVNGString>::const_iterator iter = m_names.find((unsigned) formatStringId);
		if (iter != m_names.end()) {}
          // parseFormatId(iter->second.cstr(), format);
      }
      if (format != 0xffff)
        element->setFormat(format);

      m_fields.push_back(element->getString(m_names));
    }
  }
  else
  {
    VSDNumericField tmpField(id, level, format, number, formatStringId);
    m_fields.push_back(tmpField.getString(m_names));
  }
}

void libvisio::VSDContentCollector::_handleLevelChange(unsigned level)
{
  if (m_currentLevel == level)
    return;
  if (level <= m_currentShapeLevel)
  {
    if (m_isShapeStarted)
    {
      if (m_stencilShape && !m_isStencilStarted)
      {
        m_isStencilStarted = true;
        m_NURBSData = m_stencilShape->m_nurbsData;
        m_polylineData = m_stencilShape->m_polylineData;

        m_isStencilStarted = false;
      }
      _flushShape();
    }
    m_originalX = 0.0;
    m_originalY = 0.0;
    m_x = 0;
    m_y = 0;
    m_txtxform.reset();
    m_xform = XForm();
    m_NURBSData.clear();
    m_polylineData.clear();
  }

  m_currentLevel = level;
}

void libvisio::VSDContentCollector::startPage(unsigned pageId)
{
  if (m_isShapeStarted)
    _flushShape();
  m_originalX = 0.0;
  m_originalY = 0.0;
  m_txtxform.reset();
  m_xform = XForm();
  m_x = 0;
  m_y = 0;
  m_currentPageNumber++;
  if (m_groupXFormsSequence.size() >= m_currentPageNumber)
    m_groupXForms = m_groupXFormsSequence.size() > m_currentPageNumber-1 ? &m_groupXFormsSequence[m_currentPageNumber-1] : nullptr;
  if (m_groupMembershipsSequence.size() >= m_currentPageNumber)
    m_groupMemberships = m_groupMembershipsSequence.begin() + (m_currentPageNumber-1);
  if (m_documentPageShapeOrders.size() >= m_currentPageNumber)
    m_pageShapeOrder = m_documentPageShapeOrders.begin() + (m_currentPageNumber-1);
  m_currentPage = libvisio::VSDPage();
  m_currentPage.m_currentPageID = pageId;
  m_isPageStarted = true;
}

void libvisio::VSDContentCollector::endPage()
{
  if (m_isPageStarted)
  {
    _handleLevelChange(0);
    _flushCurrentPage();
    // TODO: this check does not prevent two pages mutually referencing themselves
    // as their background pages. Or even longer cycle of pages.
    if (m_currentPage.m_backgroundPageID == m_currentPage.m_currentPageID)
      m_currentPage.m_backgroundPageID = MINUS_ONE;
    if (m_isBackgroundPage)
      m_pages.addBackgroundPage(m_currentPage);
    else
      m_pages.addPage(m_currentPage);
    m_isPageStarted = false;
    m_isBackgroundPage = false;
  }
}

void libvisio::VSDContentCollector::endPages()
{
  m_pages.draw(m_painter);
}

void libvisio::VSDContentCollector::appendCharacters(librevenge::RVNGString &text, const std::vector<unsigned char> &characters, TextFormat format)
{
	if (format == VSD_TEXT_UTF16)
		return appendCharacters(text, characters);
	if (format == VSD_TEXT_UTF8)
	{
		std::string result = string_data_to_utf8(&characters[0], characters.size(), CP_UTF8);
		text.append(result.c_str());
		return;
	}

	std::string result;
	result = string_data_to_utf8(&characters[0], characters.size(), 1250);
	text.append(result.c_str());

	// do we need this?
	/*
	if (0x1e == ucs4Character)
		appendUCS4(text, 0xfffc);
	else
		appendUCS4(text, ucs4Character);
	*/

	return;

 // other options ( possible values of "format" variable)
 //
 //	  VSD_TEXT_SYMBOL
 //   case VSD_TEXT_JAPANESE: ==>				"windows-932"
 //   case VSD_TEXT_KOREAN: ==>				"windows-949"
 //   case VSD_TEXT_CHINESE_SIMPLIFIED: ==>	"windows-936"
 //   case VSD_TEXT_CHINESE_TRADITIONAL: ==>	"windows-950"
 //   case VSD_TEXT_GREEK: ==>				"windows-1253"
 //   case VSD_TEXT_TURKISH: == >				"windows-1254"
 //   case VSD_TEXT_VIETNAMESE: ==>			"windows-1258"
 //   case VSD_TEXT_HEBREW: ==>				"windows-1255"
 //   case VSD_TEXT_ARABIC: ==>				"windows-1256"
 //   case VSD_TEXT_BALTIC: ==>				"windows-1257"
 //   case VSD_TEXT_RUSSIAN: ==>				"windows-1250"
 //   case VSD_TEXT_THAI: ==>					"windows-874"
 //   case VSD_TEXT_CENTRAL_EUROPE: ==>		"windows-1250"
 //   default: ==>							"windows-1250"
  
}

void libvisio::VSDContentCollector::appendCharacters(librevenge::RVNGString &text, const std::vector<unsigned char> &characters)
{
	std::string result = string_data_to_utf8(&characters[0], characters.size(), CP_UNICODE);
	text.append(result.c_str());
}

void libvisio::VSDContentCollector::_appendField(librevenge::RVNGString &text)
{
  if (m_fieldIndex < m_fields.size())
    text.append(m_fields[m_fieldIndex++].cstr());
  else
    m_fieldIndex++;
}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
