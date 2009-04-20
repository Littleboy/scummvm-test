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

#ifndef SCI_ENGINE_KERNEL_H
#define SCI_ENGINE_KERNEL_H

#include "common/scummsys.h"
#include "common/debug.h"

#include "sci/engine/kdebug.h"
#include "sci/uinput.h"
#include "sci/scicore/sciconsole.h" /* sciprintf() */

namespace Sci {

struct Node;	// from vm.h
struct List;	// from vm.h

extern int _kdebug_cheap_event_hack;
extern int _kdebug_cheap_soundcue_hack;
extern int stop_on_event;

extern int _debug_seeking;
extern int _debug_step_running;

#define AVOIDPATH_DYNMEM_STRING "AvoidPath polyline"


/* Formerly, the heap macros were here; they have been deprecated, however. */

/******************** Selector functionality ********************/

#define GET_SEL32(_o_, _slc_) read_selector(s, _o_, s->selector_map._slc_, __FILE__, __LINE__)
#define GET_SEL32V(_o_, _slc_) (GET_SEL32(_o_, _slc_).offset)
#define GET_SEL32SV(_o_, _slc_) ((int16)(GET_SEL32(_o_, _slc_).offset))
/* Retrieves a selector from an object
** Parameters: (reg_t) object: The address of the object which the selector should be read from
**             (selector_name) selector: The selector to read
** Returns   : (int16/uint16/reg_t) The selector value
** This macro halts on error. 'selector' must be a selector name registered in vm.h's
** selector_map_t and mapped in script.c.
*/

#define PUT_SEL32(_o_, _slc_, _val_) write_selector(s, _o_, s->selector_map._slc_, _val_, __FILE__, __LINE__)
#define PUT_SEL32V(_o_, _slc_, _val_) write_selector(s, _o_, s->selector_map._slc_, make_reg(0, _val_), __FILE__, __LINE__)
/* Writes a selector value to an object
** Parameters: (reg_t) object: The address of the object which the selector should be written to
**             (selector_name) selector: The selector to read
**             (int16) value: The value to write
** Returns   : (void)
** This macro halts on error. 'selector' must be a selector name registered in vm.h's
** selector_map_t and mapped in script.c.
*/


#define INV_SEL(_object_, _selector_, _noinvalid_) \
	s, _object_,  s->selector_map._selector_, _noinvalid_, funct_nr, argv, argc, __FILE__, __LINE__
/* Kludge for use with invoke_selector(). Used for compatibility with compilers that can't
** handle vararg macros.
*/


reg_t read_selector(EngineState *s, reg_t object, Selector selector_id, const char *fname, int line);
void write_selector(EngineState *s, reg_t object, Selector selector_id, reg_t value, const char *fname, int line);
int invoke_selector(EngineState *s, reg_t object, int selector_id, int noinvalid, int kfunct,
	StackPtr k_argp, int k_argc, const char *fname, int line, int argc, ...);


/******************** Text functionality ********************/
char *kernel_lookup_text(EngineState *s, reg_t address, int index);
/* Looks up text referenced by scripts
** Parameters: (EngineState *s): The current state
**             (reg_t) address: The address to look up
**             (int) index: The relative index
** Returns   : (char *): The referenced text, or NULL on error.
** SCI uses two values to reference to text: An address, and an index. The address
** determines whether the text should be read from a resource file, or from the heap,
** while the index either refers to the number of the string in the specified source,
** or to a relative position inside the text.
*/


/******************** Debug functionality ********************/
#define KERNEL_OOPS(reason) kernel_oops(s, __FILE__, __LINE__, reason)

#ifdef SCI_KERNEL_DEBUG

#define CHECK_THIS_KERNEL_FUNCTION if (s->debug_mode & (1 << SCIkFUNCCHK_NR)) {\
	int i;\
	sciprintf("Kernel CHECK: %s[%x](", s->kernel_names[funct_nr], funct_nr); \
	for (i = 0; i < argc; i++) { \
		sciprintf("%04x", 0xffff & UKPV(i)); \
		if (i+1 < argc) sciprintf(", "); \
	} \
	sciprintf(")\n"); \
} \

