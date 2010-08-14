/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL$
 * $Id$
 *
 */

/*
 * This code is based on Broken Sword 2.5 engine
 *
 * Copyright (c) Malte Thiesen, Daniel Queteschiner and Michael Elsdoerfer
 *
 * Licensed under GNU GPL v2
 *
 */

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include "sword25/package/packagemanager.h"
#include "sword25/gfx/image/imageloader.h"
#include "sword25/gfx/opengl/openglgfx.h"
#include "sword25/gfx/opengl/glimage.h"

namespace Sword25 {

#define BS_LOG_PREFIX "GLIMAGE"

// -----------------------------------------------------------------------------
// CONSTRUCTION / DESTRUCTION
// -----------------------------------------------------------------------------

BS_GLImage::BS_GLImage(const Common::String &Filename, bool &Result) :
	_data(0),
	m_Width(0),
	m_Height(0) {
	Result = false;

	BS_PackageManager *pPackage = static_cast<BS_PackageManager *>(BS_Kernel::GetInstance()->GetService("package"));
	BS_ASSERT(pPackage);

	_backSurface = (static_cast<BS_GraphicEngine *>(BS_Kernel::GetInstance()->GetService("gfx")))->getSurface();

	// Datei laden
	char *pFileData;
	unsigned int FileSize;
	if (!(pFileData = (char *) pPackage->GetFile(Filename, &FileSize))) {
		BS_LOG_ERRORLN("File \"%s\" could not be loaded.", Filename.c_str());
		return;
	}

	// Bildeigenschaften bestimmen
	BS_GraphicEngine::COLOR_FORMATS ColorFormat;
	int Pitch;
	if (!BS_ImageLoader::ExtractImageProperties(pFileData, FileSize, ColorFormat, m_Width, m_Height)) {
		BS_LOG_ERRORLN("Could not read image properties.");
		return;
	}

	// Das Bild dekomprimieren
	if (!BS_ImageLoader::LoadImage(pFileData, FileSize, BS_GraphicEngine::CF_ABGR32, _data, m_Width, m_Height, Pitch)) {
		BS_LOG_ERRORLN("Could not decode image.");
		return;
	}

	// Dateidaten freigeben
	delete[] pFileData;

	Result = true;
	return;
}

// -----------------------------------------------------------------------------

BS_GLImage::BS_GLImage(unsigned int Width, unsigned int Height, bool &Result) :
	m_Width(Width),
	m_Height(Height) {
	Result = false;

	_data = new byte[Width * Height * 4];

	Result = true;
	return;
}

// -----------------------------------------------------------------------------

BS_GLImage::~BS_GLImage() {
	delete[] _data;
}

// -----------------------------------------------------------------------------

bool BS_GLImage::Fill(const BS_Rect *pFillRect, unsigned int Color) {
	BS_LOG_ERRORLN("Fill() is not supported.");
	return false;
}

// -----------------------------------------------------------------------------

bool BS_GLImage::SetContent(const byte *Pixeldata, uint size, unsigned int Offset, unsigned int Stride) {
	// �berpr�fen, ob PixelData ausreichend viele Pixel enth�lt um ein Bild der Gr��e Width * Height zu erzeugen
	if (size < static_cast<unsigned int>(m_Width * m_Height * 4)) {
		BS_LOG_ERRORLN("PixelData vector is too small to define a 32 bit %dx%d image.", m_Width, m_Height);
		return false;
	}

	const byte *in = &Pixeldata[Offset];
	byte *out = _data;

	for (int i = 0; i < m_Height; i++) {
		memcpy(out, in, m_Width * 4);
		out += m_Width * 4;
		in += Stride;
	}

	return true;
}

// -----------------------------------------------------------------------------

unsigned int BS_GLImage::GetPixel(int X, int Y) {
	BS_LOG_ERRORLN("GetPixel() is not supported. Returning black.");
	return 0;
}

// -----------------------------------------------------------------------------

bool BS_GLImage::Blit(int PosX, int PosY,
                      int Flipping,
                      BS_Rect *pPartRect,
                      unsigned int Color,
                      int Width, int Height) {
	int x1 = 0, y1 = 0;
	int w = m_Width, h = m_Height;
	if (pPartRect) {
		x1 = pPartRect->left;
		y1 = pPartRect->top;
		w = pPartRect->right - pPartRect->left;
		h = pPartRect->bottom - pPartRect->top;
	}

	// Skalierungen berechnen
	float ScaleX, ScaleY;
	if (Width == -1)
		Width = m_Width;
	ScaleX = (float) Width / (float) m_Width;

	if (Height == -1)
		Height = m_Height;
	ScaleY = (float) Height / (float) m_Height;

	if (Color != 0xffffffff) {
		warning("STUB: Image bg color: %x", Color);
	}

	if (ScaleX != 1.0 || ScaleY != 1.0) {
		warning("STUB: Sprite scaling (%f x %f)", ScaleX, ScaleY);
	}

	if (Flipping & (BS_Image::FLIP_V | BS_Image::FLIP_H)) {
		warning("STUB: Sprite flipping");
	}

	w = CLIP(x1 + w, 0, (int)_backSurface->w);
	h = CLIP(y1 + h, 0, (int)_backSurface->h);

	// Rendern
	// TODO:
	// Die Bedeutung von FLIP_V und FLIP_H ist vertauscht. Allerdings glaubt der Rest der Engine auch daran, daher war es einfacher diesen Fehler
	// weiterzuf�hren. Bei Gelegenheit ist dieses aber zu �ndern.

	// TODO: scaling
	// TODO: Flipping
	byte *ino = &_data[y1 * m_Width * 4 + x1 * 4];
	byte *outo = (byte *)_backSurface->getBasePtr(PosX, PosY);
	byte *in, *out;
	bool alphawarn = false;

	for (int i = 0; i < h; i++) {
		out = outo;
		in = ino;
		for (int j = 0; j < w; j++) {
			if (*in == 0) {
				in += 4;
				out += 4;
				continue;
			}

			if (*in != 255)
				alphawarn = true;

			*in++ = *out++; // TODO: alpha blending
			*in++ = *out++;
			*in++ = *out++;
			*in++ = *out++;
		}
		outo += _backSurface->pitch;
		ino += m_Width * 4;
	}

	if (alphawarn)
		warning("STUB: alpha image");

	return true;
}

} // End of namespace Sword25