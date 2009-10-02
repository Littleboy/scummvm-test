/* md5table - Convert a MD5 table to either PHP or C++ code
 * Copyright (C) 2003-2006 The ScummVM Team
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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void error(const char *s, ...) {
	char buf[1024];
	va_list va;

	va_start(va, s);
	vsnprintf(buf, 1024, s, va);
	va_end(va);

	fprintf(stderr, "ERROR: %s!\n", buf);

	exit(1);
}

void warning(const char *s, ...) {
	char buf[1024];
	va_list va;

	va_start(va, s);
	vsnprintf(buf, 1024, s, va);
	va_end(va);

	fprintf(stderr, "WARNING: %s!\n", buf);
}

typedef struct {
	const char *key;
	const char *value;
} StringMap;

typedef struct {
	const char *md5;
	const char *language;
	const char *platform;
	const char *variant;
	const char *extra;
	const char *size;
	const char *desc;
	const char *comments;
	const char *infoSource;
} Entry;

/* Map MD5 table platform names to ScummVM constant names.
 * Note: Currently not many constants are defined within ScummVM. However, more
 * will probably be added eventually (see also commented out constants in
 * common/util.h).
 */
static const StringMap platformMap[] = {
	{ "3DO",		"kPlatform3DO" },
	{ "Amiga",		"kPlatformAmiga" },
	{ "Atari",		"kPlatformAtariST" },
	{ "C64",		"kPlatformC64" },
	{ "DOS",		"kPlatformPC" },
	{ "FM-TOWNS",		"kPlatformFMTowns" },
	{ "Mac",		"kPlatformMacintosh" },
	{ "NES",		"kPlatformNES" },
	{ "PC-Engine",		"kPlatformPCEngine" },
	{ "SEGA",		"kPlatformSegaCD" },
	{ "Windows",		"kPlatformWindows" },
	{ "Wii",		"kPlatformWii" },

	{ "All?",		"kPlatformUnknown" },
	{ "All",		"kPlatformUnknown" },

	{ 0,			"kPlatformUnknown" }
};

static const StringMap langMap[] = {
	{ "en",		"EN_ANY" },
	{ "us",		"EN_USA" },
	{ "gb",		"EN_GRB" },
	{ "de",		"DE_DEU" },
	{ "fr",		"FR_FRA" },
	{ "it",		"IT_ITA" },
	{ "br",		"PT_BRA" },
	{ "es",		"ES_ESP" },
	{ "jp",		"JA_JPN" },
	{ "zh",		"ZH_TWN" },
	{ "ko",		"KO_KOR" },
	{ "se",		"SE_SWE" },
	{ "en",		"EN_GRB" },
	{ "hb",		"HB_ISR" },
	{ "ru",		"RU_RUS" },
	{ "cz",		"CZ_CZE" },
	{ "nl",		"NL_NLD" },
	{ "nb",		"NB_NOR" },
	{ "pl",		"PL_POL" },

	{ "All",	"UNK_LANG" },
	{ "All?",	"UNK_LANG" },

	{ 0,		"UNK_LANG" }
};

static const char *php_header =
	"<!--\n"
	"  This file was generated by the md5table tool on %s"
	"  DO NOT EDIT MANUALLY!\n"
	" -->\n"
	"<?php\n"
	"\n";

static const char *c_header =
	"/*\n"
	"  This file was generated by the md5table tool on %s"
	"  DO NOT EDIT MANUALLY!\n"
	" */\n"
	"\n"
	"struct MD5Table {\n"
	"	const char *md5;\n"
	"	const char *gameid;\n"
	"	const char *variant;\n"
	"	const char *extra;\n"
	"	int32 filesize;\n"
	"	Common::Language language;\n"
	"	Common::Platform platform;\n"
	"};\n"
	"\n"
	"static const MD5Table md5table[] = {\n";

static const char *c_footer =
	"	{ 0, 0, 0, 0, 0, Common::UNK_LANG, Common::kPlatformUnknown }\n"
	"};\n";

static void parseEntry(Entry *entry, char *line) {
	assert(entry);
	assert(line);

	/* Split at the tabs */
	entry->md5 = strtok(line, "\t\n\r");
	entry->size = strtok(NULL, "\t\n\r");
	entry->language = strtok(NULL, "\t\n\r");
	entry->platform = strtok(NULL, "\t\n\r");
	entry->variant = strtok(NULL, "\t\n\r");
	entry->extra = strtok(NULL, "\t\n\r");
	entry->desc = strtok(NULL, "\t\n\r");
	entry->infoSource = strtok(NULL, "\t\n\r");
}

static int isEmptyLine(const char *line) {
	const char *whitespace = " \t\n\r";
	while (*line) {
		if (!strchr(whitespace, *line))
			return 0;
		line++;
	}
	return 1;
}

static const char *mapStr(const char *str, const StringMap *map) {
	assert(str);
	assert(map);
	while (map->key) {
		if (0 == strcmp(map->key, str))
			return map->value;
		map++;
	}
	warning("mapStr: unknown string '%s', defaulting to '%s'", str, map->value);
	return map->value;
}

void showhelp(const char *exename)
{
	printf("\nUsage: %s <params>\n", exename);
	printf("\nParams:\n");
	printf(" --c++   output C++ code for inclusion in ScummVM (default)\n");
	printf(" --php   output PHP code for the web site\n");
	printf(" --txt   output TXT file (should be identical to input file)\n");
	exit(2);
}

/* needed to call from qsort */
int strcmp_wrapper(const void *s1, const void *s2)
{
	return strcmp((const char *)s1, (const char *)s2);
}

