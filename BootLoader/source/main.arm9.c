/*
 main.arm9.c

 By Michael Chisholm (Chishm)

 All resetMemory and startBinary functions are based
 on the MultiNDS loader by Darkain.
 Original source available at:
 http://cvs.sourceforge.net/viewcvs.py/ndslib/ndslib/examples/loader/boot/main.cpp

 License:
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

#define ARM9
#undef ARM7

#include <nds/memory.h>
#include <nds/arm9/video.h>
#include <nds/arm9/input.h>
#include <nds/interrupts.h>
#include <nds/dma.h>
#include <nds/timers.h>
#include <nds/system.h>
#include <nds/ipc.h>

#include "common.h"
#include "miniConsole.h"

tNDSHeader* ndsHeader = NULL;

volatile int arm9_stateFlag = ARM9_BOOT;
volatile u32 arm9_errorCode = 0xFFFFFFFF;
volatile bool arm9_errorClearBG = false;
volatile u32 arm9_BLANK_RAM = 0;
volatile u32 defaultFontPalSlot = 0x16;
bool arm9_isSdk5 = false;

static char TXT_STATUS[] = "STATUS: ";
static char TXT_ERROR[] = "ERROR: ";
static char ERRTXT_NONE[] = "NONE";
static char ERRTXT_STS_CLRMEM[] = "CLEAR MEMORY";
static char ERRTXT_STS_LOAD_BIN[] = "LOAD BINARY";
static char ERRTXT_STS_START[] = "START";
static char ERRTXT_STS_HOOK_BIN[] = "HOOK BINARY";
static char ERRTXT_LOAD_NORM[] = "LOAD NORMAL";
static char ERRTXT_LOAD_OTHR[] = "LOAD OTHER";
static char ERRTXT_SEC_NORM[] = "SECURE NORMAL";
static char ERRTXT_SEC_OTHR[] = "SECURE OTHER";
static char ERRTXT_LOGO_CRC[] = "LOGO CRC";
static char ERRTXT_HEAD_CRC[] = "HEADER CRC";
static char ERRTXT_NOCHEAT[] = "NO CHEATS FOUND";
static char ERRTXT_HOOK[] = "CHEAT HOOK FAIL";
static char NEW_LINE[] = "\n";
/*-------------------------------------------------------------------------
External functions
--------------------------------------------------------------------------*/
extern void arm9_clearCache (void);
extern void arm9_reset (void);
extern void Print(char *str);
/*-------------------------------------------------------------------------
arm9_errorOutput
Displays an error code on screen.

Each box is 2 bits, and left-to-right is most-significant bits to least.
Red = 00, Yellow = 01, Green = 10, Blue = 11

Written by Chishm
--------------------------------------------------------------------------*/
static void arm9_errorOutput (u32 code) {
	switch (code) {
		case (ERR_NONE) : { 
			BG_PALETTE_SUB[defaultFontPalSlot] = 0x8360;
			Print(TXT_STATUS);
			Print(ERRTXT_NONE);
		} break;
		case (ERR_STS_CLR_MEM) : {
			BG_PALETTE_SUB[defaultFontPalSlot] = 0x8360;
			Print(TXT_STATUS);
			Print(ERRTXT_STS_CLRMEM);
		} break;
		case (ERR_STS_LOAD_BIN) : {
			BG_PALETTE_SUB[defaultFontPalSlot] = 0x8360;
			Print(TXT_STATUS);
			Print(ERRTXT_STS_LOAD_BIN);
		} break;
		case (ERR_STS_HOOK_BIN) : {
			BG_PALETTE_SUB[defaultFontPalSlot] = 0x801B;
			Print(TXT_STATUS);
			Print(ERRTXT_STS_HOOK_BIN);
		} break;
		case (ERR_STS_START) : {
			BG_PALETTE_SUB[defaultFontPalSlot] = 0x801B;
			Print(TXT_STATUS);
			Print(ERRTXT_STS_START);
		} break;
		case (ERR_LOAD_NORM) : {
			BG_PALETTE_SUB[defaultFontPalSlot] = 0x801B;
			Print(TXT_ERROR);
			Print(ERRTXT_LOAD_NORM);
		} break;
		case (ERR_LOAD_OTHR) : {
			BG_PALETTE_SUB[defaultFontPalSlot] = 0x801B;
			Print(TXT_ERROR);
			Print(ERRTXT_LOAD_OTHR);
		} break;
		case (ERR_SEC_NORM) : {
			BG_PALETTE_SUB[defaultFontPalSlot] = 0x801B;
			Print(TXT_ERROR);
			Print(ERRTXT_SEC_NORM);
		} break;
		case (ERR_SEC_OTHR) : {
			BG_PALETTE_SUB[defaultFontPalSlot] = 0x801B;
			Print(TXT_ERROR);
			Print(ERRTXT_SEC_OTHR);
		} break;
		case (ERR_LOGO_CRC) : {
			BG_PALETTE_SUB[defaultFontPalSlot] = 0x801B;
			Print(TXT_ERROR);
			Print(ERRTXT_LOGO_CRC);
		} break;
		case (ERR_HEAD_CRC) : {
			BG_PALETTE_SUB[defaultFontPalSlot] = 0x801B;
			Print(TXT_ERROR);
			Print(ERRTXT_HEAD_CRC);
		} break;
		case (ERR_NOCHEAT) : {
			BG_PALETTE_SUB[defaultFontPalSlot] = 0x801B;
			Print(TXT_ERROR);
			Print(ERRTXT_NOCHEAT);
		} break;
		case (ERR_HOOK) : {
			BG_PALETTE_SUB[defaultFontPalSlot] = 0x801B;
			Print(TXT_ERROR);
			Print(ERRTXT_HOOK);
		} break;
	}
	Print(NEW_LINE);
}

