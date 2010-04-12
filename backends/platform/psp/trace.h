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


#ifndef TRACE_H
#define TRACE_H

#include <stdio.h>
#include <psptypes.h>
#include <stdarg.h>

// Use these defines for debugging

//#define __PSP_PRINT_TO_FILE__
//#define __PSP_PRINT_TO_FILE_AND_SCREEN__
//#define __PSP_DEBUG_FUNCS__	/* can put this locally too */
//#define __PSP_DEBUG_PRINT__

void PSPDebugTrace (bool alsoToScreen, const char *format, ...);

#ifndef TRACE_C
extern int psp_debug_indent;
#endif

#endif /* TRACE_H */

// From here on, we allow multiple definitions
#undef __PSP_PRINT__
#undef PSP_ERROR
#undef __PSP_INDENT__
#undef PSP_INFO_PRINT
#undef PSP_INFO_PRINT_INDENT
#undef PSP_DEBUG_PRINT
#undef PSP_DEBUG_PRINT_FUNC
#undef PSP_DEBUG_PRINT_SAMELN
#undef PSP_DEBUG_DO
#undef DEBUG_ENTER_FUNC
#undef DEBUG_EXIT_FUNC
#undef INLINE

/* Choose to print to file/screen/both */
#ifdef __PSP_PRINT_TO_FILE__
	#define __PSP_PRINT__(format,...)			PSPDebugTrace(false, format, ## __VA_ARGS__)
#elif defined __PSP_PRINT_TO_FILE_AND_SCREEN__
	#define __PSP_PRINT__(format,...)			PSPDebugTrace(true, format, ## __VA_ARGS__)
#else /* default - print to screen */
 	#define __PSP_PRINT__(format,...)			fprintf(stderr, format, ## __VA_ARGS__)
#endif /* PSP_PRINT_TO_FILE/SCREEN */
	
/* Error function */
#define PSP_ERROR(format,...)					__PSP_PRINT__("Error in %s: " format, __PRETTY_FUNCTION__, ## __VA_ARGS__)

/* Do the indent */
#define __PSP_INDENT__							for(int _i=psp_debug_indent; _i>0; _i--) \
													__PSP_PRINT__( "   ") 

/* always print */
#define PSP_INFO_PRINT(format,...)				__PSP_PRINT__(format, ## __VA_ARGS__)
/* always print, with indent */
#define PSP_INFO_PRINT_INDENT(format,...)		{ __PSP_INDENT__; \
												__PSP_PRINT__(format, ## __VA_ARGS__); }

#ifdef __PSP_DEBUG_PRINT__
	/* printf with indents */
	#define PSP_DEBUG_PRINT_SAMELN(format,...)	__PSP_PRINT__(format, ## __VA_ARGS__)
	#define PSP_DEBUG_PRINT(format,...)			{ __PSP_INDENT__; \
												__PSP_PRINT__(format, ## __VA_ARGS__); }
	#define PSP_DEBUG_PRINT_FUNC(format,...)	{ __PSP_INDENT__; \
												__PSP_PRINT__("In %s: " format, __PRETTY_FUNCTION__, ## __VA_ARGS__); }
	#define PSP_DEBUG_DO(x)						(x)						

#else	/* no debug print */
	#define PSP_DEBUG_PRINT_SAMELN(format,...)
	#define PSP_DEBUG_PRINT(format,...)	
	#define PSP_DEBUG_PRINT_FUNC(format,...)
	#define PSP_DEBUG_DO(x)	
#endif /* __PSP_DEBUG_PRINT__ */

/* Debugging function calls */
#ifdef __PSP_DEBUG_FUNCS__
	#define DEBUG_ENTER_FUNC()					PSP_INFO_PRINT_INDENT("++ %s\n", __PRETTY_FUNCTION__); \
												psp_debug_indent++ 
							
	#define DEBUG_EXIT_FUNC()					psp_debug_indent--; \
												if (psp_debug_indent < 0) PSP_ERROR("debug indent < 0\n"); \
												PSP_INFO_PRINT_INDENT("-- %s\n", __PRETTY_FUNCTION__)

	#define INLINE			/* don't want to inline so we get function names properly */
							
#else /* Don't debug function calls */
	#define DEBUG_ENTER_FUNC()
	#define DEBUG_EXIT_FUNC()
	#define INLINE						inline
#endif /* __PSP_DEBUG_FUNCS__ */

// Undef the main defines for next time
#undef __PSP_PRINT_TO_FILE__
#undef __PSP_PRINT_TO_FILE_AND_SCREEN__
#undef __PSP_DEBUG_FUNCS__
#undef __PSP_DEBUG_PRINT__
