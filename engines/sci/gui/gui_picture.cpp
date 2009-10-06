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

#include "sci/sci.h"
#include "sci/engine/state.h"
#include "sci/tools.h"
#include "sci/gui/gui_screen.h"
#include "sci/gui/gui_palette.h"
#include "sci/gui/gui_gfx.h"
#include "sci/gui/gui_picture.h"

namespace Sci {

SciGuiPicture::SciGuiPicture(EngineState *state, SciGuiGfx *gfx, SciGuiScreen *screen, SciGuiPalette *palette, GuiResourceId resourceId)
	: _s(state), _gfx(gfx), _screen(screen), _palette(palette), _resourceId(resourceId) {
	assert(resourceId != -1);
	initData(resourceId);
}

SciGuiPicture::~SciGuiPicture() {
}

void SciGuiPicture::initData(GuiResourceId resourceId) {
	_resource = _s->resMan->findResource(ResourceId(kResourceTypePic, resourceId), false);
	if (!_resource) {
		error("picture resource %d not found", resourceId);
	}
}

GuiResourceId SciGuiPicture::getResourceId() {
	return _resourceId;
}

void SciGuiPicture::draw(uint16 style, bool addToFlag, int16 EGApaletteNo) {
	_style = style;
	_addToFlag = addToFlag;
	_EGApaletteNo = EGApaletteNo;
	_priority = 0;

	if (READ_LE_UINT16(_resource->data) == 0x26) {
		// SCI 1.1 VGA picture
		drawSci11Vga();
	} else {
		// EGA or Amiga vector data
		drawVectorData(_resource->data, _resource->size);
	}
}

void SciGuiPicture::reset() {
	int16 x, y;
	for (y = _gfx->GetPort()->top; y < _screen->_height; y++) {
		for (x = 0; x < _screen->_width; x++) {
			_screen->putPixel(x, y, SCI_SCREEN_MASK_ALL, 255, 0, 0);
		}
	}
}

void SciGuiPicture::drawSci11Vga() {
	byte *inbuffer = _resource->data;
	int size = _resource->size;
	int has_view = READ_LE_UINT16(inbuffer + 4);
	int vector_data_ptr = READ_LE_UINT16(inbuffer + 16);
	int vector_size = size - vector_data_ptr;
	int palette_data_ptr = READ_LE_UINT16(inbuffer + 28);
	int view_data_ptr = READ_LE_UINT16(inbuffer + 32);
	int view_size = palette_data_ptr - view_data_ptr;
	int view_rle_ptr = READ_LE_UINT16(inbuffer + view_data_ptr + 24);
	int view_pixel_ptr = READ_LE_UINT16(inbuffer + view_data_ptr + 28);
	byte *view = NULL;
	GuiPalette palette;

	// Create palette and set it
	_palette->createFromData(inbuffer + palette_data_ptr, &palette);
	_palette->set(&palette, 2);

	// display Cel-data
	if (has_view) {
		view = (byte *)malloc(size*2); // is there a way to know how much decoded view-data there will be?
		if (!view) return;
		memcpy(view, inbuffer + view_data_ptr, 8);
		decodeRLE(inbuffer + view_rle_ptr, inbuffer + view_pixel_ptr, view + 8, view_size - 8);
		drawCel(0, 0, view, size * 2);
		free(view);
	}

	// process vector data
	drawVectorData(inbuffer + vector_data_ptr, vector_size);
}

void SciGuiPicture::decodeRLE(byte *rledata, byte *pixeldata, byte *outbuffer, int size) {
	int pos = 0;
	byte nextbyte;
	byte *rd = rledata;
	byte *ob = outbuffer;
	byte *pd = pixeldata;

	while (pos < size) {
		nextbyte = *(rd++);
		*(ob++) = nextbyte;
		pos ++;
		switch (nextbyte&0xC0) {
		case 0x40 :
		case 0x00 :
			memcpy(ob, pd, nextbyte);
			pd += nextbyte;
			ob += nextbyte;
			pos += nextbyte;
			break;
		case 0xC0 :
			break;
		case 0x80 :
			nextbyte = *(pd++);
			*(ob++) = nextbyte;
			pos ++;
			break;
		}
	}
}

void SciGuiPicture::drawCel(int16 x, int16 y, byte *pdata, int size) {
	byte* pend = pdata + size;
	uint16 width = READ_LE_UINT16(pdata + 0);
	uint16 height = READ_LE_UINT16(pdata + 2);
	signed char dx = *(pdata + 4);
	signed char dy = *(pdata + 5);
	byte priority = _addToFlag ? _priority : 0;
	byte clearColor = *(pdata + 6);
	if (dx || dy || width != 320)
		warning("embedded picture cel has width=%d dx=%d dy=%d", width, dx, dy);
	byte *ptr = pdata + 8; // offset to data
	byte byte, runLength;
	uint16 lasty;

	y += _gfx->GetPort()->top;

	lasty = MIN<int16>(height + y, _gfx->GetPort()->rect.bottom) + _gfx->GetPort()->top;

	switch (_s->resMan->getViewType()) {
	case kViewVga:
	case kViewVga11:
		while (y < lasty && ptr < pend) {
			byte = *ptr++;
			runLength = byte & 0x3F; // bytes run length on this step
			switch (byte & 0xC0) {
			case 0: // copy bytes as-is but skip transparent ones
				while (runLength-- && y < lasty && ptr < pend) {
					if ((byte = *ptr++) != clearColor && priority >= _screen->getPriority(x, y))
						_screen->putPixel(x, y, 3, byte, priority, 0);
					x++;
					if (x >= _screen->_width) {
						x -= _screen->_width; y++;
					}
				}
				break;
			case 0x80: // fill with color
				byte = *ptr++;
				while (runLength-- && y < lasty) {
					if (priority >= _screen->getPriority(x, y)) {
						_screen->putPixel(x, y, 3, byte, priority, 0);
					}
					x++;
					if (x >= _screen->_width) {
						x -= _screen->_width; y++;
					}
				}
				break;
			case 0xC0: // fill with transparent - skip
				x += runLength;
				if (x >= _screen->_width) {
					x -= _screen->_width; y++;
				}
				break;
			}
		}
		break;

	case kViewAmiga:
		while (y < lasty && ptr < pend) {
			byte = *ptr++;
			if (byte & 0x07) {
				runLength = byte & 0x07;
				byte = byte >> 3;
				while (runLength-- && y < lasty) {
					if (priority >= _screen->getPriority(x, y)) {
						_screen->putPixel(x, y, 3, byte, priority, 0);
					}
					x++;
					if (x >= _screen->_width) {
						x -= _screen->_width; y++;
					}
				}
			} else {
				runLength = byte >> 3;
				x += runLength;
				if (x >= _screen->_width) {
					x -= _screen->_width; y++;
				}
			}
		}
		break;

	default:
		error("Unsupported picture viewtype");
	}
}

enum {
	PIC_OP_SET_COLOR = 0xf0,
	PIC_OP_DISABLE_VISUAL = 0xf1,
	PIC_OP_SET_PRIORITY = 0xf2,
	PIC_OP_DISABLE_PRIORITY = 0xf3,
	PIC_OP_SHORT_PATTERNS = 0xf4,
	PIC_OP_MEDIUM_LINES = 0xf5,
	PIC_OP_LONG_LINES = 0xf6,
	PIC_OP_SHORT_LINES = 0xf7,
	PIC_OP_FILL = 0xf8,
	PIC_OP_SET_PATTERN = 0xf9,
	PIC_OP_ABSOLUTE_PATTERN = 0xfa,
	PIC_OP_SET_CONTROL = 0xfb,
	PIC_OP_DISABLE_CONTROL = 0xfc,
	PIC_OP_MEDIUM_PATTERNS = 0xfd,
	PIC_OP_OPX = 0xfe,
	PIC_OP_TERMINATE = 0xff
};
#define PIC_OP_FIRST PIC_OP_SET_COLOR

enum {
	PIC_OPX_EGA_SET_PALETTE_ENTRIES = 0,
	PIC_OPX_EGA_SET_PALETTE = 1,
	PIC_OPX_EGA_MONO0 = 2,
	PIC_OPX_EGA_MONO1 = 3,
	PIC_OPX_EGA_MONO2 = 4,
	PIC_OPX_EGA_MONO3 = 5,
	PIC_OPX_EGA_MONO4 = 6,
	PIC_OPX_EGA_EMBEDDED_VIEW = 7,
	PIC_OPX_EGA_SET_PRIORITY_TABLE = 8
};

enum {
	PIC_OPX_VGA_SET_PALETTE_ENTRIES = 0,
	PIC_OPX_VGA_EMBEDDED_VIEW = 1,
	PIC_OPX_VGA_SET_PALETTE = 2,
	PIC_OPX_VGA_PRIORITY_TABLE_EQDIST = 3,
	PIC_OPX_VGA_PRIORITY_TABLE_EXPLICIT = 4
};

#define PIC_EGAPALETTE_COUNT 4
#define PIC_EGAPALETTE_SIZE  40
#define PIC_EGAPALETTE_TOTALSIZE PIC_EGAPALETTE_COUNT*PIC_EGAPALETTE_SIZE
#define PIC_EGAPRIORITY_SIZE PIC_EGAPALETTE_SIZE

static const byte vector_defaultEGApalette[PIC_EGAPALETTE_SIZE] = {
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x88,
	0x88, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x88,
	0x88, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
	0x08, 0x91, 0x2a, 0x3b, 0x4c, 0x5d, 0x6e, 0x88
};

static const byte vector_defaultEGApriority[PIC_EGAPRIORITY_SIZE] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
};

