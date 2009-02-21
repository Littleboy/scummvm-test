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

/* Savegame handling for EngineState structs. Makes heavy use of cfsml magic. */
/* DON'T EDIT savegame.cpp ! Only modify savegame.cfsml, if something needs
** to be changed. Refer to freesci/docs/misc/cfsml.spec if you don't understand
** savegame.cfsml. If this doesn't solve your problem, contact the maintainer.
*/

#include <stdarg.h>
#include "sci/include/sci_memory.h"
#include "sci/include/gfx_operations.h"
#include "sci/include/sfx_engine.h"
#include "sci/include/engine.h"
#include "sci/engine/heap.h"
#include "common/stream.h"
#include "common/system.h"

#ifdef _MSC_VER
#include <direct.h>
#endif

#ifdef _WIN32
#pragma warning( disable : 4101 )
#endif

namespace Sci {

#define HUNK_TYPE_GFX_SNAPSHOT_STRING "g\n"

/* Missing:
** - SFXdriver
** - File input/output state (this is likely not to happen)
*/


const unsigned int PRINTFBUFLEN = 128;
int WSprintf(Common::WriteStream* str, const char *format, ...) {
	va_list args;
	char buf[PRINTFBUFLEN]; // default buffer to prevent new in common case
	char* writebuf = buf;

	unsigned int s = PRINTFBUFLEN;
	unsigned int outsize;
	while (true) {
		va_start(args, format);
		outsize = vsnprintf(writebuf, s, format, args);
		va_end(args);

		if (outsize == s) {
			if (s > 16384) { // there are limits...
				delete[] writebuf;
				warning("Saving failed: line much too long");
				return 0;
			}
			s *= 2;
			if (writebuf != buf) delete[] writebuf;
			writebuf = new char[s];
		} else {
			break;
		}
	}

	uint32 ret = str->write(writebuf, outsize);

	if (writebuf != buf) delete[] writebuf;

	return ret;
}

int SRSgetc(Common::SeekableReadStream* str) {
	char c = str->readSByte();
	if (str->err() || str->eos())
		return EOF;
	return c;
}

char* SRSgets(char* s, int size, Common::SeekableReadStream* str) {
	return str->readLine_NEW(s, size);
}



static EngineState *_global_save_state;
// Needed for some graphical stuff.
#define FILE_VERSION _global_save_state->savegame_version


void write_reg_t(Common::WriteStream *fh, reg_t *foo) {
	WSprintf(fh, PREG, PRINT_REG(*foo));
}

int read_reg_t(Common::SeekableReadStream *fh, reg_t *foo, const char *lastval, int *line, int *hiteof) {
	unsigned int segment, offset;

	if (sscanf(lastval, PREG, &segment, &offset) < 2) {
		sciprintf("Error parsing reg_t on line %d\n", *line);
		return 1;
	}

	*foo = make_reg(segment, offset);
	return 0;
}

void write_sci_version(Common::WriteStream *fh, sci_version_t *foo) {
	WSprintf(fh, "%d.%03d.%03d", SCI_VERSION_MAJOR(*foo), SCI_VERSION_MINOR(*foo), SCI_VERSION_PATCHLEVEL(*foo));
}

int read_sci_version(Common::SeekableReadStream *fh, sci_version_t *foo, const char *lastval, int *line, int *hiteof) {
	return version_parse(lastval, foo);
}

void write_PTN(Common::WriteStream *fh, parse_tree_node_t *foo) {
	if (foo->type == PARSE_TREE_NODE_LEAF)
		WSprintf(fh, "L%d", foo->content.value);
	else
		WSprintf(fh, "B(%d,%d)", foo->content.branches[0], foo->content.branches[1]);
}

int read_PTN(Common::SeekableReadStream *fh, parse_tree_node_t *foo, const char *lastval, int *line, int *hiteof) {
	if (lastval[0] == 'L') {
		const char *c = lastval + 1;
		char *strend;

		while (*c && isspace(*c))
			++c;

		if (!*c)
			return 1;

		foo->content.value = strtol(c, &strend, 0);

		return (strend == c); // Error if nothing could be read

		return 0;
	} else if (lastval[0] == 'B') {
		const char *c = lastval + 1;
		char *strend;

		while (*c && isspace(*c))
			++c;
		if (*c++ != '(') return 1;
		while (*c && isspace(*c))
			++c;

		foo->content.branches[0] = strtol(c, &strend, 0);
		if (strend == c)
			return 1;
		c = strend;

		while (*c && isspace(*c))
			++c;
		if (*c++ != ',')
			return 1;

		while (*c && isspace(*c))
			++c;

		foo->content.branches[1] = strtol(c, &strend, 0);
		if (strend == c)
			return 1;
		c = strend;

		while (*c && isspace(*c))
			++c;
		if (*c++ != ')')
			return 1;

		return 0;
	} else return 1; // failure to parse anything
}


void write_menubar_tp(Common::WriteStream *fh, menubar_t **foo);
int read_menubar_tp(Common::SeekableReadStream *fh, menubar_t **foo, const char *lastval, int *line, int *hiteof);

void write_mem_obj_tp(Common::WriteStream *fh, mem_obj_t **foo);
int read_mem_obj_tp(Common::SeekableReadStream *fh, mem_obj_t **foo, const char *lastval, int *line, int *hiteof);

void write_int_hash_map_tp(Common::WriteStream *fh, int_hash_map_t **foo);
int read_int_hash_map_tp(Common::SeekableReadStream *fh, int_hash_map_t **foo, const char *lastval, int *line, int *hiteof);

void write_songlib_t(Common::WriteStream *fh, songlib_t *foo);
int read_songlib_t(Common::SeekableReadStream *fh, songlib_t *foo, const char *lastval, int *line, int *hiteof);

void write_int_hash_map_node_tp(Common::WriteStream *fh, int_hash_map_t::node_t **foo);
int read_int_hash_map_node_tp(Common::SeekableReadStream *fh, int_hash_map_t::node_t **foo, const char *lastval, int *line, int *hiteof);

int read_song_tp(Common::SeekableReadStream *fh, song_t **foo, const char *lastval, int *line, int *hiteof);

typedef mem_obj_t *mem_obj_ptr;

// Unused types
/*
TYPE long "long" LIKE int;
TYPE gint16 "gint16" LIKE int;

RECORD synonym_t "synonym_t" {
	int replaceant;
	int replacement;
}
*/


// Auto-generated CFSML declaration and function block

#line 736 "engines/sci/engine/savegame.cfsml"
#define CFSML_SUCCESS 0
#define CFSML_FAILURE 1

#line 102 "engines/sci/engine/savegame.cfsml"

#include <stdarg.h> // We need va_lists
#include "sci/include/sci_memory.h"

#ifdef CFSML_DEBUG_MALLOC
/*
#define free(p)        dbg_sci_free(p)
#define malloc(s)      dbg_sci_malloc(s)
#define calloc(n, s)   dbg_sci_calloc(n, s)
#define realloc(p, s)  dbg_sci_realloc(p, s)
*/
#define free        dbg_sci_free
#define malloc      dbg_sci_malloc
#define calloc      dbg_sci_calloc
#define realloc     dbg_sci_realloc
#endif

static void _cfsml_error(const char *fmt, ...) {
	va_list argp;

	fprintf(stderr, "Error: ");
	va_start(argp, fmt);
	vfprintf(stderr, fmt, argp);
	va_end(argp);
}


static struct _cfsml_pointer_refstruct {
	struct _cfsml_pointer_refstruct *next;
	void *ptr;
} *_cfsml_pointer_references = NULL;

static struct _cfsml_pointer_refstruct **_cfsml_pointer_references_current = &_cfsml_pointer_references;

static char *_cfsml_last_value_retrieved = NULL;
static char *_cfsml_last_identifier_retrieved = NULL;

static void _cfsml_free_pointer_references_recursively(struct _cfsml_pointer_refstruct *refs, int free_pointers) {
	if (!refs)
		return;

	_cfsml_free_pointer_references_recursively(refs->next, free_pointers);

	if (free_pointers)
		free(refs->ptr);

	free(refs);
}

static void _cfsml_free_pointer_references(struct _cfsml_pointer_refstruct **meta_ref, int free_pointers) {
	_cfsml_free_pointer_references_recursively(*meta_ref, free_pointers);
	*meta_ref = NULL;
	_cfsml_pointer_references_current = meta_ref;
}

static struct _cfsml_pointer_refstruct **_cfsml_get_current_refpointer() {
	return _cfsml_pointer_references_current;
}

static void _cfsml_register_pointer(void *ptr) {
	struct _cfsml_pointer_refstruct *newref = (struct _cfsml_pointer_refstruct*)sci_malloc(sizeof (struct _cfsml_pointer_refstruct));
	newref->next = *_cfsml_pointer_references_current;
	newref->ptr = ptr;
	*_cfsml_pointer_references_current = newref;
}

static char *_cfsml_mangle_string(const char *s) {
	const char *source = s;
	char c;
	char *target = (char *)sci_malloc(1 + strlen(s) * 2); // We will probably need less than that
	char *writer = target;

	while ((c = *source++)) {
		if (c < 32) { // Special character?
			*writer++ = '\\'; // Escape...
			c += ('a' - 1);
		} else if (c == '\\' || c == '"')
			*writer++ = '\\'; // Escape, but do not change
		*writer++ = c;
	}
	*writer = 0; // Terminate string

	return (char *)sci_realloc(target, strlen(target) + 1);
}

static char *_cfsml_unmangle_string(const char *s, unsigned int length) {
	char *target = (char *)sci_malloc(1 + strlen(s));
	char *writer = target;
	const char *source = s;
	const char *end = s + length;
	char c;

	while ((source != end) && (c = *source++) && (c > 31)) {
		if (c == '\\') { // Escaped character?
    			c = *source++;
			if ((c != '\\') && (c != '"')) // Un-escape 0-31 only
				c -= ('a' - 1);
		}
		*writer++ = c;
	}
	*writer = 0; // Terminate string

	return (char *)sci_realloc(target, strlen(target) + 1);
}

static char *_cfsml_get_identifier(Common::SeekableReadStream *fd, int *line, int *hiteof, int *assignment) {
	int c;
	int mem = 32;
	int pos = 0;
	int done = 0;
	char *retval = (char *)sci_malloc(mem);

	if (_cfsml_last_identifier_retrieved) {
		free(_cfsml_last_identifier_retrieved);
		_cfsml_last_identifier_retrieved = NULL;
	}

	while (isspace(c = SRSgetc(fd)) && (c != EOF));
	if (c == EOF) {
	    _cfsml_error("Unexpected end of file at line %d\n", *line);
	    free(retval);
	    *hiteof = 1;
	    return NULL;
	}

	int first = 1;
	while ((first || (c = SRSgetc(fd)) != EOF) && ((pos == 0) || (c != '\n')) && (c != '=')) {
		first = 0;
		if (pos == mem - 1) // Need more memory?
			retval = (char *)sci_realloc(retval, mem *= 2);
		if (!isspace(c)) {
			if (done) {
				_cfsml_error("Single word identifier expected at line %d\n", *line);
				free(retval);
				return NULL;
			}
			retval[pos++] = c;
		} else
			if (pos != 0)
				done = 1; // Finished the variable name
			else if (c == '\n')
				++(*line);
	}

	if (c == EOF) {
		_cfsml_error("Unexpected end of file at line %d\n", *line);
		free(retval);
		*hiteof = 1;
		return NULL;
	}

	if (c == '\n') {
		++(*line);
		if (assignment)
			*assignment = 0;
	} else
		if (assignment)
			*assignment = 1;

	if (pos == 0) {
		_cfsml_error("Missing identifier in assignment at line %d\n", *line);
		free(retval);
		return NULL;
	}

	if (pos == mem - 1) // Need more memory?
		retval = (char *)sci_realloc(retval, mem += 1);

	retval[pos] = 0; // Terminate string
#line 281 "engines/sci/engine/savegame.cfsml"

	return _cfsml_last_identifier_retrieved = retval;
}

static char *_cfsml_get_value(Common::SeekableReadStream *fd, int *line, int *hiteof) {
	int c;
	int mem = 64;
	int pos = 0;
	char *retval = (char *)sci_malloc(mem);

	if (_cfsml_last_value_retrieved) {
		free(_cfsml_last_value_retrieved);
		_cfsml_last_value_retrieved = NULL;
	}

	while (((c = SRSgetc(fd)) != EOF) && (c != '\n')) {
		if (pos == mem - 1) // Need more memory?
			retval = (char *)sci_realloc(retval, mem *= 2);

		if (pos || (!isspace(c)))
			retval[pos++] = c;
	}

	while ((pos > 0) && (isspace(retval[pos - 1])))
		--pos; // Strip trailing whitespace

	if (c == EOF)
		*hiteof = 1;

	if (pos == 0) {
	    _cfsml_error("Missing value in assignment at line %d\n", *line);
	    free(retval);
	    return NULL;
	}

	if (c == '\n')
		++(*line);

	if (pos == mem - 1) // Need more memory?
		retval = (char *)sci_realloc(retval, mem += 1);

	retval[pos] = 0; // Terminate string
#line 333 "engines/sci/engine/savegame.cfsml"
	return (_cfsml_last_value_retrieved = (char *)sci_realloc(retval, strlen(retval) + 1));
	// Re-allocate; this value might be used for quite some while (if we are restoring a string)
}
#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_sfx_state_t(Common::WriteStream *fh, sfx_state_t* save_struc);
static int _cfsml_read_sfx_state_t(Common::SeekableReadStream *fh, sfx_state_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_clone_entry_t(Common::WriteStream *fh, clone_entry_t* save_struc);
static int _cfsml_read_clone_entry_t(Common::SeekableReadStream *fh, clone_entry_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_object_t(Common::WriteStream *fh, object_t* save_struc);
static int _cfsml_read_object_t(Common::SeekableReadStream *fh, object_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_string(Common::WriteStream *fh, char ** save_struc);
static int _cfsml_read_string(Common::SeekableReadStream *fh, char ** save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_menubar_t(Common::WriteStream *fh, menubar_t* save_struc);
static int _cfsml_read_menubar_t(Common::SeekableReadStream *fh, menubar_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_size_t(Common::WriteStream *fh, size_t* save_struc);
static int _cfsml_read_size_t(Common::SeekableReadStream *fh, size_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_list_entry_t(Common::WriteStream *fh, list_entry_t* save_struc);
static int _cfsml_read_list_entry_t(Common::SeekableReadStream *fh, list_entry_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_int_hash_map_t(Common::WriteStream *fh, int_hash_map_t* save_struc);
static int _cfsml_read_int_hash_map_t(Common::SeekableReadStream *fh, int_hash_map_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_SegManager(Common::WriteStream *fh, SegManager* save_struc);
static int _cfsml_read_SegManager(Common::SeekableReadStream *fh, SegManager* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_song_t(Common::WriteStream *fh, song_t* save_struc);
static int _cfsml_read_song_t(Common::SeekableReadStream *fh, song_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_menu_item_t(Common::WriteStream *fh, menu_item_t* save_struc);
static int _cfsml_read_menu_item_t(Common::SeekableReadStream *fh, menu_item_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_node_entry_t(Common::WriteStream *fh, node_entry_t* save_struc);
static int _cfsml_read_node_entry_t(Common::SeekableReadStream *fh, node_entry_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_seg_id_t(Common::WriteStream *fh, seg_id_t* save_struc);
static int _cfsml_read_seg_id_t(Common::SeekableReadStream *fh, seg_id_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_dynmem_t(Common::WriteStream *fh, dynmem_t* save_struc);
static int _cfsml_read_dynmem_t(Common::SeekableReadStream *fh, dynmem_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_local_variables_t(Common::WriteStream *fh, local_variables_t* save_struc);
static int _cfsml_read_local_variables_t(Common::SeekableReadStream *fh, local_variables_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_node_table_t(Common::WriteStream *fh, node_table_t* save_struc);
static int _cfsml_read_node_table_t(Common::SeekableReadStream *fh, node_table_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_sys_strings_t(Common::WriteStream *fh, sys_strings_t* save_struc);
static int _cfsml_read_sys_strings_t(Common::SeekableReadStream *fh, sys_strings_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_byte(Common::WriteStream *fh, byte* save_struc);
static int _cfsml_read_byte(Common::SeekableReadStream *fh, byte* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_node_t(Common::WriteStream *fh, node_t* save_struc);
static int _cfsml_read_node_t(Common::SeekableReadStream *fh, node_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_list_table_t(Common::WriteStream *fh, list_table_t* save_struc);
static int _cfsml_read_list_table_t(Common::SeekableReadStream *fh, list_table_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_class_t(Common::WriteStream *fh, class_t* save_struc);
static int _cfsml_read_class_t(Common::SeekableReadStream *fh, class_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_song_handle_t(Common::WriteStream *fh, song_handle_t* save_struc);
static int _cfsml_read_song_handle_t(Common::SeekableReadStream *fh, song_handle_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_int(Common::WriteStream *fh, int* save_struc);
static int _cfsml_read_int(Common::SeekableReadStream *fh, int* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_EngineState(Common::WriteStream *fh, EngineState* save_struc);
static int _cfsml_read_EngineState(Common::SeekableReadStream *fh, EngineState* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_SavegameMetadata(Common::WriteStream *fh, SavegameMetadata* save_struc);
static int _cfsml_read_SavegameMetadata(Common::SeekableReadStream *fh, SavegameMetadata* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_menu_t(Common::WriteStream *fh, menu_t* save_struc);
static int _cfsml_read_menu_t(Common::SeekableReadStream *fh, menu_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_clone_table_t(Common::WriteStream *fh, clone_table_t* save_struc);
static int _cfsml_read_clone_table_t(Common::SeekableReadStream *fh, clone_table_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_clone_t(Common::WriteStream *fh, clone_t* save_struc);
static int _cfsml_read_clone_t(Common::SeekableReadStream *fh, clone_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_list_t(Common::WriteStream *fh, list_t* save_struc);
static int _cfsml_read_list_t(Common::SeekableReadStream *fh, list_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_sys_string_t(Common::WriteStream *fh, sys_string_t* save_struc);
static int _cfsml_read_sys_string_t(Common::SeekableReadStream *fh, sys_string_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 383 "engines/sci/engine/savegame.cfsml"
static void _cfsml_write_script_t(Common::WriteStream *fh, script_t* save_struc);
static int _cfsml_read_script_t(Common::SeekableReadStream *fh, script_t* save_struc, const char *lastval, int *line, int *hiteof);

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_sfx_state_t(Common::WriteStream *fh, sfx_state_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "songlib = ");
	write_songlib_t(fh, (songlib_t*) &(save_struc->songlib));
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_sfx_state_t(Common::SeekableReadStream *fh, sfx_state_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record sfx_state_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "songlib")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_songlib_t(fh, (songlib_t*) &(save_struc->songlib), value, line, hiteof)) {
					_cfsml_error("Token expected by read_songlib_t() for songlib at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("sfx_state_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_clone_entry_t(Common::WriteStream *fh, clone_entry_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "next_free = ");
	_cfsml_write_int(fh, (int*) &(save_struc->next_free));
	WSprintf(fh, "\n");
	WSprintf(fh, "entry = ");
	_cfsml_write_clone_t(fh, (clone_t*) &(save_struc->entry));
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_clone_entry_t(Common::SeekableReadStream *fh, clone_entry_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record clone_entry_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "next_free")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->next_free), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for next_free at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "entry")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_clone_t(fh, (clone_t*) &(save_struc->entry), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_clone_t() for entry at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("clone_entry_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_object_t(Common::WriteStream *fh, object_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "flags = ");
	_cfsml_write_int(fh, (int*) &(save_struc->flags));
	WSprintf(fh, "\n");
	WSprintf(fh, "pos = ");
	write_reg_t(fh, (reg_t*) &(save_struc->pos));
	WSprintf(fh, "\n");
	WSprintf(fh, "variables_nr = ");
	_cfsml_write_int(fh, (int*) &(save_struc->variables_nr));
	WSprintf(fh, "\n");
	WSprintf(fh, "variable_names_nr = ");
	_cfsml_write_int(fh, (int*) &(save_struc->variable_names_nr));
	WSprintf(fh, "\n");
	WSprintf(fh, "methods_nr = ");
	_cfsml_write_int(fh, (int*) &(save_struc->methods_nr));
	WSprintf(fh, "\n");
	WSprintf(fh, "variables = ");
	int min, max;
	min = max = save_struc->variables_nr;
	if (!save_struc->variables)
		min = max = 0; /* Don't write if it points to NULL */
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		write_reg_t(fh, &(save_struc->variables[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_object_t(Common::SeekableReadStream *fh, object_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record object_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "flags")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->flags), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for flags at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "pos")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_reg_t(fh, (reg_t*) &(save_struc->pos), value, line, hiteof)) {
					_cfsml_error("Token expected by read_reg_t() for pos at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "variables_nr")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->variables_nr), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for variables_nr at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "variable_names_nr")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->variable_names_nr), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for variable_names_nr at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "methods_nr")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->methods_nr), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for methods_nr at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "variables")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
#line 615 "engines/sci/engine/savegame.cfsml"
			// Prepare to restore dynamic array
			max = strtol(value + 1, NULL, 0);
			if (max < 0) {
				_cfsml_error("Invalid number of elements to allocate for dynamic array '%s' at line %d\n", token, *line);
				return CFSML_FAILURE;
			}

			if (max) {
				save_struc->variables = (reg_t *)sci_malloc(max * sizeof(reg_t));
#ifdef SATISFY_PURIFY
				memset(save_struc->variables, 0, max * sizeof(reg_t));
#endif
				_cfsml_register_pointer(save_struc->variables);
			} else
				save_struc->variables = NULL;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (read_reg_t(fh, &(save_struc->variables[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by read_reg_t() for variables[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
		 	save_struc->variables_nr = max ; // Set array size accordingly
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("object_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_string(Common::WriteStream *fh, char ** save_struc)
{
#line 402 "engines/sci/engine/savegame.cfsml"
	if (!(*save_struc))
		WSprintf(fh, "\\null\\");
	else {
		char *token = _cfsml_mangle_string((const char *) *save_struc);
		WSprintf(fh, "\"%s\"", token);
		free(token);
	}
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_string(Common::SeekableReadStream *fh, char ** save_struc, const char *lastval, int *line, int *hiteof)
{
#line 519 "engines/sci/engine/savegame.cfsml"

	if (strcmp(lastval, "\\null\\")) { // null pointer?
		unsigned int length = strlen(lastval);
		if (*lastval == '"') { // Quoted string?
			while (lastval[length] != '"')
				--length;

			if (!length) { // No matching double-quotes?
				_cfsml_error("Unbalanced quotes at line %d\n", *line);
				return CFSML_FAILURE;
			}

			lastval++; // ...and skip the opening quotes locally
			length--;
		}
		*save_struc = _cfsml_unmangle_string(lastval, length);
		_cfsml_register_pointer(*save_struc);
		return CFSML_SUCCESS;
	} else {
		*save_struc = NULL;
		return CFSML_SUCCESS;
	}
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_menubar_t(Common::WriteStream *fh, menubar_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "menus = ");
	int min, max;
	min = max = save_struc->menus_nr;
	if (!save_struc->menus)
		min = max = 0; /* Don't write if it points to NULL */
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		_cfsml_write_menu_t(fh, &(save_struc->menus[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_menubar_t(Common::SeekableReadStream *fh, menubar_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record menubar_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "menus")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
#line 615 "engines/sci/engine/savegame.cfsml"
			// Prepare to restore dynamic array
			max = strtol(value + 1, NULL, 0);
			if (max < 0) {
				_cfsml_error("Invalid number of elements to allocate for dynamic array '%s' at line %d\n", token, *line);
				return CFSML_FAILURE;
			}

			if (max) {
				save_struc->menus = (menu_t *)sci_malloc(max * sizeof(menu_t));
#ifdef SATISFY_PURIFY
				memset(save_struc->menus, 0, max * sizeof(menu_t));
#endif
				_cfsml_register_pointer(save_struc->menus);
			} else
				save_struc->menus = NULL;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (_cfsml_read_menu_t(fh, &(save_struc->menus[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_menu_t() for menus[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
		 	save_struc->menus_nr = max ; // Set array size accordingly
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("menubar_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_size_t(Common::WriteStream *fh, size_t* save_struc)
{
	WSprintf(fh, "%li", (long)*save_struc);
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_size_t(Common::SeekableReadStream *fh, size_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 506 "engines/sci/engine/savegame.cfsml"
	char *token;

	*save_struc = strtol(lastval, &token, 0);
	if ((*save_struc == 0) && (token == lastval)) {
		_cfsml_error("strtol failed at line %d\n", *line);
		return CFSML_FAILURE;
	}
	if (*token != 0) {
		_cfsml_error("Non-integer encountered while parsing int value at line %d\n", *line);
		return CFSML_FAILURE;
	}
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_list_entry_t(Common::WriteStream *fh, list_entry_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "next_free = ");
	_cfsml_write_int(fh, (int*) &(save_struc->next_free));
	WSprintf(fh, "\n");
	WSprintf(fh, "entry = ");
	_cfsml_write_list_t(fh, (list_t*) &(save_struc->entry));
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_list_entry_t(Common::SeekableReadStream *fh, list_entry_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record list_entry_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "next_free")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->next_free), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for next_free at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "entry")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_list_t(fh, (list_t*) &(save_struc->entry), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_list_t() for entry at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("list_entry_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_int_hash_map_t(Common::WriteStream *fh, int_hash_map_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "base_value = ");
	_cfsml_write_int(fh, (int*) &(save_struc->base_value));
	WSprintf(fh, "\n");
	WSprintf(fh, "nodes = ");
	int min, max;
	min = max = DCS_INT_HASH_MAX+1;
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		write_int_hash_map_node_tp(fh, &(save_struc->nodes[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_int_hash_map_t(Common::SeekableReadStream *fh, int_hash_map_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record int_hash_map_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "base_value")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->base_value), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for base_value at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "nodes")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
			// Prepare to restore static array
			max = DCS_INT_HASH_MAX+1;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (read_int_hash_map_node_tp(fh, &(save_struc->nodes[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by read_int_hash_map_node_tp() for nodes[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("int_hash_map_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_SegManager(Common::WriteStream *fh, SegManager* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "id_seg_map = ");
	write_int_hash_map_tp(fh, (int_hash_map_t **) &(save_struc->id_seg_map));
	WSprintf(fh, "\n");
	WSprintf(fh, "heap = ");
	int min, max;
	min = max = save_struc->heap_size;
	if (!save_struc->heap)
		min = max = 0; /* Don't write if it points to NULL */
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		write_mem_obj_tp(fh, &(save_struc->heap[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "heap_size = ");
	_cfsml_write_int(fh, (int*) &(save_struc->heap_size));
	WSprintf(fh, "\n");
	WSprintf(fh, "reserved_id = ");
	_cfsml_write_int(fh, (int*) &(save_struc->reserved_id));
	WSprintf(fh, "\n");
	WSprintf(fh, "exports_wide = ");
	_cfsml_write_int(fh, (int*) &(save_struc->exports_wide));
	WSprintf(fh, "\n");
	WSprintf(fh, "sci1_1 = ");
	_cfsml_write_int(fh, (int*) &(save_struc->sci1_1));
	WSprintf(fh, "\n");
	WSprintf(fh, "gc_mark_bits = ");
	_cfsml_write_int(fh, (int*) &(save_struc->gc_mark_bits));
	WSprintf(fh, "\n");
	WSprintf(fh, "mem_allocated = ");
	_cfsml_write_size_t(fh, (size_t*) &(save_struc->mem_allocated));
	WSprintf(fh, "\n");
	WSprintf(fh, "clones_seg_id = ");
	_cfsml_write_seg_id_t(fh, (seg_id_t*) &(save_struc->clones_seg_id));
	WSprintf(fh, "\n");
	WSprintf(fh, "lists_seg_id = ");
	_cfsml_write_seg_id_t(fh, (seg_id_t*) &(save_struc->lists_seg_id));
	WSprintf(fh, "\n");
	WSprintf(fh, "nodes_seg_id = ");
	_cfsml_write_seg_id_t(fh, (seg_id_t*) &(save_struc->nodes_seg_id));
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_SegManager(Common::SeekableReadStream *fh, SegManager* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record SegManager; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "id_seg_map")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_int_hash_map_tp(fh, (int_hash_map_t **) &(save_struc->id_seg_map), value, line, hiteof)) {
					_cfsml_error("Token expected by read_int_hash_map_tp() for id_seg_map at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "heap")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
#line 615 "engines/sci/engine/savegame.cfsml"
			// Prepare to restore dynamic array
			max = strtol(value + 1, NULL, 0);
			if (max < 0) {
				_cfsml_error("Invalid number of elements to allocate for dynamic array '%s' at line %d\n", token, *line);
				return CFSML_FAILURE;
			}

			if (max) {
				save_struc->heap = (mem_obj_ptr *)sci_malloc(max * sizeof(mem_obj_ptr));
#ifdef SATISFY_PURIFY
				memset(save_struc->heap, 0, max * sizeof(mem_obj_ptr));
#endif
				_cfsml_register_pointer(save_struc->heap);
			} else
				save_struc->heap = NULL;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (read_mem_obj_tp(fh, &(save_struc->heap[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by read_mem_obj_tp() for heap[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
		 	save_struc->heap_size = max ; // Set array size accordingly
			} else
				if (!strcmp(token, "heap_size")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->heap_size), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for heap_size at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "reserved_id")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->reserved_id), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for reserved_id at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "exports_wide")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->exports_wide), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for exports_wide at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "sci1_1")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->sci1_1), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for sci1_1 at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "gc_mark_bits")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->gc_mark_bits), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for gc_mark_bits at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "mem_allocated")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_size_t(fh, (size_t*) &(save_struc->mem_allocated), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_size_t() for mem_allocated at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "clones_seg_id")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_seg_id_t(fh, (seg_id_t*) &(save_struc->clones_seg_id), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_seg_id_t() for clones_seg_id at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "lists_seg_id")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_seg_id_t(fh, (seg_id_t*) &(save_struc->lists_seg_id), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_seg_id_t() for lists_seg_id at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "nodes_seg_id")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_seg_id_t(fh, (seg_id_t*) &(save_struc->nodes_seg_id), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_seg_id_t() for nodes_seg_id at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("SegManager: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_song_t(Common::WriteStream *fh, song_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "handle = ");
	_cfsml_write_song_handle_t(fh, (song_handle_t*) &(save_struc->handle));
	WSprintf(fh, "\n");
	WSprintf(fh, "resource_num = ");
	_cfsml_write_int(fh, (int*) &(save_struc->resource_num));
	WSprintf(fh, "\n");
	WSprintf(fh, "priority = ");
	_cfsml_write_int(fh, (int*) &(save_struc->priority));
	WSprintf(fh, "\n");
	WSprintf(fh, "status = ");
	_cfsml_write_int(fh, (int*) &(save_struc->status));
	WSprintf(fh, "\n");
	WSprintf(fh, "restore_behavior = ");
	_cfsml_write_int(fh, (int*) &(save_struc->restore_behavior));
	WSprintf(fh, "\n");
	WSprintf(fh, "restore_time = ");
	_cfsml_write_int(fh, (int*) &(save_struc->restore_time));
	WSprintf(fh, "\n");
	WSprintf(fh, "loops = ");
	_cfsml_write_int(fh, (int*) &(save_struc->loops));
	WSprintf(fh, "\n");
	WSprintf(fh, "hold = ");
	_cfsml_write_int(fh, (int*) &(save_struc->hold));
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_song_t(Common::SeekableReadStream *fh, song_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record song_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "handle")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_song_handle_t(fh, (song_handle_t*) &(save_struc->handle), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_song_handle_t() for handle at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "resource_num")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->resource_num), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for resource_num at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "priority")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->priority), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for priority at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "status")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->status), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for status at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "restore_behavior")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->restore_behavior), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for restore_behavior at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "restore_time")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->restore_time), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for restore_time at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "loops")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->loops), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for loops at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "hold")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->hold), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for hold at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("song_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_menu_item_t(Common::WriteStream *fh, menu_item_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "type = ");
	_cfsml_write_int(fh, (int*) &(save_struc->type));
	WSprintf(fh, "\n");
	WSprintf(fh, "keytext = ");
	_cfsml_write_string(fh, (char **) &(save_struc->keytext));
	WSprintf(fh, "\n");
	WSprintf(fh, "keytext_size = ");
	_cfsml_write_int(fh, (int*) &(save_struc->keytext_size));
	WSprintf(fh, "\n");
	WSprintf(fh, "flags = ");
	_cfsml_write_int(fh, (int*) &(save_struc->flags));
	WSprintf(fh, "\n");
	WSprintf(fh, "said = ");
	int min, max;
	min = max = MENU_SAID_SPEC_SIZE;
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		_cfsml_write_byte(fh, &(save_struc->said[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "said_pos = ");
	write_reg_t(fh, (reg_t*) &(save_struc->said_pos));
	WSprintf(fh, "\n");
	WSprintf(fh, "text = ");
	_cfsml_write_string(fh, (char **) &(save_struc->text));
	WSprintf(fh, "\n");
	WSprintf(fh, "text_pos = ");
	write_reg_t(fh, (reg_t*) &(save_struc->text_pos));
	WSprintf(fh, "\n");
	WSprintf(fh, "modifiers = ");
	_cfsml_write_int(fh, (int*) &(save_struc->modifiers));
	WSprintf(fh, "\n");
	WSprintf(fh, "key = ");
	_cfsml_write_int(fh, (int*) &(save_struc->key));
	WSprintf(fh, "\n");
	WSprintf(fh, "enabled = ");
	_cfsml_write_int(fh, (int*) &(save_struc->enabled));
	WSprintf(fh, "\n");
	WSprintf(fh, "tag = ");
	_cfsml_write_int(fh, (int*) &(save_struc->tag));
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_menu_item_t(Common::SeekableReadStream *fh, menu_item_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record menu_item_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "type")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->type), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for type at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "keytext")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_string(fh, (char **) &(save_struc->keytext), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_string() for keytext at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "keytext_size")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->keytext_size), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for keytext_size at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "flags")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->flags), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for flags at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "said")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
			// Prepare to restore static array
			max = MENU_SAID_SPEC_SIZE;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (_cfsml_read_byte(fh, &(save_struc->said[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_byte() for said[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
			} else
				if (!strcmp(token, "said_pos")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_reg_t(fh, (reg_t*) &(save_struc->said_pos), value, line, hiteof)) {
					_cfsml_error("Token expected by read_reg_t() for said_pos at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "text")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_string(fh, (char **) &(save_struc->text), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_string() for text at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "text_pos")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_reg_t(fh, (reg_t*) &(save_struc->text_pos), value, line, hiteof)) {
					_cfsml_error("Token expected by read_reg_t() for text_pos at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "modifiers")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->modifiers), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for modifiers at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "key")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->key), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for key at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "enabled")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->enabled), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for enabled at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "tag")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->tag), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for tag at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("menu_item_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_node_entry_t(Common::WriteStream *fh, node_entry_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "next_free = ");
	_cfsml_write_int(fh, (int*) &(save_struc->next_free));
	WSprintf(fh, "\n");
	WSprintf(fh, "entry = ");
	_cfsml_write_node_t(fh, (node_t*) &(save_struc->entry));
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_node_entry_t(Common::SeekableReadStream *fh, node_entry_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record node_entry_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "next_free")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->next_free), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for next_free at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "entry")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_node_t(fh, (node_t*) &(save_struc->entry), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_node_t() for entry at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("node_entry_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_seg_id_t(Common::WriteStream *fh, seg_id_t* save_struc)
{
	WSprintf(fh, "%li", (long)*save_struc);
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_seg_id_t(Common::SeekableReadStream *fh, seg_id_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 506 "engines/sci/engine/savegame.cfsml"
	char *token;

	*save_struc = strtol(lastval, &token, 0);
	if ((*save_struc == 0) && (token == lastval)) {
		_cfsml_error("strtol failed at line %d\n", *line);
		return CFSML_FAILURE;
	}
	if (*token != 0) {
		_cfsml_error("Non-integer encountered while parsing int value at line %d\n", *line);
		return CFSML_FAILURE;
	}
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_dynmem_t(Common::WriteStream *fh, dynmem_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "size = ");
	_cfsml_write_int(fh, (int*) &(save_struc->size));
	WSprintf(fh, "\n");
	WSprintf(fh, "description = ");
	_cfsml_write_string(fh, (char **) &(save_struc->description));
	WSprintf(fh, "\n");
	WSprintf(fh, "buf = ");
	int min, max;
	min = max = save_struc->size;
	if (!save_struc->buf)
		min = max = 0; /* Don't write if it points to NULL */
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		_cfsml_write_byte(fh, &(save_struc->buf[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_dynmem_t(Common::SeekableReadStream *fh, dynmem_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record dynmem_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "size")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->size), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for size at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "description")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_string(fh, (char **) &(save_struc->description), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_string() for description at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "buf")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
#line 615 "engines/sci/engine/savegame.cfsml"
			// Prepare to restore dynamic array
			max = strtol(value + 1, NULL, 0);
			if (max < 0) {
				_cfsml_error("Invalid number of elements to allocate for dynamic array '%s' at line %d\n", token, *line);
				return CFSML_FAILURE;
			}

			if (max) {
				save_struc->buf = (byte *)sci_malloc(max * sizeof(byte));
#ifdef SATISFY_PURIFY
				memset(save_struc->buf, 0, max * sizeof(byte));
#endif
				_cfsml_register_pointer(save_struc->buf);
			} else
				save_struc->buf = NULL;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (_cfsml_read_byte(fh, &(save_struc->buf[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_byte() for buf[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
		 	save_struc->size = max ; // Set array size accordingly
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("dynmem_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_local_variables_t(Common::WriteStream *fh, local_variables_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "script_id = ");
	_cfsml_write_int(fh, (int*) &(save_struc->script_id));
	WSprintf(fh, "\n");
	WSprintf(fh, "nr = ");
	_cfsml_write_int(fh, (int*) &(save_struc->nr));
	WSprintf(fh, "\n");
	WSprintf(fh, "locals = ");
	int min, max;
	min = max = save_struc->nr;
	if (!save_struc->locals)
		min = max = 0; /* Don't write if it points to NULL */
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		write_reg_t(fh, &(save_struc->locals[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_local_variables_t(Common::SeekableReadStream *fh, local_variables_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record local_variables_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "script_id")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->script_id), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for script_id at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "nr")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->nr), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for nr at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "locals")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
#line 615 "engines/sci/engine/savegame.cfsml"
			// Prepare to restore dynamic array
			max = strtol(value + 1, NULL, 0);
			if (max < 0) {
				_cfsml_error("Invalid number of elements to allocate for dynamic array '%s' at line %d\n", token, *line);
				return CFSML_FAILURE;
			}

			if (max) {
				save_struc->locals = (reg_t *)sci_malloc(max * sizeof(reg_t));
#ifdef SATISFY_PURIFY
				memset(save_struc->locals, 0, max * sizeof(reg_t));
#endif
				_cfsml_register_pointer(save_struc->locals);
			} else
				save_struc->locals = NULL;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (read_reg_t(fh, &(save_struc->locals[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by read_reg_t() for locals[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
		 	save_struc->nr = max ; // Set array size accordingly
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("local_variables_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_node_table_t(Common::WriteStream *fh, node_table_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "entries_nr = ");
	_cfsml_write_int(fh, (int*) &(save_struc->entries_nr));
	WSprintf(fh, "\n");
	WSprintf(fh, "first_free = ");
	_cfsml_write_int(fh, (int*) &(save_struc->first_free));
	WSprintf(fh, "\n");
	WSprintf(fh, "entries_used = ");
	_cfsml_write_int(fh, (int*) &(save_struc->entries_used));
	WSprintf(fh, "\n");
	WSprintf(fh, "max_entry = ");
	_cfsml_write_int(fh, (int*) &(save_struc->max_entry));
	WSprintf(fh, "\n");
	WSprintf(fh, "table = ");
	int min, max;
	min = max = save_struc->entries_nr;
	if (!save_struc->table)
		min = max = 0; /* Don't write if it points to NULL */
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		_cfsml_write_node_entry_t(fh, &(save_struc->table[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_node_table_t(Common::SeekableReadStream *fh, node_table_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record node_table_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "entries_nr")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->entries_nr), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for entries_nr at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "first_free")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->first_free), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for first_free at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "entries_used")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->entries_used), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for entries_used at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "max_entry")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->max_entry), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for max_entry at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "table")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
#line 615 "engines/sci/engine/savegame.cfsml"
			// Prepare to restore dynamic array
			max = strtol(value + 1, NULL, 0);
			if (max < 0) {
				_cfsml_error("Invalid number of elements to allocate for dynamic array '%s' at line %d\n", token, *line);
				return CFSML_FAILURE;
			}

			if (max) {
				save_struc->table = (node_entry_t *)sci_malloc(max * sizeof(node_entry_t));
#ifdef SATISFY_PURIFY
				memset(save_struc->table, 0, max * sizeof(node_entry_t));
#endif
				_cfsml_register_pointer(save_struc->table);
			} else
				save_struc->table = NULL;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (_cfsml_read_node_entry_t(fh, &(save_struc->table[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_node_entry_t() for table[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
		 	save_struc->entries_nr = max ; // Set array size accordingly
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("node_table_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_sys_strings_t(Common::WriteStream *fh, sys_strings_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "strings = ");
	int min, max;
	min = max = SYS_STRINGS_MAX;
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		_cfsml_write_sys_string_t(fh, &(save_struc->strings[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_sys_strings_t(Common::SeekableReadStream *fh, sys_strings_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record sys_strings_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "strings")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
			// Prepare to restore static array
			max = SYS_STRINGS_MAX;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (_cfsml_read_sys_string_t(fh, &(save_struc->strings[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_sys_string_t() for strings[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("sys_strings_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_byte(Common::WriteStream *fh, byte* save_struc)
{
	WSprintf(fh, "%li", (long)*save_struc);
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_byte(Common::SeekableReadStream *fh, byte* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 506 "engines/sci/engine/savegame.cfsml"
	char *token;

	*save_struc = strtol(lastval, &token, 0);
	if ((*save_struc == 0) && (token == lastval)) {
		_cfsml_error("strtol failed at line %d\n", *line);
		return CFSML_FAILURE;
	}
	if (*token != 0) {
		_cfsml_error("Non-integer encountered while parsing int value at line %d\n", *line);
		return CFSML_FAILURE;
	}
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_node_t(Common::WriteStream *fh, node_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "pred = ");
	write_reg_t(fh, (reg_t*) &(save_struc->pred));
	WSprintf(fh, "\n");
	WSprintf(fh, "succ = ");
	write_reg_t(fh, (reg_t*) &(save_struc->succ));
	WSprintf(fh, "\n");
	WSprintf(fh, "key = ");
	write_reg_t(fh, (reg_t*) &(save_struc->key));
	WSprintf(fh, "\n");
	WSprintf(fh, "value = ");
	write_reg_t(fh, (reg_t*) &(save_struc->value));
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_node_t(Common::SeekableReadStream *fh, node_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record node_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "pred")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_reg_t(fh, (reg_t*) &(save_struc->pred), value, line, hiteof)) {
					_cfsml_error("Token expected by read_reg_t() for pred at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "succ")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_reg_t(fh, (reg_t*) &(save_struc->succ), value, line, hiteof)) {
					_cfsml_error("Token expected by read_reg_t() for succ at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "key")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_reg_t(fh, (reg_t*) &(save_struc->key), value, line, hiteof)) {
					_cfsml_error("Token expected by read_reg_t() for key at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "value")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_reg_t(fh, (reg_t*) &(save_struc->value), value, line, hiteof)) {
					_cfsml_error("Token expected by read_reg_t() for value at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("node_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_list_table_t(Common::WriteStream *fh, list_table_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "entries_nr = ");
	_cfsml_write_int(fh, (int*) &(save_struc->entries_nr));
	WSprintf(fh, "\n");
	WSprintf(fh, "first_free = ");
	_cfsml_write_int(fh, (int*) &(save_struc->first_free));
	WSprintf(fh, "\n");
	WSprintf(fh, "entries_used = ");
	_cfsml_write_int(fh, (int*) &(save_struc->entries_used));
	WSprintf(fh, "\n");
	WSprintf(fh, "max_entry = ");
	_cfsml_write_int(fh, (int*) &(save_struc->max_entry));
	WSprintf(fh, "\n");
	WSprintf(fh, "table = ");
	int min, max;
	min = max = save_struc->entries_nr;
	if (!save_struc->table)
		min = max = 0; /* Don't write if it points to NULL */
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		_cfsml_write_list_entry_t(fh, &(save_struc->table[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_list_table_t(Common::SeekableReadStream *fh, list_table_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record list_table_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "entries_nr")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->entries_nr), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for entries_nr at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "first_free")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->first_free), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for first_free at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "entries_used")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->entries_used), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for entries_used at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "max_entry")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->max_entry), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for max_entry at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "table")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
#line 615 "engines/sci/engine/savegame.cfsml"
			// Prepare to restore dynamic array
			max = strtol(value + 1, NULL, 0);
			if (max < 0) {
				_cfsml_error("Invalid number of elements to allocate for dynamic array '%s' at line %d\n", token, *line);
				return CFSML_FAILURE;
			}

			if (max) {
				save_struc->table = (list_entry_t *)sci_malloc(max * sizeof(list_entry_t));
#ifdef SATISFY_PURIFY
				memset(save_struc->table, 0, max * sizeof(list_entry_t));
#endif
				_cfsml_register_pointer(save_struc->table);
			} else
				save_struc->table = NULL;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (_cfsml_read_list_entry_t(fh, &(save_struc->table[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_list_entry_t() for table[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
		 	save_struc->entries_nr = max ; // Set array size accordingly
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("list_table_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_class_t(Common::WriteStream *fh, class_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "script = ");
	_cfsml_write_int(fh, (int*) &(save_struc->script));
	WSprintf(fh, "\n");
	WSprintf(fh, "reg = ");
	write_reg_t(fh, (reg_t*) &(save_struc->reg));
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_class_t(Common::SeekableReadStream *fh, class_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record class_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "script")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->script), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for script at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "reg")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_reg_t(fh, (reg_t*) &(save_struc->reg), value, line, hiteof)) {
					_cfsml_error("Token expected by read_reg_t() for reg at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("class_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_song_handle_t(Common::WriteStream *fh, song_handle_t* save_struc)
{
	WSprintf(fh, "%li", (long)*save_struc);
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_song_handle_t(Common::SeekableReadStream *fh, song_handle_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 506 "engines/sci/engine/savegame.cfsml"
	char *token;

	*save_struc = strtol(lastval, &token, 0);
	if ((*save_struc == 0) && (token == lastval)) {
		_cfsml_error("strtol failed at line %d\n", *line);
		return CFSML_FAILURE;
	}
	if (*token != 0) {
		_cfsml_error("Non-integer encountered while parsing int value at line %d\n", *line);
		return CFSML_FAILURE;
	}
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_int(Common::WriteStream *fh, int* save_struc)
{
	WSprintf(fh, "%li", (long)*save_struc);
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_int(Common::SeekableReadStream *fh, int* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 506 "engines/sci/engine/savegame.cfsml"
	char *token;

	*save_struc = strtol(lastval, &token, 0);
	if ((*save_struc == 0) && (token == lastval)) {
		_cfsml_error("strtol failed at line %d\n", *line);
		return CFSML_FAILURE;
	}
	if (*token != 0) {
		_cfsml_error("Non-integer encountered while parsing int value at line %d\n", *line);
		return CFSML_FAILURE;
	}
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_EngineState(Common::WriteStream *fh, EngineState* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "savegame_version = ");
	_cfsml_write_int(fh, (int*) &(save_struc->savegame_version));
	WSprintf(fh, "\n");
	WSprintf(fh, "game_version = ");
	_cfsml_write_string(fh, (char **) &(save_struc->game_version));
	WSprintf(fh, "\n");
	WSprintf(fh, "version = ");
	write_sci_version(fh, (sci_version_t*) &(save_struc->version));
	WSprintf(fh, "\n");
	WSprintf(fh, "menubar = ");
	write_menubar_tp(fh, (menubar_t **) &(save_struc->menubar));
	WSprintf(fh, "\n");
	WSprintf(fh, "status_bar_foreground = ");
	_cfsml_write_int(fh, (int*) &(save_struc->status_bar_foreground));
	WSprintf(fh, "\n");
	WSprintf(fh, "status_bar_background = ");
	_cfsml_write_int(fh, (int*) &(save_struc->status_bar_background));
	WSprintf(fh, "\n");
	WSprintf(fh, "seg_manager = ");
	_cfsml_write_SegManager(fh, (SegManager*) &(save_struc->seg_manager));
	WSprintf(fh, "\n");
	WSprintf(fh, "classtable_size = ");
	_cfsml_write_int(fh, (int*) &(save_struc->classtable_size));
	WSprintf(fh, "\n");
	WSprintf(fh, "classtable = ");
	int min, max;
	min = max = save_struc->classtable_size;
	if (!save_struc->classtable)
		min = max = 0; /* Don't write if it points to NULL */
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		_cfsml_write_class_t(fh, &(save_struc->classtable[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "sound = ");
	_cfsml_write_sfx_state_t(fh, (sfx_state_t*) &(save_struc->sound));
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_EngineState(Common::SeekableReadStream *fh, EngineState* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record EngineState; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "savegame_version")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->savegame_version), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for savegame_version at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "game_version")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_string(fh, (char **) &(save_struc->game_version), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_string() for game_version at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "version")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_sci_version(fh, (sci_version_t*) &(save_struc->version), value, line, hiteof)) {
					_cfsml_error("Token expected by read_sci_version() for version at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "menubar")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_menubar_tp(fh, (menubar_t **) &(save_struc->menubar), value, line, hiteof)) {
					_cfsml_error("Token expected by read_menubar_tp() for menubar at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "status_bar_foreground")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->status_bar_foreground), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for status_bar_foreground at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "status_bar_background")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->status_bar_background), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for status_bar_background at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "seg_manager")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_SegManager(fh, (SegManager*) &(save_struc->seg_manager), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_SegManager() for seg_manager at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "classtable_size")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->classtable_size), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for classtable_size at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "classtable")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
#line 615 "engines/sci/engine/savegame.cfsml"
			// Prepare to restore dynamic array
			max = strtol(value + 1, NULL, 0);
			if (max < 0) {
				_cfsml_error("Invalid number of elements to allocate for dynamic array '%s' at line %d\n", token, *line);
				return CFSML_FAILURE;
			}

			if (max) {
				save_struc->classtable = (class_t *)sci_malloc(max * sizeof(class_t));
#ifdef SATISFY_PURIFY
				memset(save_struc->classtable, 0, max * sizeof(class_t));
#endif
				_cfsml_register_pointer(save_struc->classtable);
			} else
				save_struc->classtable = NULL;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (_cfsml_read_class_t(fh, &(save_struc->classtable[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_class_t() for classtable[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
		 	save_struc->classtable_size = max ; // Set array size accordingly
			} else
				if (!strcmp(token, "sound")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_sfx_state_t(fh, (sfx_state_t*) &(save_struc->sound), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_sfx_state_t() for sound at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("EngineState: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_SavegameMetadata(Common::WriteStream *fh, SavegameMetadata* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "savegame_name = ");
	_cfsml_write_string(fh, (char **) &(save_struc->savegame_name));
	WSprintf(fh, "\n");
	WSprintf(fh, "savegame_version = ");
	_cfsml_write_int(fh, (int*) &(save_struc->savegame_version));
	WSprintf(fh, "\n");
	WSprintf(fh, "game_version = ");
	_cfsml_write_string(fh, (char **) &(save_struc->game_version));
	WSprintf(fh, "\n");
	WSprintf(fh, "version = ");
	write_sci_version(fh, (sci_version_t*) &(save_struc->version));
	WSprintf(fh, "\n");
	WSprintf(fh, "savegame_date = ");
	_cfsml_write_int(fh, (int*) &(save_struc->savegame_date));
	WSprintf(fh, "\n");
	WSprintf(fh, "savegame_time = ");
	_cfsml_write_int(fh, (int*) &(save_struc->savegame_time));
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_SavegameMetadata(Common::SeekableReadStream *fh, SavegameMetadata* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record SavegameMetadata; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "savegame_name")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_string(fh, (char **) &(save_struc->savegame_name), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_string() for savegame_name at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "savegame_version")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->savegame_version), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for savegame_version at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "game_version")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_string(fh, (char **) &(save_struc->game_version), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_string() for game_version at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "version")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_sci_version(fh, (sci_version_t*) &(save_struc->version), value, line, hiteof)) {
					_cfsml_error("Token expected by read_sci_version() for version at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "savegame_date")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->savegame_date), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for savegame_date at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "savegame_time")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->savegame_time), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for savegame_time at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("SavegameMetadata: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_menu_t(Common::WriteStream *fh, menu_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "title = ");
	_cfsml_write_string(fh, (char **) &(save_struc->title));
	WSprintf(fh, "\n");
	WSprintf(fh, "title_width = ");
	_cfsml_write_int(fh, (int*) &(save_struc->title_width));
	WSprintf(fh, "\n");
	WSprintf(fh, "width = ");
	_cfsml_write_int(fh, (int*) &(save_struc->width));
	WSprintf(fh, "\n");
	WSprintf(fh, "items = ");
	int min, max;
	min = max = save_struc->items_nr;
	if (!save_struc->items)
		min = max = 0; /* Don't write if it points to NULL */
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		_cfsml_write_menu_item_t(fh, &(save_struc->items[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_menu_t(Common::SeekableReadStream *fh, menu_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record menu_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "title")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_string(fh, (char **) &(save_struc->title), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_string() for title at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "title_width")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->title_width), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for title_width at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "width")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->width), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for width at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "items")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
#line 615 "engines/sci/engine/savegame.cfsml"
			// Prepare to restore dynamic array
			max = strtol(value + 1, NULL, 0);
			if (max < 0) {
				_cfsml_error("Invalid number of elements to allocate for dynamic array '%s' at line %d\n", token, *line);
				return CFSML_FAILURE;
			}

			if (max) {
				save_struc->items = (menu_item_t *)sci_malloc(max * sizeof(menu_item_t));
#ifdef SATISFY_PURIFY
				memset(save_struc->items, 0, max * sizeof(menu_item_t));
#endif
				_cfsml_register_pointer(save_struc->items);
			} else
				save_struc->items = NULL;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (_cfsml_read_menu_item_t(fh, &(save_struc->items[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_menu_item_t() for items[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
		 	save_struc->items_nr = max ; // Set array size accordingly
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("menu_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_clone_table_t(Common::WriteStream *fh, clone_table_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "entries_nr = ");
	_cfsml_write_int(fh, (int*) &(save_struc->entries_nr));
	WSprintf(fh, "\n");
	WSprintf(fh, "first_free = ");
	_cfsml_write_int(fh, (int*) &(save_struc->first_free));
	WSprintf(fh, "\n");
	WSprintf(fh, "entries_used = ");
	_cfsml_write_int(fh, (int*) &(save_struc->entries_used));
	WSprintf(fh, "\n");
	WSprintf(fh, "max_entry = ");
	_cfsml_write_int(fh, (int*) &(save_struc->max_entry));
	WSprintf(fh, "\n");
	WSprintf(fh, "table = ");
	int min, max;
	min = max = save_struc->entries_nr;
	if (!save_struc->table)
		min = max = 0; /* Don't write if it points to NULL */
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		_cfsml_write_clone_entry_t(fh, &(save_struc->table[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_clone_table_t(Common::SeekableReadStream *fh, clone_table_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record clone_table_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "entries_nr")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->entries_nr), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for entries_nr at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "first_free")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->first_free), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for first_free at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "entries_used")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->entries_used), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for entries_used at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "max_entry")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->max_entry), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for max_entry at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "table")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
#line 615 "engines/sci/engine/savegame.cfsml"
			// Prepare to restore dynamic array
			max = strtol(value + 1, NULL, 0);
			if (max < 0) {
				_cfsml_error("Invalid number of elements to allocate for dynamic array '%s' at line %d\n", token, *line);
				return CFSML_FAILURE;
			}

			if (max) {
				save_struc->table = (clone_entry_t *)sci_malloc(max * sizeof(clone_entry_t));
#ifdef SATISFY_PURIFY
				memset(save_struc->table, 0, max * sizeof(clone_entry_t));
#endif
				_cfsml_register_pointer(save_struc->table);
			} else
				save_struc->table = NULL;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (_cfsml_read_clone_entry_t(fh, &(save_struc->table[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_clone_entry_t() for table[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
		 	save_struc->entries_nr = max ; // Set array size accordingly
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("clone_table_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_clone_t(Common::WriteStream *fh, clone_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "flags = ");
	_cfsml_write_int(fh, (int*) &(save_struc->flags));
	WSprintf(fh, "\n");
	WSprintf(fh, "pos = ");
	write_reg_t(fh, (reg_t*) &(save_struc->pos));
	WSprintf(fh, "\n");
	WSprintf(fh, "variables_nr = ");
	_cfsml_write_int(fh, (int*) &(save_struc->variables_nr));
	WSprintf(fh, "\n");
	WSprintf(fh, "variable_names_nr = ");
	_cfsml_write_int(fh, (int*) &(save_struc->variable_names_nr));
	WSprintf(fh, "\n");
	WSprintf(fh, "methods_nr = ");
	_cfsml_write_int(fh, (int*) &(save_struc->methods_nr));
	WSprintf(fh, "\n");
	WSprintf(fh, "variables = ");
	int min, max;
	min = max = save_struc->variables_nr;
	if (!save_struc->variables)
		min = max = 0; /* Don't write if it points to NULL */
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		write_reg_t(fh, &(save_struc->variables[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_clone_t(Common::SeekableReadStream *fh, clone_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record clone_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "flags")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->flags), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for flags at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "pos")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_reg_t(fh, (reg_t*) &(save_struc->pos), value, line, hiteof)) {
					_cfsml_error("Token expected by read_reg_t() for pos at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "variables_nr")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->variables_nr), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for variables_nr at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "variable_names_nr")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->variable_names_nr), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for variable_names_nr at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "methods_nr")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->methods_nr), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for methods_nr at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "variables")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
#line 615 "engines/sci/engine/savegame.cfsml"
			// Prepare to restore dynamic array
			max = strtol(value + 1, NULL, 0);
			if (max < 0) {
				_cfsml_error("Invalid number of elements to allocate for dynamic array '%s' at line %d\n", token, *line);
				return CFSML_FAILURE;
			}

			if (max) {
				save_struc->variables = (reg_t *)sci_malloc(max * sizeof(reg_t));
#ifdef SATISFY_PURIFY
				memset(save_struc->variables, 0, max * sizeof(reg_t));
#endif
				_cfsml_register_pointer(save_struc->variables);
			} else
				save_struc->variables = NULL;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (read_reg_t(fh, &(save_struc->variables[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by read_reg_t() for variables[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
		 	save_struc->variables_nr = max ; // Set array size accordingly
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("clone_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_list_t(Common::WriteStream *fh, list_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "first = ");
	write_reg_t(fh, (reg_t*) &(save_struc->first));
	WSprintf(fh, "\n");
	WSprintf(fh, "last = ");
	write_reg_t(fh, (reg_t*) &(save_struc->last));
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_list_t(Common::SeekableReadStream *fh, list_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record list_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "first")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_reg_t(fh, (reg_t*) &(save_struc->first), value, line, hiteof)) {
					_cfsml_error("Token expected by read_reg_t() for first at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "last")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_reg_t(fh, (reg_t*) &(save_struc->last), value, line, hiteof)) {
					_cfsml_error("Token expected by read_reg_t() for last at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("list_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_sys_string_t(Common::WriteStream *fh, sys_string_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "name = ");
	_cfsml_write_string(fh, (char **) &(save_struc->name));
	WSprintf(fh, "\n");
	WSprintf(fh, "max_size = ");
	_cfsml_write_int(fh, (int*) &(save_struc->max_size));
	WSprintf(fh, "\n");
	WSprintf(fh, "value = ");
	_cfsml_write_string(fh, (char **) &(save_struc->value));
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_sys_string_t(Common::SeekableReadStream *fh, sys_string_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record sys_string_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "name")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_string(fh, (char **) &(save_struc->name), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_string() for name at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "max_size")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->max_size), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for max_size at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "value")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_string(fh, (char **) &(save_struc->value), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_string() for value at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("sys_string_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}

#line 395 "engines/sci/engine/savegame.cfsml"
static void
_cfsml_write_script_t(Common::WriteStream *fh, script_t* save_struc)
{
#line 412 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "{\n");
	WSprintf(fh, "nr = ");
	_cfsml_write_int(fh, (int*) &(save_struc->nr));
	WSprintf(fh, "\n");
	WSprintf(fh, "buf_size = ");
	_cfsml_write_size_t(fh, (size_t*) &(save_struc->buf_size));
	WSprintf(fh, "\n");
	WSprintf(fh, "script_size = ");
	_cfsml_write_size_t(fh, (size_t*) &(save_struc->script_size));
	WSprintf(fh, "\n");
	WSprintf(fh, "heap_size = ");
	_cfsml_write_size_t(fh, (size_t*) &(save_struc->heap_size));
	WSprintf(fh, "\n");
	WSprintf(fh, "obj_indices = ");
	write_int_hash_map_tp(fh, (int_hash_map_t **) &(save_struc->obj_indices));
	WSprintf(fh, "\n");
	WSprintf(fh, "exports_nr = ");
	_cfsml_write_int(fh, (int*) &(save_struc->exports_nr));
	WSprintf(fh, "\n");
	WSprintf(fh, "synonyms_nr = ");
	_cfsml_write_int(fh, (int*) &(save_struc->synonyms_nr));
	WSprintf(fh, "\n");
	WSprintf(fh, "lockers = ");
	_cfsml_write_int(fh, (int*) &(save_struc->lockers));
	WSprintf(fh, "\n");
	WSprintf(fh, "objects_allocated = ");
	_cfsml_write_int(fh, (int*) &(save_struc->objects_allocated));
	WSprintf(fh, "\n");
	WSprintf(fh, "objects_nr = ");
	_cfsml_write_int(fh, (int*) &(save_struc->objects_nr));
	WSprintf(fh, "\n");
	WSprintf(fh, "objects = ");
	int min, max;
	min = max = save_struc->objects_allocated;
	if (!save_struc->objects)
		min = max = 0; /* Don't write if it points to NULL */
#line 439 "engines/sci/engine/savegame.cfsml"
	WSprintf(fh, "[%d][\n", max);
	for (int i = 0; i < min; i++) {
		_cfsml_write_object_t(fh, &(save_struc->objects[i]));
		WSprintf(fh, "\n");
	}
	WSprintf(fh, "]");
	WSprintf(fh, "\n");
	WSprintf(fh, "locals_offset = ");
	_cfsml_write_int(fh, (int*) &(save_struc->locals_offset));
	WSprintf(fh, "\n");
	WSprintf(fh, "locals_segment = ");
	_cfsml_write_int(fh, (int*) &(save_struc->locals_segment));
	WSprintf(fh, "\n");
	WSprintf(fh, "marked_as_deleted = ");
	_cfsml_write_int(fh, (int*) &(save_struc->marked_as_deleted));
	WSprintf(fh, "\n");
	WSprintf(fh, "}");
}

#line 486 "engines/sci/engine/savegame.cfsml"
static int
_cfsml_read_script_t(Common::SeekableReadStream *fh, script_t* save_struc, const char *lastval, int *line, int *hiteof)
{
#line 541 "engines/sci/engine/savegame.cfsml"
	char *token;
	int assignment, closed;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Reading record script_t; expected opening braces in line %d, got \"%s\"\n", *line, lastval);
		return CFSML_FAILURE;
	};
	closed = 0;
	do {
		const char *value;
		token = _cfsml_get_identifier(fh, line, hiteof, &assignment);

		if (!token) {
			_cfsml_error("Expected token at line %d\n", *line);
			return CFSML_FAILURE;
		}
		if (!assignment) {
			if (!strcmp(token, "}")) 
				closed = 1;
			else {
				_cfsml_error("Expected assignment or closing braces in line %d\n", *line);
				return CFSML_FAILURE;
			}
		} else {
			value = "";
			while (!value || !strcmp(value, ""))
				value = _cfsml_get_value(fh, line, hiteof);
			if (!value) {
				_cfsml_error("Expected token at line %d\n", *line);
				return CFSML_FAILURE;
			}
				if (!strcmp(token, "nr")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->nr), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for nr at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "buf_size")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_size_t(fh, (size_t*) &(save_struc->buf_size), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_size_t() for buf_size at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "script_size")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_size_t(fh, (size_t*) &(save_struc->script_size), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_size_t() for script_size at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "heap_size")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_size_t(fh, (size_t*) &(save_struc->heap_size), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_size_t() for heap_size at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "obj_indices")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (read_int_hash_map_tp(fh, (int_hash_map_t **) &(save_struc->obj_indices), value, line, hiteof)) {
					_cfsml_error("Token expected by read_int_hash_map_tp() for obj_indices at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "exports_nr")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->exports_nr), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for exports_nr at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "synonyms_nr")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->synonyms_nr), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for synonyms_nr at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "lockers")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->lockers), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for lockers at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "objects_allocated")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->objects_allocated), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for objects_allocated at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "objects_nr")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->objects_nr), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for objects_nr at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "objects")) {
#line 604 "engines/sci/engine/savegame.cfsml"
			if ((value[0] != '[') || (value[strlen(value) - 1] != '[')) {
				_cfsml_error("Opening brackets expected at line %d\n", *line);
				return CFSML_FAILURE;
			}
			int max,done,i;
#line 615 "engines/sci/engine/savegame.cfsml"
			// Prepare to restore dynamic array
			max = strtol(value + 1, NULL, 0);
			if (max < 0) {
				_cfsml_error("Invalid number of elements to allocate for dynamic array '%s' at line %d\n", token, *line);
				return CFSML_FAILURE;
			}

			if (max) {
				save_struc->objects = (object_t *)sci_malloc(max * sizeof(object_t));
#ifdef SATISFY_PURIFY
				memset(save_struc->objects, 0, max * sizeof(object_t));
#endif
				_cfsml_register_pointer(save_struc->objects);
			} else
				save_struc->objects = NULL;
#line 639 "engines/sci/engine/savegame.cfsml"
			done = i = 0;
			do {
			if (!(value = _cfsml_get_identifier(fh, line, hiteof, NULL))) {
#line 647 "engines/sci/engine/savegame.cfsml"
				_cfsml_error("Token expected at line %d\n", *line);
				return 1;
			}
			if (strcmp(value, "]")) {
				if (i == max) {
					_cfsml_error("More elements than space available (%d) in '%s' at line %d\n", max, token, *line);
					return CFSML_FAILURE;
				}
				if (_cfsml_read_object_t(fh, &(save_struc->objects[i++]), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_object_t() for objects[i++] at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else 
				done = 1;
			} while (!done);
		 	save_struc->objects_allocated = max ; // Set array size accordingly
			} else
				if (!strcmp(token, "locals_offset")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->locals_offset), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for locals_offset at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "locals_segment")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->locals_segment), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for locals_segment at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
				if (!strcmp(token, "marked_as_deleted")) {
#line 690 "engines/sci/engine/savegame.cfsml"
				if (_cfsml_read_int(fh, (int*) &(save_struc->marked_as_deleted), value, line, hiteof)) {
					_cfsml_error("Token expected by _cfsml_read_int() for marked_as_deleted at line %d\n", *line);
					return CFSML_FAILURE;
				}
			} else
#line 699 "engines/sci/engine/savegame.cfsml"
			{
				_cfsml_error("script_t: Assignment to invalid identifier '%s' in line %d\n", token, *line);
				return CFSML_FAILURE;
			}
		}
	} while (!closed); // Until closing braces are hit
	return CFSML_SUCCESS;
}


// Auto-generated CFSML declaration and function block ends here
// Auto-generation performed by cfsml.pl 0.8.2 
#line 446 "engines/sci/engine/savegame.cfsml"

void write_songlib_t(Common::WriteStream *fh, songlib_t *songlib) {
	song_t *seeker = *(songlib->lib);
	int songcount = song_lib_count(*songlib);

	WSprintf(fh, "{\n");
	WSprintf(fh, "songcount = %d\n", songcount);
	WSprintf(fh, "list = \n");
	WSprintf(fh, "[\n");
	while (seeker) {
		seeker->restore_time = seeker->it->get_timepos(seeker->it);
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_song_t(fh, seeker);
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 458 "engines/sci/engine/savegame.cfsml"
		seeker = seeker->next;
	}
	WSprintf(fh, "]\n");
	WSprintf(fh, "}\n");
}

int read_songlib_t(Common::SeekableReadStream *fh, songlib_t *songlib, const char *lastval, int *line, int *hiteof) {
	int songcount;
	int i;
	song_t *newsong;

	if (strcmp(lastval, "{")) {
		_cfsml_error("Opening brackets expected at line %d\n", *line);
		return CFSML_FAILURE;
	}
	// FIXME: error checking
	Common::String l = fh->readLine();
	sscanf(l.c_str(), "songcount = %d", &songcount);
	l = fh->readLine(); // "list = "
	l = fh->readLine(); // "["
	*line += 4;
	song_lib_init(songlib);
	for (i = 0; i < songcount; i++) {
// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 778 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = lastval;
#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = read_song_tp(fh, &newsong, _cfsml_inp, &(*line), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		*hiteof = _cfsml_error;
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 482 "engines/sci/engine/savegame.cfsml"
		song_lib_add(*songlib, newsong);
	}  
	l = fh->readLine(); // "]"
	l = fh->readLine(); // "}"
	*line += 2;
	return 0;
}

static struct {
	int type;
	const char *name;
} mem_obj_string_names[] = {
	{MEM_OBJ_INVALID, "INVALID"},
	{MEM_OBJ_SCRIPT, "SCRIPT"},
	{MEM_OBJ_CLONES, "CLONES"},
	{MEM_OBJ_LOCALS, "LOCALS"},
	{MEM_OBJ_STACK, "STACK"},
	{MEM_OBJ_SYS_STRINGS,"SYS_STRINGS"},
	{MEM_OBJ_LISTS,"LISTS"},
	{MEM_OBJ_NODES,"NODES"},
	{MEM_OBJ_HUNK,"HUNK"},
	{MEM_OBJ_DYNMEM,"DYNMEM"}};

int mem_obj_string_to_enum(const char *str) {
	int i;

	for (i = 0; i <= MEM_OBJ_MAX; i++) {
		if (!scumm_stricmp(mem_obj_string_names[i].name, str))
			return i;
	}

	return -1;
}

void write_int_hash_map_tp(Common::WriteStream *fh, int_hash_map_t **foo) {
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_int_hash_map_t(fh, *foo);
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 518 "engines/sci/engine/savegame.cfsml"
}

void write_song_tp(Common::WriteStream *fh, song_t **foo) {
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_song_t(fh, *foo);
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 522 "engines/sci/engine/savegame.cfsml"
}

song_iterator_t *build_iterator(EngineState *s, int song_nr, int type, songit_id_t id);

int read_song_tp(Common::SeekableReadStream *fh, song_t **foo, const char *lastval, int *line, int *hiteof) {
	char *token;
	int assignment;
	*foo = (song_t*) malloc(sizeof(song_t));
	token = _cfsml_get_identifier(fh, line, hiteof, &assignment);
// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 778 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = token;
#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_song_t(fh, (*foo), _cfsml_inp, &(*line), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		*hiteof = _cfsml_error;
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 532 "engines/sci/engine/savegame.cfsml"
	(*foo)->delay = 0;
	(*foo)->it = NULL;
	(*foo)->next_playing = (*foo)->next_stopping = (*foo)->next = NULL;
	return 0;
}

int read_int_hash_map_tp(Common::SeekableReadStream *fh, int_hash_map_t **foo, const char *lastval, int *line, int *hiteof) {
	*foo = new int_hash_map_t;
// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 778 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = lastval;
#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_int_hash_map_t(fh, (*foo), _cfsml_inp, &(*line), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		*hiteof = _cfsml_error;
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 541 "engines/sci/engine/savegame.cfsml"
	(*foo)->holes = NULL;
	return 0;
}

void write_int_hash_map_node_tp(Common::WriteStream *fh, int_hash_map_t::node_t **foo) {
	if (!(*foo)) {
		WSprintf(fh, "\\null");
	} else {
		WSprintf(fh,"[\n%d=>%d\n", (*foo)->name, (*foo)->value);
		if ((*foo)->next) {
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	write_int_hash_map_node_tp(fh, &((*foo)->next));
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 552 "engines/sci/engine/savegame.cfsml"
		} else
			WSprintf(fh, "L");
		WSprintf(fh, "]");
	}
}

int read_int_hash_map_node_tp(Common::SeekableReadStream *fh, int_hash_map_t::node_t **foo, const char *lastval, int *line, int *hiteof) {
	static char buffer[80];

	if (lastval[0] == '\\') {
		*foo = NULL; // No hash map node
	} else {
		*foo = (int_hash_map_t::node_t*)malloc(sizeof(int_hash_map_t::node_t));
		if (lastval[0] != '[') {
			sciprintf("Expected opening bracket in hash_map_node_t on line %d\n", *line);
			return 1;
		}
		
		do {
			(*line)++;
			SRSgets(buffer, 80, fh);
			if (buffer[0] == 'L') {
				(*foo)->next = NULL;
				buffer[0] = buffer[1];
			} // HACK: deliberately no else clause here
			if (buffer[0] == ']')  {
				break;
			}
			else if (buffer[0] == '[') {
				if (read_int_hash_map_node_tp(fh, &((*foo)->next), buffer, line, hiteof))
					return 1;
			}
			else if (sscanf(buffer, "%d=>%d", &((*foo)->name), &((*foo)->value))<2) {
				sciprintf("Error parsing hash_map_node_t on line %d\n", *line);
				return 1;
			}
		} while (1);
	}

	return 0;
}

void write_menubar_tp(Common::WriteStream *fh, menubar_t **foo) {
	if (*foo) {
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_menubar_t(fh, (*foo));
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 597 "engines/sci/engine/savegame.cfsml"
	} else { // Nothing to write
		WSprintf(fh, "\\null\\");
	}
}


int read_menubar_tp(Common::SeekableReadStream *fh, menubar_t **foo, const char *lastval, int *line, int *hiteof) {
	if (lastval[0] == '\\') {
		*foo = NULL; // No menu bar
	} else {
		*foo = (menubar_t *) sci_malloc(sizeof(menubar_t));
// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 778 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = lastval;
#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_menubar_t(fh, (*foo), _cfsml_inp, &(*line), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		*hiteof = _cfsml_error;
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 609 "engines/sci/engine/savegame.cfsml"
	}
	return *hiteof;
}

void write_mem_obj_t(Common::WriteStream *fh, mem_obj_t *foo) {
	WSprintf(fh, "%s\n", mem_obj_string_names[foo->type].name);	
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_int(fh, &foo->segmgr_id);
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 616 "engines/sci/engine/savegame.cfsml"
	switch (foo->type) {
	case MEM_OBJ_SCRIPT:
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_script_t(fh, &foo->data.script);
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 619 "engines/sci/engine/savegame.cfsml"
	break;
	case MEM_OBJ_CLONES:
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_clone_table_t(fh, &foo->data.clones);
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 622 "engines/sci/engine/savegame.cfsml"
	break;
	case MEM_OBJ_LOCALS:
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_local_variables_t(fh, &foo->data.locals);
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 625 "engines/sci/engine/savegame.cfsml"
	break;
	case MEM_OBJ_SYS_STRINGS:
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_sys_strings_t(fh, &foo->data.sys_strings);
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 628 "engines/sci/engine/savegame.cfsml"
	break;
	case MEM_OBJ_STACK:
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_int(fh, &foo->data.stack.nr);
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 631 "engines/sci/engine/savegame.cfsml"
	break;
	case MEM_OBJ_HUNK:
		break;
	case MEM_OBJ_LISTS:	
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_list_table_t(fh, &foo->data.lists);
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 636 "engines/sci/engine/savegame.cfsml"
	break;
	case MEM_OBJ_NODES:	
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_node_table_t(fh, &foo->data.nodes);
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 639 "engines/sci/engine/savegame.cfsml"
	break;
	case MEM_OBJ_DYNMEM:
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_dynmem_t(fh, &foo->data.dynmem);
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 642 "engines/sci/engine/savegame.cfsml"
	break;
	}
}

int read_mem_obj_t(Common::SeekableReadStream *fh, mem_obj_t *foo, const char *lastval, int *line, int *hiteof) {
	foo->type = mem_obj_string_to_enum(lastval);
	if (foo->type < 0) {
		sciprintf("Unknown mem_obj_t type %s on line %d\n", lastval, *line);
		return 1;
	}

// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 781 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = _cfsml_get_identifier(fh, &(*line), &_cfsml_eof, 0);

#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_int(fh, &foo->segmgr_id, _cfsml_inp, &(*line), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		*hiteof = _cfsml_error;
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 654 "engines/sci/engine/savegame.cfsml"
	switch (foo->type) {
	case MEM_OBJ_SCRIPT:
// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 781 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = _cfsml_get_identifier(fh, &(*line), &_cfsml_eof, 0);

#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_script_t(fh, &foo->data.script, _cfsml_inp, &(*line), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		*hiteof = _cfsml_error;
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 657 "engines/sci/engine/savegame.cfsml"
	break;
	case MEM_OBJ_CLONES:
// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 781 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = _cfsml_get_identifier(fh, &(*line), &_cfsml_eof, 0);

#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_clone_table_t(fh, &foo->data.clones, _cfsml_inp, &(*line), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		*hiteof = _cfsml_error;
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 660 "engines/sci/engine/savegame.cfsml"
	break;
	case MEM_OBJ_LOCALS:
// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 781 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = _cfsml_get_identifier(fh, &(*line), &_cfsml_eof, 0);

#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_local_variables_t(fh, &foo->data.locals, _cfsml_inp, &(*line), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		*hiteof = _cfsml_error;
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 663 "engines/sci/engine/savegame.cfsml"
	break;
	case MEM_OBJ_SYS_STRINGS:
// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 781 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = _cfsml_get_identifier(fh, &(*line), &_cfsml_eof, 0);

#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_sys_strings_t(fh, &foo->data.sys_strings, _cfsml_inp, &(*line), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		*hiteof = _cfsml_error;
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 666 "engines/sci/engine/savegame.cfsml"
	break;
	case MEM_OBJ_LISTS:
// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 781 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = _cfsml_get_identifier(fh, &(*line), &_cfsml_eof, 0);

#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_list_table_t(fh, &foo->data.lists, _cfsml_inp, &(*line), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		*hiteof = _cfsml_error;
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 669 "engines/sci/engine/savegame.cfsml"
	break;
	case MEM_OBJ_NODES:
// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 781 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = _cfsml_get_identifier(fh, &(*line), &_cfsml_eof, 0);

#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_node_table_t(fh, &foo->data.nodes, _cfsml_inp, &(*line), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		*hiteof = _cfsml_error;
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 672 "engines/sci/engine/savegame.cfsml"
	break;
	case MEM_OBJ_STACK:
// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 781 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = _cfsml_get_identifier(fh, &(*line), &_cfsml_eof, 0);

#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_int(fh, &foo->data.stack.nr, _cfsml_inp, &(*line), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		*hiteof = _cfsml_error;
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 675 "engines/sci/engine/savegame.cfsml"
	foo->data.stack.entries = (reg_t *)sci_calloc(foo->data.stack.nr, sizeof(reg_t));
	break;
	case MEM_OBJ_HUNK:
		init_hunk_table(&foo->data.hunks);
		break;
	case MEM_OBJ_DYNMEM:
// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 781 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = _cfsml_get_identifier(fh, &(*line), &_cfsml_eof, 0);

#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_dynmem_t(fh, &foo->data.dynmem, _cfsml_inp, &(*line), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		*hiteof = _cfsml_error;
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 682 "engines/sci/engine/savegame.cfsml"
	break;
	}

	return *hiteof;
}

void write_mem_obj_tp(Common::WriteStream *fh, mem_obj_t **foo) {
	if (*foo) {
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	write_mem_obj_t(fh, (*foo));
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 691 "engines/sci/engine/savegame.cfsml"
	} else { // Nothing to write
		WSprintf(fh, "\\null\\");
	}
}

int read_mem_obj_tp(Common::SeekableReadStream *fh, mem_obj_t **foo, const char *lastval, int *line, int *hiteof) {
	if (lastval[0] == '\\') {
		*foo = NULL; // No menu bar
	} else {
		*foo = (mem_obj_t *)sci_malloc(sizeof(mem_obj_t));
// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 778 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = lastval;
#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = read_mem_obj_t(fh, (*foo), _cfsml_inp, &(*line), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		*hiteof = _cfsml_error;
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 702 "engines/sci/engine/savegame.cfsml"
		return *hiteof;
	}
	return 0;
}


// This function is called to undo some strange stuff done in preparation
// to writing a gamestate to disk
void _gamestate_unfrob(EngineState *s) {
}


int gamestate_save(EngineState *s, Common::WriteStream *fh, const char* savename) {
	tm curTime;
	g_system->getTimeAndDate(curTime);

	SavegameMetadata *meta = new SavegameMetadata;
	meta->savegame_version = FREESCI_CURRENT_SAVEGAME_VERSION;
	meta->savegame_name = savename;
	meta->version = s->version;
	meta->game_version = s->game_version;
	meta->savegame_date = ((curTime.tm_mday & 0xFF) << 24) | (((curTime.tm_mon + 1) & 0xFF) << 16) | ((curTime.tm_year + 1900) & 0xFFFF);
	meta->savegame_time = ((curTime.tm_hour & 0xFF) << 16) | (((curTime.tm_min) & 0xFF) << 8) | ((curTime.tm_sec) & 0xFF);
	fprintf(stderr, "date/time: %d %d\n", meta->savegame_date, meta->savegame_time);

	_global_save_state = s;
	s->savegame_version = FREESCI_CURRENT_SAVEGAME_VERSION;
	s->dyn_views_list_serial = (s->dyn_views)? s->dyn_views->serial : -2;
	s->drop_views_list_serial = (s->drop_views)? s->drop_views->serial : -2;
	s->port_serial = (s->port)? s->port->serial : -2;

	if (s->execution_stack_base) {
		sciprintf("Cannot save from below kernel function\n");
		return 1;
	}

/*
	if (s->sound_server) {
		if ((s->sound_server->save)(s, dirname)) {
			sciprintf("Saving failed for the sound subsystem\n");
			chdir("..");
			return 1;
		}
	}
*/
	// Calculate the time spent with this game
	s->game_time = g_system->getMillis() / 1000;

#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_SavegameMetadata(fh, meta);
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 751 "engines/sci/engine/savegame.cfsml"
#line 814 "engines/sci/engine/savegame.cfsml"
// Auto-generated CFSML data writer code
	_cfsml_write_EngineState(fh, s);
	WSprintf(fh, "\n");
// End of auto-generated CFSML data writer code
#line 752 "engines/sci/engine/savegame.cfsml"

	delete meta;

	_gamestate_unfrob(s);

	return 0;
}

static seg_id_t find_unique_seg_by_type(SegManager *self, int type) {
	int i;

	for (i = 0; i < self->heap_size; i++)
		if (self->heap[i] &&
		    self->heap[i]->type == type)
			return i;
	return -1;
}

static byte *find_unique_script_block(EngineState *s, byte *buf, int type) {
	int magic_pos_adder = s->version >= SCI_VERSION_FTU_NEW_SCRIPT_HEADER ? 0 : 2;

	buf += magic_pos_adder;
	do {
		int seeker_type = getUInt16(buf);
		int seeker_size;

		if (seeker_type == 0) break;
		if (seeker_type == type) return buf;

		seeker_size = getUInt16(buf + 2);
		buf += seeker_size;
	} while(1);

	return NULL;
}

static void reconstruct_stack(EngineState *retval) {
	seg_id_t stack_seg = find_unique_seg_by_type(&retval->seg_manager, MEM_OBJ_STACK);
	dstack_t *stack = &(retval->seg_manager.heap[stack_seg]->data.stack);

	retval->stack_segment = stack_seg;
	retval->stack_base = stack->entries;
	retval->stack_top = retval->stack_base + VM_STACK_SIZE;
}

static int clone_entry_used(clone_table_t *table, int n) {
	int backup;
	int seeker = table->first_free;
	clone_entry_t *entries = table->table;

	if (seeker == HEAPENTRY_INVALID) return 1;

	do {
		if (seeker == n) return 0;
		backup = seeker;
		seeker = entries[seeker].next_free;
	} while (entries[backup].next_free != HEAPENTRY_INVALID);

	return 1;
}

static void load_script(EngineState *s, seg_id_t seg) {
	resource_t *script, *heap = NULL;
	script_t *scr = &(s->seg_manager.heap[seg]->data.script);

	scr->buf = (byte *)malloc(scr->buf_size);

	script = scir_find_resource(s->resmgr, sci_script, scr->nr, 0);
	if (s->version >= SCI_VERSION(1,001,000))
		heap = scir_find_resource(s->resmgr, sci_heap, scr->nr, 0);

	switch (s->seg_manager.sci1_1) {
	case 0 :
		sm_mcpy_in_out(&s->seg_manager, 0, script->data, script->size, seg, SEG_ID);
		break;
	case 1 :
		sm_mcpy_in_out(&s->seg_manager, 0, script->data, script->size, seg, SEG_ID);
		sm_mcpy_in_out(&s->seg_manager, scr->script_size, heap->data, heap->size, seg, SEG_ID);
		break;
	}
}

static void reconstruct_scripts(EngineState *s, SegManager *self) {
	int i;
	mem_obj_t *mobj;
	for (i = 0; i < self->heap_size; i++) {
		if (self->heap[i]) {
			mobj = self->heap[i];
			switch (mobj->type)  {
			case MEM_OBJ_SCRIPT: {
				int j;
				script_t *scr = &mobj->data.script;

				load_script(s, i);
				scr->locals_block = scr->locals_segment == 0 ? NULL : &s->seg_manager.heap[scr->locals_segment]->data.locals;
				scr->export_table = (guint16 *) find_unique_script_block(s, scr->buf, sci_obj_exports);
				scr->synonyms = find_unique_script_block(s, scr->buf, sci_obj_synonyms);
				scr->code = NULL;
				scr->code_blocks_nr = 0;
				scr->code_blocks_allocated = 0;

				if (!self->sci1_1)
					scr->export_table += 3;
				
				for (j = 0; j < scr->objects_nr; j++) {
					byte *data = scr->buf + scr->objects[j].pos.offset;
					scr->objects[j].base = scr->buf;
					scr->objects[j].base_obj = data;
				}

			}
			}
		}
	}

	for (i = 0; i < self->heap_size; i++) {
		if (self->heap[i]) {
			mobj = self->heap[i];
			switch (mobj->type)  {
			case MEM_OBJ_SCRIPT: {
				int j;
				script_t *scr = &mobj->data.script;

				for (j = 0; j < scr->objects_nr; j++) {
					byte *data = scr->buf + scr->objects[j].pos.offset;

					if (self->sci1_1) {
						guint16 *funct_area = (guint16 *) (scr->buf + getUInt16( data + 6 ));
						guint16 *prop_area = (guint16 *) (scr->buf + getUInt16( data + 4 ));

						scr->objects[j].base_method = funct_area;
						scr->objects[j].base_vars = prop_area;
					} else {
						int funct_area = getUInt16( data + SCRIPT_FUNCTAREAPTR_OFFSET );
						object_t *base_obj;

						base_obj = obj_get(s, scr->objects[j].variables[SCRIPT_SPECIES_SELECTOR]);

						if (!base_obj) {
							sciprintf("Object without a base class: Script %d, index %d (reg address "PREG"\n",
								  scr->nr, j, PRINT_REG(scr->objects[j].variables[SCRIPT_SPECIES_SELECTOR]));
							continue;
						}
						scr->objects[j].variable_names_nr = base_obj->variables_nr;
						scr->objects[j].base_obj = base_obj->base_obj;

						scr->objects[j].base_method = (guint16 *) (data + funct_area);
						scr->objects[j].base_vars = (guint16 *) (data + scr->objects[j].variable_names_nr * 2 + SCRIPT_SELECTOR_OFFSET);
					}
				}
			}
			}
		}
	}
}

void reconstruct_clones(EngineState *s, SegManager *self) {
	int i;
	mem_obj_t *mobj;

	for (i = 0; i < self->heap_size; i++) {
		if (self->heap[i]) {
			mobj = self->heap[i];
			switch (mobj->type) {
			case MEM_OBJ_CLONES: {
				int j;
				clone_entry_t *seeker = mobj->data.clones.table;
				
				sciprintf("Free list: ");
				for (j = mobj->data.clones.first_free; j != HEAPENTRY_INVALID; j = mobj->data.clones.table[j].next_free) {
					sciprintf("%d ", j);
				}
				sciprintf("\n");

				sciprintf("Entries w/zero vars: ");
				for (j = 0; j < mobj->data.clones.max_entry; j++) {
					if (mobj->data.clones.table[j].entry.variables == NULL)
						sciprintf("%d ", j);
				}
				sciprintf("\n");

				for (j = 0; j < mobj->data.clones.max_entry; j++) {
 					object_t *base_obj;

					if (!clone_entry_used(&mobj->data.clones, j)) {
						seeker++;
						continue;
					}
					base_obj = obj_get(s, seeker->entry.variables[SCRIPT_SPECIES_SELECTOR]);
					if (!base_obj) {
						sciprintf("Clone entry without a base class: %d\n", j);
						seeker->entry.base = seeker->entry.base_obj = NULL;
						seeker->entry.base_vars = seeker->entry.base_method = NULL;
						continue;
					}
					seeker->entry.base = base_obj->base;
					seeker->entry.base_obj = base_obj->base_obj;
					seeker->entry.base_vars = base_obj->base_vars;
					seeker->entry.base_method = base_obj->base_method;

					seeker++;
				}

				break;
			}
			}
		}
	}
}

int _reset_graphics_input(EngineState *s);

song_iterator_t *new_fast_forward_iterator(song_iterator_t *it, int delta);

static void reconstruct_sounds(EngineState *s) {
	song_t *seeker;
	int it_type = s->resmgr->sci_version >= SCI_VERSION_01 ? SCI_SONG_ITERATOR_TYPE_SCI1 : SCI_SONG_ITERATOR_TYPE_SCI0;

	if (s->sound.songlib.lib)
		seeker = *(s->sound.songlib.lib);
	else {
		song_lib_init(&s->sound.songlib);
		seeker = NULL;
	}

	while (seeker) {
		song_iterator_t *base, *ff;
		int oldstatus;
		song_iterator_message_t msg;

		base = ff = build_iterator(s, seeker->resource_num, it_type, seeker->handle);
		if (seeker->restore_behavior == RESTORE_BEHAVIOR_CONTINUE)
			ff = (song_iterator_t *)new_fast_forward_iterator(base, seeker->restore_time);
		ff->init(ff);

		msg = songit_make_message(seeker->handle, SIMSG_SET_LOOPS(seeker->loops));
		songit_handle_message(&ff, msg);
		msg = songit_make_message(seeker->handle, SIMSG_SET_HOLD(seeker->hold));
		songit_handle_message(&ff, msg);

		oldstatus = seeker->status;
		seeker->status = SOUND_STATUS_STOPPED;
		seeker->it = ff;
		sfx_song_set_status(&s->sound, seeker->handle, oldstatus);
		seeker = seeker->next;
	}
}

EngineState *gamestate_restore(EngineState *s, Common::SeekableReadStream *fh) {
	int read_eof = 0;
	EngineState *retval;
	songlib_t temp;

/*
	if (s->sound_server) {
		if ((s->sound_server->restore)(s, dirname)) {
			sciprintf("Restoring failed for the sound subsystem\n");
			return NULL;
		}
	}
*/

	retval = (EngineState *) sci_malloc(sizeof(EngineState));

	memset(retval, 0, sizeof(EngineState));

	retval->savegame_version = -1;
	_global_save_state = retval;
	retval->gfx_state = s->gfx_state;

	SavegameMetadata* meta = new SavegameMetadata;
	memset(retval, 0, sizeof(SavegameMetadata));

// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 766 "engines/sci/engine/savegame.cfsml"
		int _cfsml_line_ctr = 0;
#line 771 "engines/sci/engine/savegame.cfsml"
		struct _cfsml_pointer_refstruct **_cfsml_myptrrefptr = _cfsml_get_current_refpointer();
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 781 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = _cfsml_get_identifier(fh, &(_cfsml_line_ctr), &_cfsml_eof, 0);

#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_SavegameMetadata(fh, meta, _cfsml_inp, &(_cfsml_line_ctr), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		read_eof = _cfsml_error;
#line 793 "engines/sci/engine/savegame.cfsml"
		_cfsml_free_pointer_references(_cfsml_myptrrefptr, _cfsml_error);
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 1026 "engines/sci/engine/savegame.cfsml"
	if ((meta->savegame_version < FREESCI_MINIMUM_SAVEGAME_VERSION) ||
	    (meta->savegame_version > FREESCI_CURRENT_SAVEGAME_VERSION)) {
		if (meta->savegame_version < FREESCI_MINIMUM_SAVEGAME_VERSION)
			sciprintf("Old savegame version detected- can't load\n");
		else
			sciprintf("Savegame version is %d- maximum supported is %0d\n", meta->savegame_version, FREESCI_CURRENT_SAVEGAME_VERSION);

		delete meta;
		return NULL;
	}

	delete meta;

	// Backwards compatibility settings
	retval->dyn_views = NULL;
	retval->drop_views = NULL;
	retval->port = NULL;
	retval->save_dir_copy_buf = NULL;

	retval->sound_mute = s->sound_mute;
	retval->sound_volume = s->sound_volume;

// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 766 "engines/sci/engine/savegame.cfsml"
		int _cfsml_line_ctr = 0;
#line 771 "engines/sci/engine/savegame.cfsml"
		struct _cfsml_pointer_refstruct **_cfsml_myptrrefptr = _cfsml_get_current_refpointer();
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 781 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = _cfsml_get_identifier(fh, &(_cfsml_line_ctr), &_cfsml_eof, 0);

#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_EngineState(fh, retval, _cfsml_inp, &(_cfsml_line_ctr), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		read_eof = _cfsml_error;
#line 793 "engines/sci/engine/savegame.cfsml"
		_cfsml_free_pointer_references(_cfsml_myptrrefptr, _cfsml_error);
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 1049 "engines/sci/engine/savegame.cfsml"

	sfx_exit(&s->sound);
	_gamestate_unfrob(retval);

	// Set exec stack base to zero
	retval->execution_stack_base = 0;
	retval->execution_stack_pos = 0;

	// Now copy all current state information
	// Graphics and input state:
	retval->animation_delay = s->animation_delay;
	retval->animation_granularity = s->animation_granularity;
	retval->gfx_state = s->gfx_state;

	retval->resmgr = s->resmgr;

	temp = retval->sound.songlib;
	sfx_init(&retval->sound, retval->resmgr, s->sfx_init_flags);
	retval->sfx_init_flags = s->sfx_init_flags;
	song_lib_free(retval->sound.songlib);
	retval->sound.songlib = temp;

	_reset_graphics_input(retval);
	reconstruct_stack(retval);
	reconstruct_scripts(retval, &retval->seg_manager);
	reconstruct_clones(retval, &retval->seg_manager);
	retval->game_obj = s->game_obj;
	retval->script_000 = &retval->seg_manager.heap[script_get_segment(s, 0, SCRIPT_GET_DONT_LOAD)]->data.script;
	retval->gc_countdown = GC_INTERVAL - 1;
	retval->save_dir_copy = make_reg(s->sys_strings_segment, SYS_STRING_SAVEDIR);
	retval->save_dir_edit_offset = 0;
	retval->sys_strings_segment = find_unique_seg_by_type(&retval->seg_manager, MEM_OBJ_SYS_STRINGS);
	retval->sys_strings = &(((mem_obj_t *)(GET_SEGMENT(retval->seg_manager, retval->sys_strings_segment, MEM_OBJ_SYS_STRINGS)))->data.sys_strings);
	sys_strings_restore(retval->sys_strings, s->sys_strings);

	// Time state:
	retval->last_wait_time = g_system->getMillis();

	// File IO state:
	retval->file_handles_nr = 2;
	retval->file_handles = (FILE **)sci_calloc(2, sizeof(FILE *));

	// static parser information:
	retval->parser_rules = s->parser_rules;
	retval->parser_words_nr = s->parser_words_nr;
	retval->parser_words = s->parser_words;
	retval->parser_suffices_nr = s->parser_suffices_nr;
	retval->parser_suffices = s->parser_suffices;
	retval->parser_branches_nr = s->parser_branches_nr;
	retval->parser_branches = s->parser_branches;

	// static VM/Kernel information:
	retval->selector_names_nr = s->selector_names_nr;
	retval->selector_names = s->selector_names;
	retval->kernel_names_nr = s->kernel_names_nr;
	retval->kernel_names = s->kernel_names;
	retval->kfunct_table = s->kfunct_table;
	retval->kfunct_nr = s->kfunct_nr;
	retval->opcodes = s->opcodes;

	memcpy(&(retval->selector_map), &(s->selector_map), sizeof(selector_map_t));

	retval->max_version = retval->version;
	retval->min_version = retval->version;
	retval->parser_base = make_reg(s->sys_strings_segment, SYS_STRING_PARSER_BASE);

	// Copy breakpoint information from current game instance
	retval->have_bp = s->have_bp;
	retval->bp_list = s->bp_list;

	retval->debug_mode = s->debug_mode;

	retval->resource_dir = s->resource_dir;
	retval->work_dir = s->work_dir;
	retval->kernel_opt_flags = 0;
	retval->have_mouse_flag = 1;

	retval->successor = NULL;
	retval->pic_priority_table = (int*)gfxop_get_pic_metainfo(retval->gfx_state);
	retval->game_name = sci_strdup(obj_get_name(retval, retval->game_obj));

	retval->sound.it = NULL;
	retval->sound.flags = s->sound.flags;
	retval->sound.song = NULL;
	retval->sound.suspended = s->sound.suspended;
	retval->sound.debug = s->sound.debug;
	reconstruct_sounds(retval);

	return retval;
}

bool get_savegame_metadata(Common::SeekableReadStream* stream, SavegameMetadata* meta) {
	int read_eof = 0;

// Auto-generated CFSML data reader code
#line 763 "engines/sci/engine/savegame.cfsml"
	{
#line 766 "engines/sci/engine/savegame.cfsml"
		int _cfsml_line_ctr = 0;
#line 771 "engines/sci/engine/savegame.cfsml"
		struct _cfsml_pointer_refstruct **_cfsml_myptrrefptr = _cfsml_get_current_refpointer();
#line 774 "engines/sci/engine/savegame.cfsml"
		int _cfsml_eof = 0, _cfsml_error;
#line 781 "engines/sci/engine/savegame.cfsml"
		const char *_cfsml_inp = _cfsml_get_identifier(stream, &(_cfsml_line_ctr), &_cfsml_eof, 0);

#line 785 "engines/sci/engine/savegame.cfsml"
		_cfsml_error = _cfsml_read_SavegameMetadata(stream, meta, _cfsml_inp, &(_cfsml_line_ctr), &_cfsml_eof);
#line 789 "engines/sci/engine/savegame.cfsml"
		read_eof = _cfsml_error;
#line 793 "engines/sci/engine/savegame.cfsml"
		_cfsml_free_pointer_references(_cfsml_myptrrefptr, _cfsml_error);
#line 796 "engines/sci/engine/savegame.cfsml"
		if (_cfsml_last_value_retrieved) {
			free(_cfsml_last_value_retrieved);
			_cfsml_last_value_retrieved = NULL;
		}
		if (_cfsml_last_identifier_retrieved) {
			free(_cfsml_last_identifier_retrieved);
			_cfsml_last_identifier_retrieved = NULL;
		}
	}
// End of auto-generated CFSML data reader code
#line 1144 "engines/sci/engine/savegame.cfsml"

	if (read_eof)
		return false;

	if ((meta->savegame_version < FREESCI_MINIMUM_SAVEGAME_VERSION) ||
	    (meta->savegame_version > FREESCI_CURRENT_SAVEGAME_VERSION)) {
		if (meta->savegame_version < FREESCI_MINIMUM_SAVEGAME_VERSION)
			sciprintf("Old savegame version detected- can't load\n");
		else
			sciprintf("Savegame version is %d- maximum supported is %0d\n", meta->savegame_version, FREESCI_CURRENT_SAVEGAME_VERSION);

		return false;
	}

	return true;
}

} // End of namespace Sci
