/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* libvisio
 * Version: MPL 1.1 / GPLv2+ / LGPLv2+
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License or as specified alternatively below. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Major Contributor(s):
 * Copyright (C) 2011 Fridrich Strba <fridrich.strba@bluewin.ch>
 * Copyright (C) 2011 Eilidh McAdam <tibbylickle@gmail.com>
 *
 *
 * All Rights Reserved.
 *
 * For minor contributions see the git repository.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPLv2+"), or
 * the GNU Lesser General Public License Version 2 or later (the "LGPLv2+"),
 * in which case the provisions of the GPLv2+ or the LGPLv2+ are applicable
 * instead of those above.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stack>

#include <librevenge-stream/librevenge-stream.h>
#include <librevenge/librevenge.h>
#include <libvisio/libvisio.h>

enum PainterCallback
{
  PC_START_GRAPHICS = 0,
  PC_START_LAYER,
  PC_START_EMBEDDED_GRAPHICS,
  PC_START_TEXT_OBJECT,
  PC_START_TEXT_LINE,
  PC_START_TEXT_SPAN
};

#ifdef _U
#undef _U
#endif

#define _U(M, L) \
	if (!m_printCallgraphScore) \
			__iuprintf M; \
	else \
		m_callStack.push(L);

#ifdef _D
#undef _D
#endif

#define _D(M, L) \
	if (!m_printCallgraphScore) \
			__idprintf M; \
	else \
	{ \
		PainterCallback lc = m_callStack.top(); \
		if (lc != L) \
			m_callbackMisses++; \
		m_callStack.pop(); \
	}

class RawPainter : public RVNGDrawingInterface
{
public:
  RawPainter(bool printCallgraphScore);

  ~RawPainter();

  void startDocument(const ::RVNGPropertyList & /*propList*/) {}
  void endDocument() {}
  void setDocumentMetaData(const RVNGPropertyList & /*propList*/) {}
  void startPage(const ::RVNGPropertyList &propList);
  void endPage();
  void startLayer(const ::RVNGPropertyList &propList);
  void endLayer();
  void startEmbeddedGraphics(const ::RVNGPropertyList &propList);
  void endEmbeddedGraphics();

  void setStyle(const ::RVNGPropertyList &propList, const ::RVNGPropertyListVector &gradient);

  void drawRectangle(const ::RVNGPropertyList &propList);
  void drawEllipse(const ::RVNGPropertyList &propList);
  void drawPolyline(const ::RVNGPropertyListVector &vertices);
  void drawPolygon(const ::RVNGPropertyListVector &vertices);
  void drawPath(const ::RVNGPropertyListVector &path);
  void drawGraphicObject(const ::RVNGPropertyList &propList, const ::RVNGBinaryData &binaryData);
  void startTextObject(const ::RVNGPropertyList &propList, const ::RVNGPropertyListVector &path);
  void endTextObject();


  void openOrderedListLevel(const RVNGPropertyList & /*propList*/) {}
  void closeOrderedListLevel() {}

  void openUnorderedListLevel(const RVNGPropertyList & /*propList*/) {}
  void closeUnorderedListLevel() {}

  void openListElement(const RVNGPropertyList & /*propList*/, const RVNGPropertyListVector & /* tabStops */) {}
  void closeListElement() {}

  void openParagraph(const RVNGPropertyList &propList, const RVNGPropertyListVector &tabStops);
  void closeParagraph();

  void openSpan(const RVNGPropertyList &propList);
  void closeSpan();

  void insertTab() {}
  void insertSpace() {}
  void insertText(const RVNGString &text);
  void insertLineBreak() {}
  void insertField(const RVNGString & /* type */, const RVNGPropertyList & /*propList*/) {}


private:
  int m_indent;
  int m_callbackMisses;
  bool m_printCallgraphScore;
  std::stack<PainterCallback> m_callStack;