void SciGuiPicture::drawVectorData(byte *data, int dataSize) {
	byte pic_op;
	byte pic_color = 0, pic_priority = 0x0F, pic_control = 0x0F;
	int16 x = 0, y = 0, oldx, oldy;
	byte EGApalettes[PIC_EGAPALETTE_TOTALSIZE] = {0};
	byte *EGApalette = &EGApalettes[_EGApaletteNo];
	byte EGApriority[PIC_EGAPRIORITY_SIZE] = {0};
	bool isEGA = false;
	int curPos = 0;
	uint16 size;
	byte byte;
	int i;
	GuiPalette palette;
	int16 pattern_Code = 0, pattern_Texture = 0;

	memset(&palette, 0, sizeof(palette));

	if (_EGApaletteNo >= PIC_EGAPALETTE_COUNT)
		_EGApaletteNo = 0;

	if (_s->resMan->getViewType() == kViewEga) {
		isEGA = true;
		// setup default mapping tables
		for (i = 0; i < PIC_EGAPALETTE_TOTALSIZE; i += PIC_EGAPALETTE_SIZE)
			memcpy(&EGApalettes[i], &vector_defaultEGApalette, sizeof(vector_defaultEGApalette));
		memcpy(&EGApriority, &vector_defaultEGApriority, sizeof(vector_defaultEGApriority));
	}

	// Drawing
	while (curPos < dataSize) {
		//warning("%X at %d", data[curPos], curPos);
		switch (pic_op = data[curPos++]) {
		case PIC_OP_SET_COLOR:
			pic_color = data[curPos++];
			if (isEGA) {
				pic_color = EGApalette[pic_color];
				pic_color ^= pic_color << 4;
			}
			break;
		case PIC_OP_DISABLE_VISUAL:
			pic_color = 0xFF;
			break;

		case PIC_OP_SET_PRIORITY:
			pic_priority = data[curPos++];
			if (isEGA) {
				pic_priority = EGApriority[pic_color];
			}
			break;
		case PIC_OP_DISABLE_PRIORITY:
			pic_priority = 255;
			break;

		case PIC_OP_SET_CONTROL:
			pic_control = data[curPos++];
			break;
		case PIC_OP_DISABLE_CONTROL:
			pic_control = 255;
			break;

		case PIC_OP_SHORT_LINES: // short line
			vectorGetAbsCoords(data, curPos, oldx, oldy);
			while (vectorIsNonOpcode(data[curPos])) {
				vectorGetRelCoords(data, curPos, oldx, oldy, x, y);
				_gfx->Draw_Line(oldx, oldy, x, y, pic_color, pic_priority, pic_control);
				oldx = x; oldy = y;
			}
			break;
		case PIC_OP_MEDIUM_LINES: // medium line
			vectorGetAbsCoords(data, curPos, oldx, oldy);
			while (vectorIsNonOpcode(data[curPos])) {
				vectorGetRelCoordsMed(data, curPos, oldx, oldy, x, y);
				_gfx->Draw_Line(oldx, oldy, x, y, pic_color, pic_priority, pic_control);
				oldx = x; oldy = y;
			}
			break;
		case PIC_OP_LONG_LINES: // long line
			vectorGetAbsCoords(data, curPos, oldx, oldy);
			while (vectorIsNonOpcode(data[curPos])) {
				vectorGetAbsCoords(data, curPos, x, y);
				_gfx->Draw_Line(oldx, oldy, x, y, pic_color, pic_priority, pic_control);
				oldx = x; oldy = y;
			}
			break;

		case PIC_OP_FILL: //fill
			while (vectorIsNonOpcode(data[curPos])) {
				vectorGetAbsCoords(data, curPos, x, y);
				_gfx->Pic_Fill(x, y, pic_color, pic_priority, pic_control);
			}
			break;

		case PIC_OP_SET_PATTERN:
			pattern_Code = data[curPos++];
			break;
		case PIC_OP_SHORT_PATTERNS:
			vectorGetPatternTexture(data, curPos, pattern_Code, pattern_Texture);
			vectorGetAbsCoords(data, curPos, x, y);
			_gfx->Draw_Pattern(x, y, pic_color, pic_priority, pic_control, pattern_Code, pattern_Texture);
			while (vectorIsNonOpcode(data[curPos])) {
				vectorGetPatternTexture(data, curPos, pattern_Code, pattern_Texture);
				vectorGetRelCoords(data, curPos, x, y, x, y);
				_gfx->Draw_Pattern(x, y, pic_color, pic_priority, pic_control, pattern_Code, pattern_Texture);
			}
			break;
		case PIC_OP_MEDIUM_PATTERNS:
			vectorGetPatternTexture(data, curPos, pattern_Code, pattern_Texture);
			vectorGetAbsCoords(data, curPos, x, y);
			_gfx->Draw_Pattern(x, y, pic_color, pic_priority, pic_control, pattern_Code, pattern_Texture);
			while (vectorIsNonOpcode(data[curPos])) {
				vectorGetPatternTexture(data, curPos, pattern_Code, pattern_Texture);
				vectorGetRelCoordsMed(data, curPos, x, y, x, y);
				_gfx->Draw_Pattern(x, y, pic_color, pic_priority, pic_control, pattern_Code, pattern_Texture);
			}
			break;
		case PIC_OP_ABSOLUTE_PATTERN:
			while (vectorIsNonOpcode(data[curPos])) {
				vectorGetPatternTexture(data, curPos, pattern_Code, pattern_Texture);
				vectorGetAbsCoords(data, curPos, x, y);
				_gfx->Draw_Pattern(x, y, pic_color, pic_priority, pic_control, pattern_Code, pattern_Texture);
			}
			break;

		case PIC_OP_OPX: // Extended functions
			if (isEGA) {
				switch (pic_op = data[curPos++]) {
				case PIC_OPX_EGA_SET_PALETTE_ENTRIES:
					while (vectorIsNonOpcode(data[curPos])) {
						byte = data[curPos++];
						if (byte >= PIC_EGAPALETTE_TOTALSIZE) {
							error("picture trying to write to invalid EGA-palette");
						}
						EGApalettes[byte] = data[curPos++];
					}
					break;
				case PIC_OPX_EGA_SET_PALETTE:
					byte = data[curPos++];
					if (byte >= PIC_EGAPALETTE_COUNT) {
						error("picture trying to write to invalid palette %d", (int)byte);
					}
					byte *= PIC_EGAPALETTE_SIZE;
					for (i = 0; i < PIC_EGAPALETTE_SIZE; i++) {
						EGApalettes[byte + i] = data[curPos++];
					}
					break;
				case PIC_OPX_EGA_MONO0:
					curPos += 41;
					break;
				case PIC_OPX_EGA_MONO1:
				case PIC_OPX_EGA_MONO3:
					curPos++;
					break;
				case PIC_OPX_EGA_MONO2:
				case PIC_OPX_EGA_MONO4:
					break;
				case PIC_OPX_EGA_EMBEDDED_VIEW:
					vectorGetAbsCoords(data, curPos, x, y);
					size = READ_LE_UINT16(data + curPos); curPos += 2;
					drawCel(x, y, data + curPos, size);
					curPos += size;
					break;
				case PIC_OPX_EGA_SET_PRIORITY_TABLE:
					//FIXME
					//g_sci->PriBands(ptr);
					curPos += 14;
					break;
				default:
					error("Unsupported sci1 extended pic-operation %X", pic_op);
				}
			} else {
				switch (pic_op = data[curPos++]) {
				case PIC_OPX_VGA_SET_PALETTE_ENTRIES:
					while (vectorIsNonOpcode(data[curPos])) {
						curPos++; // skip commands
					}
					break;
				case PIC_OPX_VGA_SET_PALETTE:
					curPos += 256 + 4; // Skip over mapping and timestamp
					for (i = 0; i < 256; i++) {
						palette.colors[i].used = data[curPos++];
						palette.colors[i].r = data[curPos++]; palette.colors[i].g = data[curPos++]; palette.colors[i].b = data[curPos++];
					}
					_palette->set(&palette, 2);
					break;
				case PIC_OPX_VGA_EMBEDDED_VIEW: // draw cel
					vectorGetAbsCoords(data, curPos, x, y);
					size = READ_LE_UINT16(data + curPos); curPos += 2;
					drawCel(x, y, data + curPos, size);
					curPos += size;
					break;
				case PIC_OPX_VGA_PRIORITY_TABLE_EQDIST:
					//FIXME
					//g_sci->InitPri(READ_LE_UINT16(ptr), READ_LE_UINT16(ptr + 2));
					debug(5, "DrawPic::InitPri %d %d",
						READ_LE_UINT16(data + curPos), READ_LE_UINT16(data + curPos + 2));
					curPos += 4;
					break;
				case PIC_OPX_VGA_PRIORITY_TABLE_EXPLICIT:
					//FIXME
					//g_sci->PriBands(ptr);
					curPos += 14;
					break;
				default:
					error("Unsupported sci1 extended pic-operation %X", pic_op);
				}
			}
			break;
		case PIC_OP_TERMINATE:
			_priority = pic_priority;
			// Dithering EGA pictures
			if (isEGA) {
				_screen->dither();
			}
			return;
		default:
			error("Unsupported pic-operation %X", pic_op);
		}
	}
	error("picture vector data without terminator");
}

