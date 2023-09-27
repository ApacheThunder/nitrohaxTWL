/*
    NitroHax -- Cheat tool for the Nintendo DS
    Copyright (C) 2008  Michael "Chishm" Chisholm

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <nds.h>

#include "load_bin.h"
#include "loadtwl_bin.h"
#include "cheat_engine.h"

#define LCDC_BANK_C (vu16*)0x06840000
#define LCDC_BANK_D (vu16*)0x06860000

#define CHEAT_DATA_LOCATION ((u32*)0x06850000)
#define CHEAT_DATA_LOCATIONTWL ((u32*)0x06870000)
#define CHEAT_CODE_END	0xCF000000

static void vramset (volatile void* dst, u16 val, int len)
{
	vu32* dst32 = (vu32*)dst;
	u32 val32 = val | (val << 16);

	for ( ; len > 0; len -= 4) {
		*dst32++ = val32;
	}
}

static void vramcpy (volatile void* dst, const void* src, int len)
{
	vu32* dst32 = (vu32*)dst;
	const u32* src32 = (u32*)src;

	for ( ; len > 0; len -= 4) {
		*dst32++ = *src32++;
	}
}

void runCheatEngine (void* cheats, int cheatLength, bool isTWLCart)
{
	irqDisable(IRQ_ALL);
	// The original bootloader fails to boot TWL carts. Using an alternate for those for now.
	// With new bootloader Arm9 SCFG settings occur there instead.
	if (isTWLCart) {
		VRAM_D_CR = VRAM_ENABLE | VRAM_D_LCD;
		vramset (LCDC_BANK_D, 0x0000, 128 * 1024);
		vramcpy (LCDC_BANK_D, loadtwl_bin, loadtwl_bin_size);
		vramcpy (CHEAT_DATA_LOCATIONTWL, cheats, cheatLength);
		CHEAT_DATA_LOCATIONTWL[cheatLength/sizeof(u32)] = CHEAT_CODE_END;
		CHEAT_DATA_LOCATIONTWL[cheatLength/sizeof(u32) + 1] = 0;
		VRAM_D_CR = VRAM_ENABLE | VRAM_D_ARM7_0x06020000;
		REG_EXMEMCNT = 0xffff;
		*(vu32*)0x02FFFFFC = 0;
		*(vu32*)0x02FFFE04 = (u32)0xE59FF018; // ldr pc, 0x02FFFE24
		*(vu32*)0x02FFFE24 = (u32)0x02FFFE04;  // Set ARM9 Loop address --> resetARM9(0x02FFFE04);
		resetARM7(0x06020000);
	} else {
		// Direct CPU access to VRAM bank C
		VRAM_C_CR = VRAM_ENABLE | VRAM_C_LCD;
		// Clear VRAM
		vramset (LCDC_BANK_C, 0x0000, 128 * 1024);
		// Load the loader/patcher into the correct address
		vramcpy (LCDC_BANK_C, load_bin, load_bin_size);
		// Put the codes 64KiB after the start of the loader
		vramcpy (CHEAT_DATA_LOCATION, cheats, cheatLength);
		// Mark the end of the code list
		CHEAT_DATA_LOCATION[cheatLength/sizeof(u32)] = CHEAT_CODE_END;
		CHEAT_DATA_LOCATION[cheatLength/sizeof(u32) + 1] = 0;
	
		// Give the VRAM to the ARM7
		VRAM_C_CR = VRAM_ENABLE | VRAM_C_ARM7_0x06000000;	
		
		// Set MBK and lock SCFG_EXT registers and if not a TWL enhanced cart. (must be locked later in bootloader if TWL cart)
		// Set standard NTR configuration for SCFG and MBK registers. (MBK 1-5 set only on Arm7. Done in bootloader for now)
		if (REG_SCFG_EXT & BIT(31)) {
			REG_MBK6 = 0x00003000;
			REG_MBK7 = 0x00003000;
			REG_MBK8 = 0x00003000;
			REG_SCFG_CLK = 0x80;
			REG_SCFG_EXT = 0x03000000;
		}
		
		// Reset into a passme loop
		REG_EXMEMCNT = 0xffff;
		*((vu32*)0x027FFFFC) = 0;
		*((vu32*)0x027FFE04) = (u32)0xE59FF018;
		*((vu32*)0x027FFE24) = (u32)0x027FFE04;
	}	
	swiSoftReset(); 
}

