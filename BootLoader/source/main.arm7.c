/*
 main.arm7.c

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

#ifndef ARM7
# define ARM7
#endif
#include <nds/ndstypes.h>
#include <nds/system.h>
#include <nds/interrupts.h>
#include <nds/timers.h>
#include <nds/dma.h>
#include <nds/arm7/audio.h>
#include <nds/ipc.h>

#ifndef NULL
#define NULL 0
#endif

#include "common.h"
#include "read_card.h"
#include "cheat.h"

/*-------------------------------------------------------------------------
External functions
--------------------------------------------------------------------------*/
extern void arm7_clearmem (void* loc, size_t len);
extern void arm7_reset (void);

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Important things
#define NDS_HEAD 0x027FFE00
tNDSHeader* ndsHeader = (tNDSHeader*)NDS_HEAD;

#define CHEAT_ENGINE_LOCATION	0x027FE000
#define CHEAT_DATA_LOCATION  	0x06010000

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Used for debugging purposes
static void errorOutput (u32 code) {
	// Set the error code, then set our state to "error"
	arm9_errorCode = code;
	ipcSendState(ARM7_ERR);
	// Stop
	while(1);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Firmware stuff

#define FW_READ        0x03

void arm7_readFirmware (uint32 address, uint8 * buffer, uint32 size) {
  uint32 index;

  // Read command
  while (REG_SPICNT & SPI_BUSY);
  REG_SPICNT = SPI_ENABLE | SPI_CONTINUOUS | SPI_DEVICE_NVRAM;
  REG_SPIDATA = FW_READ;
  while (REG_SPICNT & SPI_BUSY);

  // Set the address
  REG_SPIDATA =  (address>>16) & 0xFF;
  while (REG_SPICNT & SPI_BUSY);
  REG_SPIDATA =  (address>>8) & 0xFF;
  while (REG_SPICNT & SPI_BUSY);
  REG_SPIDATA =  (address) & 0xFF;
  while (REG_SPICNT & SPI_BUSY);

  for (index = 0; index < size; index++) {
    REG_SPIDATA = 0;
    while (REG_SPICNT & SPI_BUSY);
    buffer[index] = REG_SPIDATA & 0xFF;
  }
  REG_SPICNT = 0;
}

/*-------------------------------------------------------------------------
arm7_resetMemory
Clears all of the NDS's RAM that is visible to the ARM7
Written by Darkain.
Modified by Chishm:
 * Added STMIA clear mem loop
--------------------------------------------------------------------------*/
void arm7_resetMemory (void) {
	int i;
	u8 settings1, settings2;

	REG_IME = 0;

	for (i=0; i<16; i++) {
		SCHANNEL_CR(i) = 0;
		SCHANNEL_TIMER(i) = 0;
		SCHANNEL_SOURCE(i) = 0;
		SCHANNEL_LENGTH(i) = 0;
	}
	REG_SOUNDCNT = 0;

	// Clear out ARM7 DMA channels and timers
	for (i=0; i<4; i++) {
		DMA_CR(i) = 0;
		DMA_SRC(i) = 0;
		DMA_DEST(i) = 0;
		TIMER_CR(i) = 0;
		TIMER_DATA(i) = 0;
	}

	// Clear out FIFO
	REG_IPC_SYNC = 0;
	REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
	REG_IPC_FIFO_CR = 0;

	// clear IWRAM - 037F:8000 to 0380:FFFF, total 96KiB
	arm7_clearmem ((void*)0x037F8000, 96*1024);

	// clear most of EXRAM - except after 0x023FD800, which has the ARM9 code
	arm7_clearmem ((void*)0x02000000, 0x003FD800);

	// clear last part of EXRAM, skipping the ARM9's section
	arm7_clearmem ((void*)0x023FE000, 0x2000);

	REG_IE = 0;
	REG_IF = ~0;
	(*(vu32*)(0x04000000-4)) = 0;  //IRQ_HANDLER ARM7 version
	(*(vu32*)(0x04000000-8)) = ~0; //VBLANK_INTR_WAIT_FLAGS, ARM7 version
	REG_POWERCNT = 1;  //turn off power to stuffs

	// Reload DS Firmware settings
	arm7_readFirmware((u32)0x03FE70, &settings1, 0x1);
	arm7_readFirmware((u32)0x03FF70, &settings2, 0x1);

	if (settings1 > settings2) {
		arm7_readFirmware((u32)0x03FE00, (u8*)0x027FFC80, 0x70);
		arm7_readFirmware((u32)0x03FF00, (u8*)0x027FFD80, 0x70);
	} else {
		arm7_readFirmware((u32)0x03FF00, (u8*)0x027FFC80, 0x70);
		arm7_readFirmware((u32)0x03FE00, (u8*)0x027FFD80, 0x70);
	}

	// Load FW header
	arm7_readFirmware((u32)0x000000, (u8*)0x027FF830, 0x20);
}


int arm7_loadBinary (void) {
	u32 chipID;
	u32 errorCode;

	// Init card
	errorCode = cardInit(ndsHeader, &chipID);
	if (errorCode) {
		return errorCode;
	}

	// Set memory values expected by loaded NDS
	*((u32*)0x027ff800) = chipID;					// CurrentCardID
	*((u32*)0x027ff804) = chipID;					// Command10CardID
	*((u32*)0x027ffc00) = chipID;					// 3rd chip ID
	*((u16*)0x027ff808) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)0x027ff80a) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]
	*((u16*)0x027ffc40) = 0x1;						// Booted from card -- EXTREMELY IMPORTANT!!! Thanks to cReDiAr

	cardRead(ndsHeader->arm9romOffset, (u32*)ndsHeader->arm9destination, ndsHeader->arm9binarySize);
	cardRead(ndsHeader->arm7romOffset, (u32*)ndsHeader->arm7destination, ndsHeader->arm7binarySize);
	return ERR_NONE;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Main function