/*-------------------------------------------------------------------------
arm9_main
Clears the ARM9's icahce and dcache
Clears the ARM9's DMA channels and resets video memory
Jumps to the ARM9 NDS binary in sync with the  ARM7
Written by Darkain, modified by Chishm
--------------------------------------------------------------------------*/
void arm9_main (void) {

	if (REG_SCFG_EXT & BIT(31)) {
		REG_MBK6 = 0x00003000;
		REG_MBK7 = 0x00003000;
		REG_MBK8 = 0x00003000;
	}
	
	register int i;

	//set shared ram to ARM7
	WRAM_CR = 0x03;
	REG_EXMEMCNT = 0xE880;

	// Disable interrupts
	REG_IME = 0;
	REG_IE = 0;
	REG_IF = ~0;

	arm9_errorCode = ERR_NONE;
	
	// Synchronise start
	ipcSendState(ARM9_START);
	while (ipcRecvState() != ARM7_START);

	ipcSendState(ARM9_MEMCLR);

	arm9_clearCache();

	for (i=0; i<16*1024; i+=4) {  //first 16KB
		(*(vu32*)(i+0x00000000)) = 0x00000000;      //clear ITCM
		(*(vu32*)(i+0x00800000)) = 0x00000000;      //clear DTCM
	}

	for (i=16*1024; i<32*1024; i+=4) {  //second 16KB
		(*(vu32*)(i+0x00000000)) = 0x00000000;      //clear ITCM
	}

	(*(vu32*)0x00803FFC) = 0;   //IRQ_HANDLER ARM9 version
	(*(vu32*)0x00803FF8) = ~0;  //VBLANK_INTR_WAIT_FLAGS ARM9 version

	// Clear out FIFO
	REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
	REG_IPC_FIFO_CR = 0;

	
//	Blank out VRAM
//	VRAM_A_CR = 0x80; // Don't clear this one to make transition into game smoother. (still gets cleared after final ipcRecvState while loop)
	VRAM_B_CR = 0x80;
//	VRAM_C_CR = 0x80; // Don't clear this one to save console for bootloader use. (and for same reason as VRAM_A)
//	VRAM_D_CR = 0x80; // Don't mess with the VRAM used for execution
	VRAM_E_CR = 0x80;
	VRAM_F_CR = 0x80;
	VRAM_G_CR = 0x80;
	VRAM_H_CR = 0x80;
	VRAM_I_CR = 0x80;
	VRAM_B_CR = 0;
//	VRAM_D_CR = 0; // Don't mess with the ARM7's VRAM
	VRAM_E_CR = 0;
	VRAM_F_CR = 0;
	VRAM_G_CR = 0;
	VRAM_H_CR = 0;
	VRAM_I_CR = 0;

	// Clear out ARM9 DMA channels
	for (i=0; i<4; i++) {
		DMA_CR(i) = 0;
		DMA_SRC(i) = 0;
		DMA_DEST(i) = 0;
		TIMER_CR(i) = 0;
		TIMER_DATA(i) = 0;
	}
	
	defaultFontPalSlot = 31;
	miniconsoleSetWindow(5, 10, 24, 2); // Set console position for debug text if/when needed.

	// set ARM9 state to ready and wait for instructions from ARM7
	ipcSendState(ARM9_READY);
	while (ipcRecvState() != ARM7_BOOTBIN) {
		if (ipcRecvState() == ARM7_ERR) {
			arm9_errorOutput (arm9_errorCode);
			// Halt after displaying error code
			while(1);
		} else if (arm9_errorCode != ERR_NONE) {
			arm9_errorOutput (arm9_errorCode);
			arm9_errorCode = ERR_NONE;
		}
		if (ipcRecvState() == ARM7_SETSCFG) {
			if (REG_SCFG_EXT & BIT(31)) {
				REG_SCFG_EXT = 0x83000000;
				if (arm9_isSdk5 && ndsHeader->unitCode > 0) {
					// REG_SCFG_EXT |= BIT(13);	// Extended VRAM Access
					REG_SCFG_CLK = 0x87;
					REG_SCFG_RST = 1;
				} else {
					REG_SCFG_CLK = 0x80;
				}
				REG_SCFG_EXT &= ~(1UL << 31); // lock SCFG
			}
			ipcSendState(ARM9_SETSCFG);
		}
	}
	
	// We clear the reset of VRAM and screen stuff now that arm9_errorOutput is not needed if it makes it this far.
	BG_PALETTE[0] = 0xFFFF;
	dmaFill((u16*)&arm9_BLANK_RAM, BG_PALETTE+1, (2*1024)-2);
	dmaFillWords(0, (void*)0x04000000, 0x56);
	dmaFillWords(0, (void*)0x04001008, 0x56);
	VRAM_A_CR = 0x80;
	REG_DISPSTAT = 0;
	videoSetMode(0);
	videoSetModeSub(0);
	REG_POWERCNT = 0x820F;
	arm9_reset();
}