#else /* !SCI_KERNEL_DEBUG */

#define CHECK_THIS_KERNEL_FUNCTION

#endif /* !SCI_KERNEL_DEBUG */


bool is_object(EngineState *s, reg_t obj);
/* Checks whether a heap address contains an object
** Parameters: (EngineState *) s: The current state
**             (reg_t) obj: The address to check
** Returns   : (bool) true if it is an object, false otherwise
*/

/******************** Kernel function parameter macros ********************/

/* Returns the parameter value or (alt) if not enough parameters were supplied */


#define KP_ALT(x, alt) ((x < argc)? argv[x] : (alt))
#define KP_UINT(x) ((uint16) x.offset)
#define KP_SINT(x) ((int16) x.offset)


#define SKPV(x) KP_SINT(argv[x])
#define UKPV(x) KP_UINT(argv[x])
#define SKPV_OR_ALT(x,a) KP_SINT(KP_ALT(x, make_reg(0, a)))
#define UKPV_OR_ALT(x,a) KP_UINT(KP_ALT(x, make_reg(0, a)))

reg_t *kernel_dereference_reg_pointer(EngineState *s, reg_t pointer, int entries);
byte *kernel_dereference_bulk_pointer(EngineState *s, reg_t pointer, int entries);
#define kernel_dereference_char_pointer(state, pointer, entries) (char*)kernel_dereference_bulk_pointer(state, pointer, entries)
/* Dereferences a heap pointer
** Parameters: (EngineState *) s: The state to operate on
**             (reg_t ) pointer: The pointer to dereference
**             (int) entries: The number of values expected (for checking)
**                            (use 0 for strings)
** Returns   : (reg_t/char *): A physical reference to the address pointed
**                        to, or NULL on error or if not enugh entries
**                        were available
** reg_t dereferenciation also assures alignedness of data.
*/

int kernel_oops(EngineState *s, const char *file, int line, const char *reason);
/* Halts script execution and informs the user about an internal kernel error or failed assertion
** Parameters: (EngineState *) s: The state to use
**            (const char *) file: The file the oops occured in
**            (int) line: The line the oops occured in
**            (const char *) reason: Reason for the kernel oops
*/

/******************** Priority macros/functions ********************/

extern int sci01_priority_table_flags; /* 1: delete, 2: print */

int _find_priority_band(EngineState *s, int band);
/* Finds the position of the priority band specified
** Parameters: (EngineState *) s: State to search in
**             (int) band: Band to look for
** Returns   : (int) Offset at which the band starts
*/

int _find_view_priority(EngineState *s, int y);
/* Does the opposite of _find_priority_band
** Parameters: (EngineState *) s: State
**             (int) y: Coordinate to check
** Returns   : (int) The priority band y belongs to
*/

#define SCI0_VIEW_PRIORITY_14_ZONES(y) (((y) < s->priority_first)? 0 : (((y) >= s->priority_last)? 14 : 1\
	+ ((((y) - s->priority_first) * 14) / (s->priority_last - s->priority_first))))

#define SCI0_PRIORITY_BAND_FIRST_14_ZONES(nr) ((((nr) == 0)? 0 :  \
	((s->priority_first) + (((nr)-1) * (s->priority_last - s->priority_first)) / 14)))

#define SCI0_VIEW_PRIORITY(y) (((y) < s->priority_first)? 0 : (((y) >= s->priority_last)? 14 : 1\
	+ ((((y) - s->priority_first) * 15) / (s->priority_last - s->priority_first))))

#define SCI0_PRIORITY_BAND_FIRST(nr) ((((nr) == 0)? 0 :  \
	((s->priority_first) + (((nr)-1) * (s->priority_last - s->priority_first)) / 15)))

#define VIEW_PRIORITY(y) _find_view_priority(s, y)
#define PRIORITY_BAND_FIRST(nr) _find_priority_band(s, nr)





/******************** Dynamic view list functions ********************/

