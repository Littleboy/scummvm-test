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

// Handles effect polygons.
//
// EffectPolyProcess() monitors triggering of effect code (i.e. a moving
// actor entering an effect polygon).
// EffectProcess() runs the appropriate effect code.
//
// NOTE: Currently will only run one effect process at a time, i.e.
// effect polygons will not currently nest. It won't be very difficult
// to fix this if required.

#include "tinsel/actors.h"
#include "tinsel/dw.h"
#include "tinsel/events.h"
#include "tinsel/pid.h"
#include "tinsel/pcode.h"		// LEAD_ACTOR
#include "tinsel/polygons.h"
#include "tinsel/rince.h"
#include "tinsel/sched.h"


namespace Tinsel {

struct EP_INIT {
	HPOLYGON	hEpoly;
	PMACTOR		pActor;
	int		index;
};

/**
 * Runs an effect polygon's Glitter code with ENTER event, waits for the
 * actor to leave that polygon. Then runs the polygon's Glitter code
 * with LEAVE event.
 */
static void EffectProcess(CORO_PARAM, const void *param) {
	// COROUTINE
	CORO_BEGIN_CONTEXT;
	CORO_END_CONTEXT(_ctx);

	EP_INIT *to = (EP_INIT *)param;		// get the stuff copied to process when it was created

	CORO_BEGIN_CODE(_ctx);

	int		x, y;		// Lead actor position

	// Run effect poly enter script
	effRunPolyTinselCode(to->hEpoly, ENTER, to->pActor->actorID);

	do {
		CORO_SLEEP(1);
		GetMActorPosition(to->pActor, &x, &y);
	} while (InPolygon(x, y, EFFECT) == to->hEpoly);

	// Run effect poly leave script
	effRunPolyTinselCode(to->hEpoly, LEAVE, to->pActor->actorID);

	SetMAinEffectPoly(to->index, false);

	CORO_END_CODE;
}

/**
 * If the actor was not already in an effect polygon, checks to see if
 * it has just entered one. If it has, a process is started up to run
 * the polygon's Glitter code.
 */
static void FettleEffectPolys(int x, int y, int index, PMACTOR pActor) {
	HPOLYGON	hPoly;
	EP_INIT		epi;

	// If just entered an effect polygon, the effect should be triggered.
	if (!IsMAinEffectPoly(index)) {
		hPoly = InPolygon(x, y, EFFECT);
		if (hPoly != NOPOLY) {
			//Just entered effect polygon
			SetMAinEffectPoly(index, true);

			epi.hEpoly = hPoly;
			epi.pActor = pActor;
			epi.index = index;
			g_scheduler->createProcess(PID_TCODE, EffectProcess, &epi, sizeof(epi));
		}
	}
}

/**
 * Just calls FettleEffectPolys() every clock tick.
 */
void EffectPolyProcess(CORO_PARAM, const void *param) {
	// COROUTINE
	CORO_BEGIN_CONTEXT;
	CORO_END_CONTEXT(_ctx);

	CORO_BEGIN_CODE(_ctx);
	while (1) {
		for (int i = 0; i < MAX_MOVERS; i++) {
			PMACTOR pActor = GetLiveMover(i);
			if (pActor != NULL) {
				int	x, y;
				GetMActorPosition(pActor, &x, &y);
				FettleEffectPolys(x, y, i, pActor);
			}
		}

		CORO_SLEEP(1);		// allow re-scheduling
	}
	CORO_END_CODE;
}

} // End of namespace Tinsel