bool SciGuiPicture::vectorIsNonOpcode(byte byte) {
	if (byte >= PIC_OP_FIRST)
		return false;
	return true;
}

void SciGuiPicture::vectorGetAbsCoords(byte *data, int &curPos, int16 &x, int16 &y) {
	byte byte = data[curPos++];
	x = data[curPos++] + ((byte & 0xF0) << 4);
	y = data[curPos++] + ((byte & 0x0F) << 8);
	if (_style & PIC_STYLE_MIRRORED) x = 319 - x;
}

void SciGuiPicture::vectorGetRelCoords(byte *data, int &curPos, int16 oldx, int16 oldy, int16 &x, int16 &y) {
	byte byte = data[curPos++];
	if (byte & 0x80) {
		x = oldx - ((byte >> 4) & 7) * (_style & PIC_STYLE_MIRRORED ? -1 : 1);
	} else {
		x = oldx + (byte >> 4) * (_style & PIC_STYLE_MIRRORED ? -1 : 1);
	}
	if (byte & 0x08) {
		y = oldy - (byte & 7);
	} else {
		y = oldy + (byte & 7);
	}
}

void SciGuiPicture::vectorGetRelCoordsMed(byte *data, int &curPos, int16 oldx, int16 oldy, int16 &x, int16 &y) {
	byte byte = data[curPos++];
	if (byte & 0x80) {
		y = oldy - (byte & 0x7F);
	} else {
		y = oldy + byte;
	}
	byte = data[curPos++];
	if (byte & 0x80) {
		x = oldx - (128 - (byte & 0x7F)) * (_style & PIC_STYLE_MIRRORED ? -1 : 1);
	} else {
		x = oldx + byte * (_style & PIC_STYLE_MIRRORED ? -1 : 1);
	}
}

void SciGuiPicture::vectorGetPatternTexture(byte *data, int &curPos, int16 pattern_Code, int16 &pattern_Texture) {
	if (pattern_Code & SCI_PATTERN_CODE_USE_TEXTURE) {
		pattern_Texture = (data[curPos++] >> 1) & 0x7f;
	}
}

} // End of namespace Sci