Common::Rect set_base(EngineState *s, reg_t object);
/* Determines the base rectangle of the specified view object
** Parameters: (EngineState *) s: The state to use
**             (reg_t) object: The object to set
** Returns   : (abs_rect) The absolute base rectangle
*/

extern Common::Rect get_nsrect(EngineState *s, reg_t object, byte clip);
/* Determines the now-seen rectangle of a view object
** Parameters: (EngineState *) s: The state to use
**             (reg_t) object: The object to check
**             (byte) clip: Flag to determine wheter priority band
**                          clipping should be performed
** Returns   : (abs_rect) The absolute rectangle describing the
** now-seen area.
*/

void _k_dyn_view_list_prepare_change(EngineState *s);
/* Removes all views in anticipation of a new window or text */
void _k_dyn_view_list_accept_change(EngineState *s);
/* Redraws all views after a new window or text was added */




/******************** Misc functions ********************/

void process_sound_events(EngineState *s); /* Get all sound events, apply their changes to the heap */

#define LOOKUP_NODE(addr) lookup_node(s, (addr), __FILE__, __LINE__)
#define LOOKUP_LIST(addr) lookup_list(s, addr, __FILE__, __LINE__)

Node *lookup_node(EngineState *s, reg_t addr, const char *file, int line);
/* Resolves an address into a list node
** Parameters: (EngineState *) s: The state to operate on
**             (reg_t) addr: The address to resolve
**             (const char *) file: The file the function was called from
**             (int) line: The line number the function was called from
** Returns   : (Node *) The list node referenced, or NULL on error
*/


List *lookup_list(EngineState *s, reg_t addr, const char *file, int line);
/* Resolves a list pointer to a list
** Parameters: (EngineState *) s: The state to operate on
**             (reg_t) addr: The address to resolve
**             (const char *) file: The file the function was called from
**             (int) line: The line number the function was called from
** Returns   : (List *) The list referenced, or NULL on error
*/


/******************** Constants ********************/

/* Maximum length of a savegame name (including terminator character) */
#define SCI_MAX_SAVENAME_LENGTH 0x24

/* Flags for the signal selector */
#define _K_VIEW_SIG_FLAG_STOP_UPDATE    0x0001
#define _K_VIEW_SIG_FLAG_UPDATED        0x0002
#define _K_VIEW_SIG_FLAG_NO_UPDATE      0x0004
#define _K_VIEW_SIG_FLAG_HIDDEN         0x0008
#define _K_VIEW_SIG_FLAG_FIX_PRI_ON     0x0010
#define _K_VIEW_SIG_FLAG_ALWAYS_UPDATE  0x0020
#define _K_VIEW_SIG_FLAG_FORCE_UPDATE   0x0040
#define _K_VIEW_SIG_FLAG_REMOVE         0x0080
#define _K_VIEW_SIG_FLAG_FROZEN         0x0100
#define _K_VIEW_SIG_FLAG_IS_EXTRA       0x0200
#define _K_VIEW_SIG_FLAG_HIT_OBSTACLE   0x0400
#define _K_VIEW_SIG_FLAG_DOESNT_TURN    0x0800
#define _K_VIEW_SIG_FLAG_NO_CYCLER      0x1000
#define _K_VIEW_SIG_FLAG_IGNORE_HORIZON 0x2000
#define _K_VIEW_SIG_FLAG_IGNORE_ACTOR   0x4000
#define _K_VIEW_SIG_FLAG_DISPOSE_ME     0x8000

#define _K_VIEW_SIG_FLAG_STOPUPD 0x20000000 /* View has been stop-updated */


/* Sound status */
#define _K_SOUND_STATUS_STOPPED 0
#define _K_SOUND_STATUS_INITIALIZED 1
#define _K_SOUND_STATUS_PAUSED 2
#define _K_SOUND_STATUS_PLAYING 3



/* Kernel optimization flags */
#define KERNEL_OPT_FLAG_GOT_EVENT (1<<0)
#define KERNEL_OPT_FLAG_GOT_2NDEVENT (1<<1)


/******************** Kernel functions ********************/

/* Generic description: */
typedef reg_t kfunct(EngineState *s, int funct_nr, int argc, reg_t *argv);