int main(int argc, char *argv[])
{
	FILE *inFile = stdin;
	FILE *outFile = stdout;
	char buffer[1024];
	char section[256];
	char gameid[32];
	char *line;
	int err;
	int i;
	time_t theTime;
	char *generationDate;

	const int entrySize = 256;
	int numEntries = 0, maxEntries = 1;
	char *entriesBuffer = malloc(maxEntries * entrySize);

	typedef enum {
		kCPPOutput,
		kPHPOutput,
		kTXTOutput
	} OutputMode;

	OutputMode outputMode = kCPPOutput;

	if (argc != 2)
		showhelp(argv[0]);
	if (strcmp(argv[1], "--c++") == 0) {
		outputMode = kCPPOutput;
	} else if (strcmp(argv[1], "--php") == 0) {
		outputMode = kPHPOutput;
	} else if (strcmp(argv[1], "--txt") == 0) {
		outputMode = kTXTOutput;
	} else {
		showhelp(argv[0]);
	}

	time(&theTime);
	generationDate = strdup(asctime(gmtime(&theTime)));

	if (outputMode == kPHPOutput)
		fprintf(outFile, php_header, generationDate);

	section[0] = 0;
	gameid[0] = 0;
	while ((line = fgets(buffer, sizeof(buffer), inFile))) {
		/* Parse line */
		if (line[0] == '#' || isEmptyLine(line)) {
			if (outputMode == kTXTOutput)
				fprintf(outFile, "%s", line);
			continue;	/* Skip comments & empty lines */
		}
		if (line[0] == '\t') {
			Entry entry;
			assert(section[0]);
			parseEntry(&entry, line+1);
			if (outputMode == kPHPOutput) {
				fprintf(outFile, "\taddEntry(");

				// Print the description string
				fprintf(outFile, "\"");
				if (entry.extra && strcmp(entry.extra, "-")) {
					fprintf(outFile, "%s", entry.extra);
					if (entry.desc && strcmp(entry.desc, "-"))
						fprintf(outFile, " (%s)", entry.desc);
				}
				fprintf(outFile, "\", ");

				fprintf(outFile, "\"%s\", ", entry.platform);
				fprintf(outFile, "\"%s\", ", entry.language);
				fprintf(outFile, "\"%s\"", entry.md5);
				if (entry.infoSource)
					fprintf(outFile, ", \"%s\"", entry.infoSource);
				fprintf(outFile, ");\n");
			} else if (outputMode == kTXTOutput) {
				fprintf(outFile, "\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
					entry.md5,
					entry.size,
					entry.language,
					entry.platform,
					entry.variant,
					entry.extra,
					entry.desc,
					entry.infoSource ? entry.infoSource : ""
					);
			} else if (entry.md5) {
				if (numEntries >= maxEntries) {
					maxEntries *= 2;
					entriesBuffer = realloc(entriesBuffer, maxEntries * entrySize);
				}
				if (0 == strcmp(entry.variant, "-"))
					entry.variant = "";
				if (0 == strcmp(entry.extra, "-"))
					entry.extra = "";
				snprintf(entriesBuffer + numEntries * entrySize, entrySize,
					"\t{ \"%s\", \"%s\", \"%s\", \"%s\", %s, Common::%s, Common::%s },\n",
					entry.md5,
					gameid,
					entry.variant,
					entry.extra,
					entry.size,
					mapStr(entry.language, langMap),
					mapStr(entry.platform, platformMap));
				numEntries++;
			}
		} else {
			if (outputMode == kPHPOutput && gameid[0] != 0) {
				// If there is an active section, close it now
				fprintf(outFile, "endSection();\n");
			}
			// Read the gameid, followed by a tab
			for (i = 0; *line && *line != '\t'; ++i)
				gameid[i] = *line++;
			assert(i > 0);
			gameid[i] = 0;
			assert(*line != 0);
			line++;

			// Read the section header (usually the full game name)
			for (i = 0; *line && *line != '\n'; ++i)
				section[i] = *line++;
			assert(i > 0);
			section[i] = 0;

			// If in PHP or TXT mode, we write the output immediately
			if (outputMode == kPHPOutput) {
				fprintf(outFile, "beginSection(\"%s\", \"%s\");\n", section, gameid);
			} else if (outputMode == kTXTOutput) {
				fprintf(outFile, "%s\t%s\n", gameid, section);
			}
		}
	}

	err = ferror(inFile);
	if (err)
		error("Failed reading from input file, error %d", err);

	if (outputMode == kPHPOutput) {
		if (gameid[0] != 0)		// If there is an active section, close it now
			fprintf(outFile, "endSection();\n");

		fprintf(outFile, "?>\n");
	}

	if (outputMode == kCPPOutput) {
		/* Printf header */
		fprintf(outFile, c_header, generationDate);
		/* Now sort the MD5 table (this allows for binary searches) */
		qsort(entriesBuffer, numEntries, entrySize, strcmp_wrapper);
		/* Output the table and emit warnings if duplicate md5s are found */
		buffer[0] = '\0';
		for (i = 0; i < numEntries; ++i) {
			const char *currentEntry = entriesBuffer + i * entrySize;
			fprintf(outFile, "%s", currentEntry);
			if (strncmp(currentEntry + 4, buffer, 32) == 0) {
				warning("Duplicate MD5 found '%.32s'", buffer);
			} else {
				strncpy(buffer, currentEntry + 4, 32);
			}
		}
		/* Finally, print the footer */
		fprintf(outFile, "%s", c_footer);
	}

	free(entriesBuffer);
	free(generationDate);

	return 0;
}
