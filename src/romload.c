/*
  DGen/SDL v1.27+

  Module for loading in the different ROM image types (.bin/.smd)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "romload.h"
#include "system.h"

/*
  WHAT YOU FIND IN THE 512 BYTES HEADER:

0: Number of blocks                           1
1: H03                                        *
2: SPLIT?                                     2
8: HAA                                        *
9: HBB                                        *
ALL OTHER BYTES: H00

1: This first byte should have the number of 16KB blocks the rom has.
The header isn't part of the formula, so this number is:
            [size of rom-512]/16384
   If the size is more than 255, the value should be H00.

2: This byte indicates if the ROM is a part of a splitted rom series. If
the rom is the last part of the series (or isn't a splitted rom at all),
this byte should be H00. In other cases should be H40. See "CREATING
SPLITTED ROMS" for details on this format.
*/

/*
  Allocate a buffer and stuff the named ROM inside. If rom_size is non-NULL,
  store the ROM size there.
*/

uint8_t *load_rom(size_t *rom_size, const char *name)
{
	FILE *file;
	size_t size;
	uint8_t *rom;
	int error;

	if (name == NULL)
		return NULL;

	// file = dgen_fopen("/accounts/1000/shared/misc/roms/smd", name, (DGEN_READ | DGEN_CURRENT));


	file = fopen(name,"r");

	if (file == NULL) {
		perror("load_rom: error");
		fprintf(stderr, "load_rom:  '%s' Can't open ROM file FD=NULL \n", name);
		return NULL;
	}

	fprintf(stderr,"load_rom: '%s'\n", name);

	/* A valid ROM will surely not be bigger than 64MB. */
	rom = load(&size, file, (64 * 1024 * 1024));
	error = errno;
	fclose(file);
	if (rom == NULL) {
		fprintf(stderr, "%s: Unable to load ROM: %s\n", name,
			strerror(error));
		return NULL;
	}
	if (size < 512) {
		fprintf(stderr, "%s: ROM file too small\n", name);
		unload(rom);
		return NULL;
	}

	fprintf(stderr,"load: checking for SEGA string ...\n");

	/*
	  If "SEGA" isn't found at 0x100 and the total size minus 512 is a
	  multiple of 0x4000, it probably is a SMD.
	*/
	if (memcmp(&rom[0x100], "SEGA", 4)) {
		uint8_t *dst = rom;
		uint8_t *src = &rom[0x200];
		size_t chunks = ((size - 0x200) / 0x4000);

		if (((size - 0x200) & (0x4000 - 1)) != 0)
			goto bad_rom;
		size -= 0x200;
		/* Corrupt ROM? Complain and continue anyway. */
		if (((rom[0] != 0x00) && (rom[0] != chunks)) ||
		    (rom[8] != 0xaa) || (rom[9] != 0xbb))
			fprintf(stderr, "%s: Corrupt SMD header\n", name);
		/*
		  De-interleave ROM, overwrite SMD header with the result.
		*/
		while (chunks) {
			size_t i;
			uint8_t tmp[0x2000];

			memcpy(tmp, src, 0x2000);
			src += 0x2000;
			for (i = 0; (i != 0x2000); ++i) {
				*(dst++) = *(src++);
				*(dst++) = tmp[i];
			}
			--chunks;
		}
		/* Does it look like a valid ROM now? */
		if (memcmp(&rom[0x100], "SEGA", 4)) {
		bad_rom:
			fprintf(stderr, "%s: Invalid ROM\n", name);
			unload(rom);
			return NULL;
		}
	}
	if (rom_size != NULL)
		*rom_size = size;
	return rom;
}

void unload_rom(uint8_t *rom)
{
	unload(rom);
}