struct kfunct_sig_pair_t {
	kfunct *fun; /* The actual function */
	const char *signature;  /* kfunct signature */
	const char *orig_name; /* Original name, in case we couldn't map it */
};

#define KF_OLD 0
#define KF_NEW 1
#define KF_NONE -1 /* No mapping, but name is known */
#define KF_TERMINATOR -42 /* terminates kfunct_mappers */

struct SciKernelFunction {
	int type; /* KF_* */
	const char *name;
	kfunct_sig_pair_t sig_pair;
};

extern SciKernelFunction kfunct_mappers[];



// New kernel functions
reg_t kStrLen(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kGetFarText(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kReadNumber(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kStrCat(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kStrCmp(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kSetSynonyms(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kLock(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kPalette(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kNumCels(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kNumLoops(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDrawCel(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kCoordPri(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kPriCoord(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kShakeScreen(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kSetCursor(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kMoveCursor(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kShow(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kPicNotValid(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kOnControl(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDrawPic(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kGetPort(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kSetPort(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kNewWindow(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDisposeWindow(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kCelWide(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kCelHigh(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kSetJump(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDirLoop(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDoAvoider(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kGetAngle(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kGetDistance(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kRandom(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kAbs(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kSqrt(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kTimesSin(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kTimesCos(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kCosMult(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kSinMult(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kTimesTan(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kTimesCot(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kCosDiv(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kSinDiv(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kValidPath(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kFOpen(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kFPuts(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kFGets(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kFClose(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kMapKeyToDir(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kGlobalToLocal(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kLocalToGlobal(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kWait(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kRestartGame(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDeviceInfo(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kGetEvent(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kCheckFreeSpace(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kFlushResources(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kGetSaveFiles(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kSetDebug(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kCheckSaveGame(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kSaveGame(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kRestoreGame(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kFileIO(EngineState *s, int funct_nr, int argc, reg_t *argp);
reg_t kGetTime(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kHaveMouse(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kJoystick(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kGameIsRestarting(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kGetCWD(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kSort(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kStrEnd(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kMemory(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kAvoidPath(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kParse(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kSaid(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kStrCpy(EngineState *s, int funct_nr, int argc, reg_t *argp);
reg_t kStrAt(EngineState *s, int funct_nr, int argc, reg_t *argp);
reg_t kEditControl(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDrawControl(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kHiliteControl(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kClone(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDisposeClone(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kCanBeHere(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kSetNowSeen(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kInitBresen(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDoBresen(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kBaseSetter(EngineState *s, int funct_nr, int argc, reg_t *argp);
reg_t kAddToPic(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kAnimate(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDisplay(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kGraph(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kFormat(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDoSound(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kAddMenu(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kSetMenu(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kGetMenu(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDrawStatus(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDrawMenuBar(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kMenuSelect(EngineState *s, int funct_nr, int argc, reg_t *argv);

reg_t kLoad(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kUnLoad(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kScriptID(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDisposeScript(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kIsObject(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kRespondsTo(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kNewList(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDisposeList(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kNewNode(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kFirstNode(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kLastNode(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kEmptyList(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kNextNode(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kPrevNode(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kNodeValue(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kAddAfter(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kAddToFront(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kAddToEnd(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kFindKey(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDeleteKey(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kMemoryInfo(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kGetSaveDir(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kTextSize(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kIsItSkip(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kMessage(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t kDoAudio(EngineState *s, int funct_nr, int argc, reg_t *argv);
reg_t k_Unknown(EngineState *s, int funct_nr, int argc, reg_t *argv);

// The Unknown/Unnamed kernel function
reg_t kstub(EngineState *s, int funct_nr, int argc, reg_t *argv);
// for unimplemented kernel functions
reg_t kNOP(EngineState *s, int funct_nr, int argc, reg_t *argv);
// for kernel functions that don't do anything
reg_t kFsciEmu(EngineState *s, int funct_nr, int argc, reg_t *argv);
// Emulating "old" kernel functions on the heap


} // End of namespace Sci

#endif // SCI_ENGIENE_KERNEL_H