void arm7_main (void) {
	int errorCode;

	// Synchronise start
	while (ipcRecvState() != ARM9_START);
	ipcSendState(ARM7_START);

	// Wait until ARM9 is ready
	while (ipcRecvState() != ARM9_READY);

	ipcSendState(ARM7_MEMCLR);

	// Get ARM7 to clear RAM
	arm7_resetMemory();

	if (REG_SNDEXTCNT != 0) {
		if ((REG_SCFG_EXT & BIT(31))) {
			*((vu32*)REG_MBK1)=0x8D898581;
			*((vu32*)REG_MBK2)=0x8C888480;
			*((vu32*)REG_MBK3)=0x9C989490;
			*((vu32*)REG_MBK4)=0x8C888480;
			*((vu32*)REG_MBK5)=0x9C989490;
			REG_MBK6=0x09403900;
			REG_MBK7=0x09803940;
			REG_MBK8=0x09C03980;
			REG_MBK9=0xFCFFFF0F;
			REG_SCFG_ROM = 0x703;
			REG_SCFG_CLK = 0x100;
			REG_SCFG_EXT = 0x12A00000;
		}
	}
	
	ipcSendState(ARM7_LOADBIN);

	// Load the NDS file
	errorCode = arm7_loadBinary();
	if (errorCode) {
		errorOutput(errorCode);
	}
	
	ipcSendState(ARM7_HOOKBIN);

	// Load the cheat engine and hook it into the ARM7 binary
	errorCode = arm7_hookGame(ndsHeader, (const u32*)CHEAT_DATA_LOCATION, (u32*)CHEAT_ENGINE_LOCATION);
	if (errorCode != ERR_NONE && errorCode != ERR_NOCHEAT) {
		errorOutput(errorCode);
	}
	
	ipcSendState(ARM7_BOOTBIN);

	arm7_reset();
}
