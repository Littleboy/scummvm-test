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

#include "common/util.h"
#include "common/stack.h"
#include "graphics/primitives.h"

#include "sci/sci.h"
#include "sci/engine/state.h"
#include "sci/tools.h"
#include "sci/gui/gui.h"
#include "sci/gui/gui_screen.h"
#include "sci/gui/gui_palette.h"
#include "sci/gui/gui_transitions.h"

namespace Sci {

SciGuiTransitions::SciGuiTransitions(SciGui *gui, SciGuiScreen *screen, SciGuiPalette *palette, bool isVGA)
	: _gui(gui), _screen(screen), _palette(palette), _isVGA(isVGA) {
	init();
}

SciGuiTransitions::~SciGuiTransitions() {
	delete[] _oldScreen;
}

// This table contains a mapping between oldIDs (prior SCI1LATE) and newIDs
byte oldTransitionIDs[256][2] = {
	{  8, SCI_TRANSITIONS_BLOCKS },
	{ 18, SCI_TRANSITIONS_PIXELATION },
	{ 30, SCI_TRANSITIONS_FADEPALETTE },
	{ 40, SCI_TRANSITIONS_SCROLLRIGHT },
	{ 41, SCI_TRANSITIONS_SCROLLLEFT },
	{ 42, SCI_TRANSITIONS_SCROLLUP },
	{ 43, SCI_TRANSITIONS_SCROLLDOWN },
	{ 255, 255 }
};

void SciGuiTransitions::init() {
	_oldScreen = new byte[_screen->_displayHeight * _screen->_displayWidth];

	if (getSciVersion() >= SCI_VERSION_1_LATE)
		_translationTable = NULL;
	else
		_translationTable = (byte *)&oldTransitionIDs;
}

void SciGuiTransitions::setup(int16 number, bool blackoutFlag) {
	_number = number;
	_blackoutFlag = blackoutFlag;
}

void SciGuiTransitions::doit(Common::Rect picRect) {
	byte *translationPtr;

	_picRect = picRect;

	if (_translationTable) {
		// We need to translate the ID
		translationPtr = _translationTable;
		while (*translationPtr != 255) {
			if (*translationPtr == _number) {
				translationPtr++; _number = *translationPtr;
				break;
			}
			translationPtr += 2;
		}
		if (*translationPtr == 255) {
			warning("SciGuiTransitions: old ID %d not supported", _number);
			setNewPalette(); setNewScreen();
			_screen->_picNotValid = 0;
			return;
		}
	}

	switch (_number) {
	case SCI_TRANSITIONS_PIXELATION:
		setNewPalette(); pixelation();
		break;

	case SCI_TRANSITIONS_BLOCKS:
		setNewPalette(); blocks();
		break;

	case SCI_TRANSITIONS_FADEPALETTE:
		fadeOut(); setNewScreen(); fadeIn();
		break;

	case SCI_TRANSITIONS_SCROLLRIGHT:
	case SCI_TRANSITIONS_SCROLLLEFT:
	case SCI_TRANSITIONS_SCROLLUP:
	case SCI_TRANSITIONS_SCROLLDOWN:
		setNewPalette(); scroll();
		break;

	default:
		warning("SciGuiTransitions: ID %d not implemented", _number);
		setNewPalette(); setNewScreen();
	}

	_screen->_picNotValid = 0;
}

void SciGuiTransitions::setNewPalette() {
	if (_isVGA)
		_palette->setOnScreen();
}

void SciGuiTransitions::setNewScreen() {
	_screen->copyRectToScreen(_picRect);
	g_system->updateScreen();
}

// Note: dont do too many steps in here, otherwise cpu will crap out because of the load
void SciGuiTransitions::fadeOut() {
	byte oldPalette[4 * 256], workPalette[4 * 256];
	int16 stepNr, colorNr;

	g_system->grabPalette(oldPalette, 0, 256);

	for (stepNr = 100; stepNr >= 0; stepNr -= 10) {
		for (colorNr = 1; colorNr < 255; colorNr++){
			workPalette[colorNr * 4 + 0] = oldPalette[colorNr * 4] * stepNr / 100;
			workPalette[colorNr * 4 + 1] = oldPalette[colorNr * 4 + 1] * stepNr / 100;
			workPalette[colorNr * 4 + 2] = oldPalette[colorNr * 4 + 2] * stepNr / 100;
		}
		g_system->setPalette(workPalette + 4, 1, 254);
		_gui->wait(2);
	}
}

// Note: dont do too many steps in here, otherwise cpu will crap out because of the load
void SciGuiTransitions::fadeIn() {
	int16 stepNr;

	for (stepNr = 0; stepNr <= 100; stepNr += 10) {
		_palette->setIntensity(1, 254, 100, true);
		_gui->wait(2);
	}
}

// pixelates the new picture over the old one - works against the whole screen
void SciGuiTransitions::pixelation () {
	uint16 mask = 0x40, stepNr = 0;
	Common::Rect pixelRect;

	do {
		mask = (mask & 1) ? (mask >> 1) ^ 0xB400 : mask >> 1;
		if (mask >= 320 * 200)
			continue;
		pixelRect.left = mask % 320; pixelRect.right = pixelRect.left + 1;
		pixelRect.top = mask / 320;	pixelRect.bottom = pixelRect.top + 1;
		_screen->copyRectToScreen(pixelRect);
		if ((stepNr & 0x3FF) == 0) {
			g_system->updateScreen();
			g_system->delayMillis(5);
		}
		stepNr++;
	} while (mask != 0x40);
}

// like pixelation but uses 8x8 blocks - works against the whole screen
void SciGuiTransitions::blocks() {
	uint16 mask = 0x40, stepNr = 0;
	Common::Rect blockRect;

	do {
		mask = (mask & 1) ? (mask >> 1) ^ 0x240 : mask >> 1;
		if (mask >= 40 * 25)
			continue;
		blockRect.left = (mask % 40) << 3; blockRect.right = blockRect.left + 8;
		blockRect.top = (mask / 40) << 3; blockRect.bottom = blockRect.top + 8;
		_screen->copyRectToScreen(blockRect);
		if ((stepNr & 7) == 0) {
			g_system->updateScreen();
			g_system->delayMillis(4);
		}
		stepNr++;
	} while (mask != 0x40);
}

// scroll old screen (up/down/left/right) and insert new screen that way - works on _picRect area
void SciGuiTransitions::scroll() {
	int16 screenWidth, screenHeight;
	byte *oldScreenPtr;
	int16 stepNr = 0;
	Common::Rect oldMoveRect = _picRect;
	Common::Rect newMoveRect = _picRect;
	Common::Rect newScreenRect = _picRect;

	_screen->copyFromScreen(_oldScreen);
	screenWidth = _screen->_displayWidth; screenHeight = _screen->_displayHeight;

	oldScreenPtr = _oldScreen + _picRect.left + _picRect.top * screenWidth;

	switch (_number) {
	case SCI_TRANSITIONS_SCROLLLEFT:
		newScreenRect.right = newScreenRect.left;
		newMoveRect.left = newMoveRect.right;
		while (oldMoveRect.left < oldMoveRect.right) {
			oldScreenPtr++; oldMoveRect.right--;
			if (oldMoveRect.right > oldMoveRect.left)
				g_system->copyRectToScreen(oldScreenPtr, screenWidth, oldMoveRect.left, oldMoveRect.top, oldMoveRect.width(), oldMoveRect.height());
			newScreenRect.right++; newMoveRect.left--;
			_screen->copyRectToScreen(newScreenRect, newMoveRect.left, newMoveRect.top);
			if ((stepNr & 1) == 0) {
				g_system->updateScreen();
				g_system->delayMillis(1);
			}
			stepNr++;
		}
		if ((stepNr & 1) == 0)
			g_system->updateScreen();
		break;

	case SCI_TRANSITIONS_SCROLLRIGHT:
		newScreenRect.left = newScreenRect.right;
		while (oldMoveRect.left < oldMoveRect.right) {
			oldMoveRect.left++;
			if (oldMoveRect.right > oldMoveRect.left)
				g_system->copyRectToScreen(oldScreenPtr, screenWidth, oldMoveRect.left, oldMoveRect.top, oldMoveRect.width(), oldMoveRect.height());
			newScreenRect.left--;
			_screen->copyRectToScreen(newScreenRect, newMoveRect.left, newMoveRect.top);
			if ((stepNr & 1) == 0) {
				g_system->updateScreen();
				g_system->delayMillis(1);
			}
			stepNr++;
		}
		if ((stepNr & 1) == 0)
			g_system->updateScreen();
		break;

	case SCI_TRANSITIONS_SCROLLUP:
		newScreenRect.bottom = newScreenRect.top;
		newMoveRect.top = newMoveRect.bottom;
		while (oldMoveRect.top < oldMoveRect.bottom) {
			oldScreenPtr += screenWidth; oldMoveRect.top++;
			if (oldMoveRect.top < oldMoveRect.bottom)
				g_system->copyRectToScreen(oldScreenPtr, screenWidth, _picRect.left, _picRect.top, oldMoveRect.width(), oldMoveRect.height());
			newScreenRect.bottom++;	newMoveRect.top--;
			_screen->copyRectToScreen(newScreenRect, newMoveRect.left, newMoveRect.top);
			g_system->updateScreen();
			g_system->delayMillis(3);
		}
		break;

	case SCI_TRANSITIONS_SCROLLDOWN:
		newScreenRect.top = newScreenRect.bottom;
		while (oldMoveRect.top < oldMoveRect.bottom) {
			oldMoveRect.top++;
			if (oldMoveRect.top < oldMoveRect.bottom)
				g_system->copyRectToScreen(oldScreenPtr, screenWidth, oldMoveRect.left, oldMoveRect.top, oldMoveRect.width(), oldMoveRect.height());
			newScreenRect.top--;
			_screen->copyRectToScreen(newScreenRect, _picRect.left, _picRect.top);
			g_system->updateScreen();
			g_system->delayMillis(3);
		}
		break;
	}
}

} // End of namespace Sci
