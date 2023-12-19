/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libvisio project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "VisioDocument.h"

#include "librevenge.h"
#include "libvisio_utils.h"
#include "VSDParser.h"

#include <algorithm>
#include <memory>
#include <string>

namespace
{

	static bool checkVisioMagic(librevenge::RVNGInputStream *input)
	{
		const unsigned char magic[] =
		{
		  0x56, 0x69, 0x73, 0x69, 0x6f, 0x20, 0x28, 0x54, 0x4d, 0x29,
		  0x20, 0x44, 0x72, 0x61, 0x77, 0x69, 0x6e, 0x67, 0x0d, 0x0a,
		  0x0
		};
		auto startPosition = (int)input->tell();
		unsigned long numBytesRead = 0;
		const unsigned char *buffer = input->read(VSD_NUM_ELEMENTS(magic), numBytesRead);
		const bool returnValue = VSD_NUM_ELEMENTS(magic) == numBytesRead
			&& std::equal(magic, magic + VSD_NUM_ELEMENTS(magic), buffer);
		input->seek(startPosition, librevenge::RVNG_SEEK_SET);
		return returnValue;
	}

	static bool isBinaryVisioDocument(librevenge::RVNGInputStream *input) try
	{
		std::shared_ptr<librevenge::RVNGInputStream> docStream;
		input->seek(0, librevenge::RVNG_SEEK_SET);
		if (input->isStructured())
		{
			input->seek(0, librevenge::RVNG_SEEK_SET);
			docStream.reset(input->getSubStreamByName("VisioDocument"));
		}
		if (!docStream)
			docStream.reset(input, libvisio::VSDDummyDeleter());

		docStream->seek(0, librevenge::RVNG_SEEK_SET);
		unsigned char version = 0;
		if (checkVisioMagic(docStream.get()))
		{
			docStream->seek(0x1A, librevenge::RVNG_SEEK_SET);
			version = libvisio::readU8(docStream.get());
		}
		input->seek(0, librevenge::RVNG_SEEK_SET);

		// Versions 2k (6) and 2k3 (11)
		return ( version == 11 );
	}
	catch (...)
	{
		return false;
	}

	static bool parseBinaryVisioDocument(librevenge::RVNGInputStream *input, librevenge::RVNGDrawingInterface *painter) try
	{
		input->seek(0, librevenge::RVNG_SEEK_SET);
		std::shared_ptr<librevenge::RVNGInputStream> docStream;

		if (input->isStructured())
			docStream.reset(input->getSubStreamByName("VisioDocument"));
		if (!docStream)
			docStream.reset(input, libvisio::VSDDummyDeleter());

		docStream->seek(0x1A, librevenge::RVNG_SEEK_SET);

		std::unique_ptr<libvisio::VSDParser> parser;

		unsigned char version = libvisio::readU8(docStream.get());

		if (version == 11) {
			parser.reset(new libvisio::VSDParser(docStream.get(), painter, input));
			return parser->parseMain();
		}
		
		return false;
	}
	catch (...)
	{
		return false;
	}

} // anonymous namespace

	/**
	Analyzes the content of an input stream to see if it can be parsed
	\param input The input stream
	\return A value that indicates whether the content from the input
	stream is a Visio Document that libvisio able to parse
	*/
	bool libvisio::VisioDocument::isSupported(librevenge::RVNGInputStream *input)
	{
		if (!input)
			return false;

		if (isBinaryVisioDocument(input))
			return true;

		return false;
	}


	/**
	Parses the input stream content. It will make callbacks to the functions provided by a
	librevenge::RVNGDrawingInterface class implementation when needed. This is often commonly called the
	'main parsing routine'.
	\param input The input stream
	\param painter A WPGPainterInterface implementation
	\return A value that indicates whether the parsing was successful
	*/
	bool libvisio::VisioDocument::parse(librevenge::RVNGInputStream *input, librevenge::RVNGDrawingInterface *painter)
	{
		if (!input || !painter)
			return false;

		if (isBinaryVisioDocument(input))
		{
			if (parseBinaryVisioDocument(input, painter))
				return true;

			return false;
		}

		return false;
	}
