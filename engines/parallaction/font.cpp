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

#include "common/endian.h"
#include "common/stream.h"

#include "parallaction/parallaction.h"

namespace Parallaction {


extern byte _amigaTopazFont[];

class BraFont : public Font {

protected:
	byte	*_cp;
	uint	_bufPitch;

	uint32	_height;
	byte	_numGlyphs;

	byte	*_widths;
	uint	*_offsets;

	byte	*_data;

	static  byte _charMap[];

	byte mapChar(byte c) {
		return _charMap[c];
	}

public:
	BraFont(Common::ReadStream &stream) {

		_numGlyphs = stream.readByte();
		_height = stream.readUint32BE();

		_widths = (byte*)malloc(_numGlyphs);
		stream.read(_widths, _numGlyphs);

		_offsets = (uint*)malloc(_numGlyphs * sizeof(uint));
		_offsets[0] = 0;
		for (uint i = 1; i < _numGlyphs; i++)
			_offsets[i] = _offsets[i-1] + _widths[i-1] * _height;

		uint size = _offsets[_numGlyphs-1] + _widths[_numGlyphs-1] * _height;

		_data = (byte*)malloc(size);
		stream.read(_data, size);

	}

	~BraFont() {
		free(_widths);
		free(_offsets);
		free(_data);
	}


	uint32 getStringWidth(const char *s) {
		uint32 len = 0;

		while (*s) {
			byte c = mapChar(*s);
			len += (_widths[c] + 2);
			s++;
		}

		return len;
	}

	uint16 height() {
		return (uint16)_height;
	}

	uint16 drawChar(unsigned char c) {
		assert(c < _numGlyphs);

		byte *src = _data + _offsets[c];
		byte *dst = _cp;
		uint16 w = _widths[c];

		for (uint16 j = 0; j < height(); j++) {
			for (uint16 k = 0; k < w; k++) {

				if (*src) {
					*dst = (_color) ? _color : *src;
				}

				dst++;
				src++;
			}

			dst += (_bufPitch - w);
		}

		return w + 2;

	}

	void drawString(byte* buffer, uint32 pitch, const char *s) {
		if (s == NULL)
			return;

		_bufPitch = pitch;

		_cp = buffer;
		while (*s) {
			byte c = mapChar(*s);
			_cp += drawChar(c);
			s++;
		}
	}

};

byte BraFont::_charMap[] = {
// 0
	0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
// 1
	0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
// 2
	0x34, 0x49, 0x48, 0x34, 0x34, 0x34, 0x34, 0x47, 0x34, 0x34, 0x34, 0x34, 0x40, 0x34, 0x3F, 0x34,
// 3
	0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x46, 0x45, 0x34, 0x34, 0x34, 0x42,
// 4
	0x34, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
// 5
	0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x34, 0x34, 0x34, 0x34, 0x34,
// 6
	0x34, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
// 7
	0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x34, 0x34, 0x34, 0x34,
// 8
	0x5E, 0x5D, 0x4E, 0x4B, 0x4D, 0x4C, 0x34, 0x5E, 0x4F, 0x51, 0x50, 0x34, 0x34, 0x34, 0x34, 0x34,
// 9
	0x34, 0x34, 0x34, 0x57, 0x59, 0x58, 0x5B, 0x5C, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
// A
	0x4A, 0x52, 0x34, 0x5A, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
// B
	0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
// C
	0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
// D
	0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
// E
	0x34, 0x5F, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
// F
	0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34
};


class BraInventoryObjects : public BraFont, public Frames {

public:
	BraInventoryObjects(Common::ReadStream &stream) : BraFont(stream) {
	}

	// Frames implementation
	uint16	getNum() {
		return _numGlyphs;
	}

	byte*	getData(uint16 index) {
		assert(index < _numGlyphs);
		return _data + _height * index + _widths[index];
	}

	void	getRect(uint16 index, Common::Rect &r) {
		assert(index < _numGlyphs);
		r.left = 0;
		r.top = 0;
		r.setWidth(_widths[index]);
		r.setHeight(_height);
	}

	uint	getRawSize(uint16 index) {
		assert(index < _numGlyphs);
		return _widths[index] * _height;
	}

	uint	getSize(uint16 index) {
		assert(index < _numGlyphs);
		return _widths[index] * _height;
	}

};

class DosFont : public Font {

protected:
	// drawing properties
	byte		*_cp;

	Cnv			*_data;
	byte		_pitch;
	uint32		_bufPitch;

protected:
	virtual uint16 drawChar(char c) = 0;
	virtual uint16 width(byte c) = 0;
	virtual uint16 height() = 0;

	byte mapChar(byte c) {
		if (c == 0xA5) return 0x5F;
		if (c == 0xDF) return 0x60;

		if (c > 0x7F) return c - 0x7F;

		return c - 0x20;
	}

public:
	DosFont(Cnv *cnv) : _data(cnv), _pitch(cnv->_width) {
	}

	~DosFont() {
		delete _data;
	}

	void setData() {

	}

