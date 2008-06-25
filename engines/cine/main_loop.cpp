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


#include "common/scummsys.h"
#include "common/events.h"
#include "common/system.h"

#include "cine/main_loop.h"
#include "cine/object.h"
#include "cine/various.h"
#include "cine/bg_list.h"
#include "cine/sound.h"

namespace Cine {

struct MouseStatusStruct {
	int left;
	int right;
};

MouseStatusStruct mouseData;

uint16 mouseRight = 0;
uint16 mouseLeft = 0;

int lastKeyStroke = 0;

uint16 mouseUpdateStatus;
uint16 dummyU16;

static void processEvent(Common::Event &event) {
	switch (event.type) {
	case Common::EVENT_LBUTTONDOWN:
		mouseLeft = 1;
		break;
	case Common::EVENT_RBUTTONDOWN:
		mouseRight = 1;
		break;
	case Common::EVENT_MOUSEMOVE:
		break;
	case Common::EVENT_QUIT:
		exitEngine = 1;
		break;
	case Common::EVENT_KEYDOWN:
		switch (event.kbd.keycode) {
		case Common::KEYCODE_RETURN:
		case Common::KEYCODE_KP_ENTER:
		case Common::KEYCODE_KP5:
			if (allowPlayerInput) {
				mouseLeft = 1;
			}
			break;
		case Common::KEYCODE_ESCAPE:
			if (allowPlayerInput) {
				mouseRight = 1;
			}
			break;
		case Common::KEYCODE_F1:
			if (allowPlayerInput) {
				playerCommand = 0; // EXAMINE
				makeCommandLine();
			}
			break;
		case Common::KEYCODE_F2:
			if (allowPlayerInput) {
				playerCommand = 1; // TAKE
				makeCommandLine();
			}
			break;
		case Common::KEYCODE_F3:
			if (allowPlayerInput) {
				playerCommand = 2; // INVENTORY
				makeCommandLine();
			}
			break;
		case Common::KEYCODE_F4:
			if (allowPlayerInput) {
				playerCommand = 3; // USE
				makeCommandLine();
			}
			break;
		case Common::KEYCODE_F5:
			if (allowPlayerInput) {
				playerCommand = 4; // ACTIVATE
				makeCommandLine();
			}
			break;
		case Common::KEYCODE_F6:
			if (allowPlayerInput) {
				playerCommand = 5; // SPEAK
				makeCommandLine();
			}
			break;
		case Common::KEYCODE_F9:
			if (allowPlayerInput && !inMenu) {
				makeActionMenu();
				makeCommandLine();
			}
			break;
		case Common::KEYCODE_F10:
			if (!disableSystemMenu && !inMenu) {
				g_cine->makeSystemMenu();
			}
			break;
		default:
			lastKeyStroke = event.kbd.keycode;
			break;
		}
		break;
	default:
		break;
	}
}

void manageEvents() {
	Common::EventManager *eventMan = g_system->getEventManager();

	uint32 nextFrame = g_system->getMillis() + kGameTimerDelay * kGameSpeed;
	do {
		Common::Event event;
		while (eventMan->pollEvent(event)) {
			processEvent(event);
		}
		g_system->updateScreen();
		g_system->delayMillis(20);
	} while (g_system->getMillis() < nextFrame);

	g_sound->update();
	mouseData.left = mouseLeft;
	mouseData.right = mouseRight;
	mouseLeft = 0;
	mouseRight = 0;
}

void getMouseData(uint16 param, uint16 *pButton, uint16 *pX, uint16 *pY) {
	Common::Point mouse = g_system->getEventManager()->getMousePos();
	*pX = mouse.x;
	*pY = mouse.y;

	*pButton = 0;

	if (mouseData.right) {
		(*pButton) |= 2;
	}

	if (mouseData.left) {
		(*pButton) |= 1;
	}
}

int getKeyData() {
	int k = lastKeyStroke;

	lastKeyStroke = -1;

	return k;
}

void CineEngine::mainLoop(int bootScriptIdx) {
	bool playerAction;
	uint16 quitFlag;
	byte di;
	uint16 mouseButton;

	quitFlag = 0;
	exitEngine = 0;

	if (_preLoad == false) {
		resetBgIncrustList();

		setTextWindow(0, 0, 20, 200);

		errorVar = 0;

		addScriptToList0(bootScriptIdx);

		menuVar = 0;

//		gfxRedrawPage(page0c, page0, page0c, page0, -1);
//		gfxWaitVBL();
//		gfxRedrawMouseCursor();

		inMenu = false;
		allowPlayerInput = 0;
		checkForPendingDataLoadSwitch = 0;

		fadeRequired = false;
		isDrawCommandEnabled = 0;
		waitForPlayerClick = 0;
		menuCommandLen = 0;

		playerCommand = -1;
		strcpy(commandBuffer, "");

		globalVars[VAR_MOUSE_X_POS] = 0;
		globalVars[VAR_MOUSE_Y_POS] = 0;
		if (g_cine->getGameType() == Cine::GType_OS) {
			globalVars[VAR_BYPASS_PROTECTION] = 0; // set to 1 to bypass the copy protection
			globalVars[VAR_LOW_MEMORY] = 0; // set to 1 to disable some animations, sounds etc.
		}

		strcpy(newPrcName, "");
		strcpy(newRelName, "");
		strcpy(newObjectName, "");
		strcpy(newMsgName, "");
		strcpy(currentCtName, "");
		strcpy(currentPartName, "");

		g_sound->stopMusic();
	}

	do {
		stopMusicAfterFadeOut();
		di = executePlayerInput();
		
		// Clear the zoneQuery table (Operation Stealth specific)
		if (g_cine->getGameType() == Cine::GType_OS) {
			for (uint i = 0; i < NUM_MAX_ZONE; i++) {
				zoneQuery[i] = 0;
			}
		}

		processSeqList();
		executeList1();
		executeList0();

		purgeList1();
		purgeList0();

		if (playerCommand == -1) {
			setMouseCursor(MOUSE_CURSOR_NORMAL);
		} else {
			setMouseCursor(MOUSE_CURSOR_CROSS);
		}

		if (renderer->ready()) {
			renderer->drawFrame();
		}

		if (waitForPlayerClick) {
			playerAction = false;

			_messageLen <<= 3;
			if (_messageLen < 0x800)
				_messageLen = 0x800;

			do {
				manageEvents();
				getMouseData(mouseUpdateStatus, &mouseButton, &dummyU16, &dummyU16);
			} while (mouseButton != 0);

			menuVar = 0;

			do {
				manageEvents();
				getMouseData(mouseUpdateStatus, &mouseButton, &dummyU16, &dummyU16);
				playerAction = (mouseButton != 0) || processKeyboard(menuVar);
				mainLoopSub6();
			} while (!playerAction);

			menuVar = 0;

			do {
				manageEvents();
				getMouseData(mouseUpdateStatus, &mouseButton, &dummyU16, &dummyU16);
			} while (mouseButton != 0);

			waitForPlayerClick = 0;

			removeMessages();
		}

		if (checkForPendingDataLoadSwitch) {
			checkForPendingDataLoad();

			checkForPendingDataLoadSwitch = 0;
		}

		if (di) {
			if ("quit"[menuCommandLen] == (char)di) {
				++menuCommandLen;
				if (menuCommandLen == 4) {
					quitFlag = 1;
				}
			} else {
				menuCommandLen = 0;
			}
		}

		manageEvents();

	} while (!exitEngine && !quitFlag && _danKeysPressed != 7);

	hideMouse();
	g_sound->stopMusic();
	// if (g_cine->getGameType() == Cine::GType_OS) {
	//	freeUnkList();
	// }
	closePart();
}

} // End of namespace Cine