  void __indentUp()
  {
    m_indent++;
  }
  void __indentDown()
  {
    if (m_indent > 0) m_indent--;
  }

  void __iprintf(const char *format, ...);
  void __iuprintf(const char *format, ...);
  void __idprintf(const char *format, ...);
};

RVNGString getPropString(const RVNGPropertyList &propList)
{
  RVNGString propString;
  RVNGPropertyList::Iter i(propList);
  if (!i.last())
  {
    propString.append(i.key());
    propString.append(": ");
    propString.append(i()->getStr().cstr());
    for (; i.next(); )
    {
      propString.append(", ");
      propString.append(i.key());
      propString.append(": ");
      propString.append(i()->getStr().cstr());
    }
  }

  return propString;
}

RVNGString getPropString(const RVNGPropertyListVector &itemList)
{
  RVNGString propString;

  propString.append("(");
  RVNGPropertyListVector::Iter i(itemList);

  if (!i.last())
  {
    propString.append("(");
    propString.append(getPropString(i()));
    propString.append(")");

    for (; i.next();)
    {
      propString.append(", (");
      propString.append(getPropString(i()));
      propString.append(")");
    }

  }
  propString.append(")");

  return propString;
}

RawPainter::RawPainter(bool printCallgraphScore):
  RVNGDrawingInterface(),
  m_indent(0),
  m_callbackMisses(0),
  m_printCallgraphScore(printCallgraphScore),
  m_callStack()
{
}

RawPainter::~RawPainter()
{
  if (m_printCallgraphScore)
    printf("%d\n", (int)(m_callStack.size() + m_callbackMisses));
}

void RawPainter::__iprintf(const char *format, ...)
{
  if (m_printCallgraphScore) return;

  va_list args;
  va_start(args, format);
  for (int i=0; i<m_indent; i++)
    printf("  ");
  vprintf(format, args);
  va_end(args);
}

void RawPainter::__iuprintf(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  for (int i=0; i<m_indent; i++)
    printf("  ");
  vprintf(format, args);
  __indentUp();
  va_end(args);
}

void RawPainter::__idprintf(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  __indentDown();
  for (int i=0; i<m_indent; i++)
    printf("  ");
  vprintf(format, args);
  va_end(args);
}

void RawPainter::startPage(const ::RVNGPropertyList &propList)
{
  _U(("RawPainter::startPage(%s)\n", getPropString(propList).cstr()), PC_START_GRAPHICS);
}

void RawPainter::endPage()
{
  _D(("RawPainter::endPage\n"), PC_START_GRAPHICS);
}

void RawPainter::startLayer(const ::RVNGPropertyList &propList)
{
  _U(("RawPainter::startLayer (%s)\n", getPropString(propList).cstr()), PC_START_LAYER);
}

void RawPainter::endLayer()
{
  _D(("RawPainter::endLayer\n"), PC_START_LAYER);
}

void RawPainter::startEmbeddedGraphics(const ::RVNGPropertyList &propList)
{
  _U(("RawPainter::startEmbeddedGraphics (%s)\n", getPropString(propList).cstr()), PC_START_EMBEDDED_GRAPHICS);
}

void RawPainter::endEmbeddedGraphics()
{
  _D(("RawPainter::endEmbeddedGraphics \n"), PC_START_EMBEDDED_GRAPHICS);
}

void RawPainter::setStyle(const ::RVNGPropertyList &propList, const ::RVNGPropertyListVector &gradient)
{
  if (m_printCallgraphScore)
    return;

  __iprintf("RawPainter::setStyle(%s, gradient: (%s))\n", getPropString(propList).cstr(), getPropString(gradient).cstr());
}

void RawPainter::drawRectangle(const ::RVNGPropertyList &propList)
{
  if (m_printCallgraphScore)
    return;

  __iprintf("RawPainter::drawRectangle (%s)\n", getPropString(propList).cstr());
}