	uint32 getStringWidth(const char *s) {
		uint32 len = 0;

		while (*s) {
			byte c = mapChar(*s);
			len += width(c);
			s++;
		}

		return len;
	}

	void drawString(byte* buffer, uint32 pitch, const char *s) {
		if (s == NULL)
			return;

		_bufPitch = pitch;

		_cp = buffer;
		while (*s) {
			byte c = mapChar(*s);
			_cp += drawChar(c);
			s++;
		}
	}
};

class DosDialogueFont : public DosFont {

private:
	static const byte	_glyphWidths[126];

protected:
	uint16 width(byte c) {
		return _glyphWidths[c];
	}

	uint16 height() {
		return _data->_height;
	}

public:
	DosDialogueFont(Cnv *cnv) : DosFont(cnv) {
	}

protected:
	uint16 drawChar(char c) {

		byte *src = _data->getFramePtr(c);
		byte *dst = _cp;
		uint16 w = width(c);

		for (uint16 j = 0; j < height(); j++) {
			for (uint16 k = 0; k < w; k++) {

				if (!*src)
					*dst = _color;

				dst++;
				src++;
			}

			src += (_pitch - w);
			dst += (_bufPitch - w);
		}

		return w;

	}

};

const byte DosDialogueFont::_glyphWidths[126] = {
  0x04, 0x03, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x04, 0x04, 0x06, 0x06, 0x03, 0x05, 0x03, 0x05,
  0x06, 0x06, 0x06, 0x06, 0x07, 0x06, 0x06, 0x06, 0x06, 0x06, 0x03, 0x03, 0x05, 0x04, 0x05, 0x05,
  0x03, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x08, 0x07, 0x07, 0x07, 0x05, 0x06, 0x05, 0x08, 0x07,
  0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x03, 0x04, 0x05, 0x05, 0x06, 0x06, 0x05,
  0x05, 0x06, 0x05, 0x05, 0x05, 0x05, 0x06, 0x07, 0x05, 0x05, 0x05, 0x05, 0x02, 0x05, 0x05, 0x07,
  0x08, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x04, 0x04, 0x04,
  0x05, 0x06, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x04, 0x06, 0x05, 0x05, 0x05, 0x05
};


class DosMonospacedFont : public DosFont {

protected:
	uint16	_width;

protected:
	uint16 width(byte c) {
		return _width;
	}

	uint16 height() {
		return _data->_height;
	}

	uint16 drawChar(char c) {

		byte *src = _data->getFramePtr(c);
		byte *dst = _cp;

		for (uint16 i = 0; i < height(); i++) {
			for (uint16 j = 0; j < _width; j++) {
				if (*src)
					*dst = *src;
				src++;
				dst++;
			}

			dst += (_bufPitch - _width);
			src += (_pitch - _width);
		}

		return _width;
	}

public:
	DosMonospacedFont(Cnv *cnv) : DosFont(cnv) {
		_width = 8;
	}

};



class AmigaFont : public Font {

#include "common/pack-start.h"
	struct CharLoc {
		uint16	_offset;
		uint16	_length;
	};

	struct AmigaDiskFont {
		uint16	_ySize;
		byte	_style;
		byte	_flags;
		uint16	_xSize;
		uint16	_baseline;
		uint16	_boldSmear;
		uint16	_accessors;	// unused
		byte	_loChar;
		byte	_hiChar;
		uint32	_charData;
		uint16	_modulo;
		uint32	_charLoc;
		uint32	_charSpace;
		uint32	_charKern;
	};
#include "common/pack-end.h"

	AmigaDiskFont	*_font;
	uint32		_dataSize;
	byte		*_data;
	byte		*_charData;
	CharLoc		*_charLoc;
	uint16		*_charSpace;
	uint16		*_charKern;

	byte			*_cp;
	uint32		_pitch;

protected:
	uint16 getSpacing(byte c);
	void blitData(byte c);
	uint16 getKerning(byte c);
	uint16 getPixels(byte c);
	uint16 getOffset(byte c);
	uint16 width(byte c);
	uint16 height();

	byte	mapChar(byte c);

public:
	AmigaFont(Common::SeekableReadStream &stream);
	~AmigaFont();