void RawPainter::drawEllipse(const ::RVNGPropertyList &propList)
{
  if (m_printCallgraphScore)
    return;

  __iprintf("RawPainter::drawEllipse (%s)\n", getPropString(propList).cstr());
}

void RawPainter::drawPolyline(const ::RVNGPropertyListVector &vertices)
{
  if (m_printCallgraphScore)
    return;

  __iprintf("RawPainter::drawPolyline (%s)\n", getPropString(vertices).cstr());
}

void RawPainter::drawPolygon(const ::RVNGPropertyListVector &vertices)
{
  if (m_printCallgraphScore)
    return;

  __iprintf("RawPainter::drawPolygon (%s)\n", getPropString(vertices).cstr());
}

void RawPainter::drawPath(const ::RVNGPropertyListVector &path)
{
  if (m_printCallgraphScore)
    return;

  __iprintf("RawPainter::drawPath (%s)\n", getPropString(path).cstr());
}

void RawPainter::drawGraphicObject(const ::RVNGPropertyList &propList, const ::RVNGBinaryData & /*binaryData*/)
{
  if (m_printCallgraphScore)
    return;

  __iprintf("RawPainter::drawGraphicObject (%s)\n", getPropString(propList).cstr());
}

void RawPainter::startTextObject(const ::RVNGPropertyList &propList, const ::RVNGPropertyListVector &path)
{
  _U(("RawPainter::startTextObject (%s, path: (%s))\n", getPropString(propList).cstr(), getPropString(path).cstr()), PC_START_TEXT_OBJECT);
}

void RawPainter::endTextObject()
{
  _D(("RawPainter::endTextObject\n"), PC_START_TEXT_OBJECT);
}

void RawPainter::openParagraph(const ::RVNGPropertyList &propList, const ::RVNGPropertyListVector &tabStops)
{
  _U(("RawPainter::openParagraph (%s, tabStops: (%s))\n", getPropString(propList).cstr(), getPropString(tabStops).cstr()), PC_START_TEXT_LINE);
}

void RawPainter::closeParagraph()
{
  _D(("RawPainter::closeParagraph\n"), PC_START_TEXT_LINE);
}

void RawPainter::openSpan(const ::RVNGPropertyList &propList)
{
  _U(("RawPainter::openSpan (%s)\n", getPropString(propList).cstr()), PC_START_TEXT_SPAN);
}

void RawPainter::closeSpan()
{
  _D(("RawPainter::closeSpan\n"), PC_START_TEXT_SPAN);
}

void RawPainter::insertText(const ::RVNGString &str)
{
  if (m_printCallgraphScore)
    return;

  __iprintf("RawPainter::insertText (%s)\n", str.cstr());
}


namespace
{

int printUsage()
{
  printf("Usage: vsd2raw [OPTION] <Visio Stencils File>\n");
  printf("\n");
  printf("Options:\n");
  printf("--callgraph           Display the call graph nesting level\n");
  printf("--help                Shows this help message\n");
  return -1;
}

} // anonymous namespace

int main(int argc, char *argv[])
{
  bool printIndentLevel = false;
  char *file = 0;

  if (argc < 2)
    return printUsage();

  for (int i = 1; i < argc; i++)
  {
    if (!strcmp(argv[i], "--callgraph"))
      printIndentLevel = true;
    else if (!file && strncmp(argv[i], "--", 2))
      file = argv[i];
    else
      return printUsage();
  }

  if (!file)
    return printUsage();

  RVNGFileStream input(file);

  if (!libvisio::VisioDocument::isSupported(&input))
  {
    fprintf(stderr, "ERROR: Unsupported file format (unsupported version) or file is encrypted!\n");
    return 1;
  }

  RawPainter painter(printIndentLevel);
  if (!libvisio::VisioDocument::parseStencils(&input, &painter))
  {
    fprintf(stderr, "ERROR: Parsing of document failed!\n");
    return 1;
  }

  return 0;
}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