	uint32 getStringWidth(const char *s);
	void drawString(byte *buf, uint32 pitch, const char *s);



};

AmigaFont::AmigaFont(Common::SeekableReadStream &stream) {
	stream.seek(32);	// skips dummy header

	_dataSize = stream.size() - stream.pos();
	_data = (byte*)malloc(_dataSize);
	stream.read(_data, _dataSize);

	_font = (AmigaDiskFont*)(_data + 78);
	_font->_ySize = FROM_BE_16(_font->_ySize);
	_font->_xSize = FROM_BE_16(_font->_xSize);
	_font->_baseline = FROM_BE_16(_font->_baseline);
	_font->_modulo = FROM_BE_16(_font->_modulo);

	_charLoc = (CharLoc*)(_data + FROM_BE_32(_font->_charLoc));
	_charData = _data + FROM_BE_32(_font->_charData);

	_charSpace = 0;
	_charKern = 0;

	if (_font->_charSpace != 0)
		_charSpace = (uint16*)(_data + FROM_BE_32(_font->_charSpace));
	if (_font->_charKern != 0)
		_charKern = (uint16*)(_data + FROM_BE_32(_font->_charKern));

}

AmigaFont::~AmigaFont() {
	free(_data);
}

uint16 AmigaFont::getSpacing(byte c) {
	return (_charSpace == 0) ? _font->_xSize : FROM_BE_16(_charSpace[c]);
}

uint16 AmigaFont::getKerning(byte c) {
	return (_charKern == 0) ? 0 : FROM_BE_16(_charKern[c]);
}

uint16 AmigaFont::getPixels(byte c) {
	return FROM_BE_16(_charLoc[c]._length);
}

uint16 AmigaFont::getOffset(byte c) {
	return FROM_BE_16(_charLoc[c]._offset);
}

void AmigaFont::blitData(byte c) {

	int num = getPixels(c);
	int bitOffset = getOffset(c);

	byte *d = _cp;
	byte *s = _charData;

	for (int i = 0; i < _font->_ySize; i++) {

		for (int j = bitOffset; j < bitOffset + num; j++) {
			byte *b = s + (j >> 3);
			byte bit = *b & (0x80 >> (j & 7));

			if (bit)
				*d = _color;

			d++;
		}

		s += _font->_modulo;
		d += _pitch - num;
	}

}

uint16 AmigaFont::width(byte c) {
//	printf("kern(%i) = %i, space(%i) = %i\t", c, getKerning(c), c, getSpacing(c));
	return getKerning(c) + getSpacing(c);
}

uint16 AmigaFont::height() {
	return _font->_ySize;
}

byte AmigaFont::mapChar(byte c) {

	if (c < _font->_loChar || c > _font->_hiChar)
		error("character '%c (%x)' not supported by font", c, c);

	return c - _font->_loChar;
}

uint32 AmigaFont::getStringWidth(const char *s) {
	uint32 len = 0;

	while (*s) {
		byte c = mapChar(*s);
		len += width(c);
		s++;
	}

	return len;
}

void AmigaFont::drawString(byte *buffer, uint32 pitch, const char *s) {

	_cp = buffer;
	_pitch = pitch;

	byte c;

	while (*s) {
		c = mapChar(*s);
		_cp += getKerning(c);
		blitData(c);
		_cp += getSpacing(c);
		s++;
	}

}

Font *DosDisk_ns::createFont(const char *name, Cnv* cnv) {
	Font *f = 0;

	if (!scumm_stricmp(name, "comic"))
		f = new DosDialogueFont(cnv);
	else
	if (!scumm_stricmp(name, "topaz"))
		f = new DosMonospacedFont(cnv);
	else
	if (!scumm_stricmp(name, "slide"))
		f = new DosMonospacedFont(cnv);
	else
		error("unknown dos font '%s'", name);

	return f;
}

Font *AmigaDisk_ns::createFont(const char *name, Common::SeekableReadStream &stream) {
	// TODO: implement AmigaLabelFont for labels
	return new AmigaFont(stream);
}

Font *DosDisk_br::createFont(const char *name, Common::ReadStream &stream) {
//	printf("DosDisk_br::createFont(%s)\n", name);
	return new BraFont(stream);
}

Font *AmigaDisk_br::createFont(const char *name, Common::SeekableReadStream &stream) {
	// TODO: implement AmigaLabelFont for labels
	return new AmigaFont(stream);
}

GfxObj* DosDisk_br::createInventoryObjects(Common::SeekableReadStream &stream) {
	Frames *frames = new BraInventoryObjects(stream);
	return new GfxObj(0, frames, "inventoryobjects");
}


void Parallaction_ns::initFonts() {

	if (getPlatform() == Common::kPlatformPC) {
		_dialogueFont = _disk->loadFont("comic");
		_labelFont = _disk->loadFont("topaz");
		_menuFont = _disk->loadFont("slide");
		_introFont = _disk->loadFont("slide");
	} else {
		_dialogueFont = _disk->loadFont("comic");
		Common::MemoryReadStream stream(_amigaTopazFont, 2600, false);
		_labelFont = new AmigaFont(stream);
		_menuFont = _disk->loadFont("slide");
		_introFont = _disk->loadFont("intro");
	}

}

void Parallaction_br::initFonts() {
	if (getPlatform() == Common::kPlatformPC) {
		_menuFont = _disk->loadFont("russia");
		_dialogueFont = _disk->loadFont("comic");
		_labelFont = _menuFont;
	} else {
		// TODO: Confirm fonts matches
		// fonts/natasha/16
		// fonts/sonya/18
		// fonts/vanya/16

		_menuFont = _disk->loadFont("fonts/natasha/16");
		_dialogueFont = _disk->loadFont("fonts/sonya/18");
		Common::MemoryReadStream stream(_amigaTopazFont, 2600, false);
		_labelFont = new AmigaFont(stream);
	}
}

}
