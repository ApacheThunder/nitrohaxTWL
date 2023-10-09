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
#include <string.h>

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
#define NDS_HEADER		   0x027FFE00
#define NDS_HEADER_POKEMON 0x027FF000
#define TWL_HEADER         0x027FE000
tNDSHeader* ndsHeader;

#define CHEAT_ENGINE_LOCATION	0x027FC000 // Moved to avoid conflict with TWL_HEADER region
#define CHEAT_DATA_LOCATION  	0x06030000

#define REG_GPIO_WIFI *(vu16*)0x4004C04

static u32 chipID;

static tNDSHeader* loadHeader(tDSiHeader* twlHeaderTemp) {
	tNDSHeader* ntrHeader = (tNDSHeader*)NDS_HEADER;
	*ntrHeader = twlHeaderTemp->ndshdr;
	if (ntrHeader->unitCode > 0) {
		tDSiHeader* dsiHeader = (tDSiHeader*)TWL_HEADER; // __DSiHeader
		*dsiHeader = *twlHeaderTemp;
	}
	return ntrHeader;
}

const char* getRomTid(const tNDSHeader* ndsHeader) {
	static char romTid[5];
	strncpy(romTid, ndsHeader->gameCode, 4);
	romTid[4] = '\0';
	return romTid;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Used for debugging purposes
static void errorOutput (u32 code, bool isError) {
	// Set the error code, then set our state to "error".
	// (unless if it's just a status readout we don't set state to error or stay in while loop for eternaty)
	arm9_errorCode = code;
	if (isError) { 
		ipcSendState(ARM7_ERR);
		while(1);// Stop
	}
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Firmware stuff
void arm7_readFirmware (uint32 address, uint8 * buffer, uint32 size) {
  uint32 index;
  u16 FW_READ = 0x03;

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
	int i, reg;
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
		if (REG_SNDEXTCNT != 0 && (REG_SCFG_EXT & BIT(31)))for(reg=0; reg<0x1c; reg+=4)*((u32*)(0x04004104 + ((i*0x1c)+reg))) = 0; //Reset NDMA.
	}

	// Clear out FIFO
	REG_IPC_SYNC = 0;
	REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
	REG_IPC_FIFO_CR = 0;

	// clear IWRAM - 037F:8000 to 0380:FFFF, total 96KiB
	arm7_clearmem ((void*)0x037F8000, 96*1024);

	// clear most of EXRAM - except after 0x022FD800, which has the ARM9 code
	arm7_clearmem ((void*)0x02000000, 0x002FD800);

	// clear last part of EXRAM, skipping the ARM9's section
	arm7_clearmem ((void*)0x023FE000, 0x2000);

	REG_IE = 0;
	REG_IF = ~0;
	if (REG_SNDEXTCNT != 0) {
		REG_AUXIE = 0;
		REG_AUXIF = ~0;
	}
	(*(vu32*)(0x04000000-4)) = 0;  //IRQ_HANDLER ARM7 version
	(*(vu32*)(0x04000000-8)) = ~0; //VBLANK_INTR_WAIT_FLAGS, ARM7 version
	
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

static void setMemoryAddress(const tNDSHeader* ndsHeader) {
	if (ndsHeader->unitCode > 0) {
		copyLoop((u32*)0x027FFA80, (u32*)ndsHeader, 0x160);	// Make a duplicate of DS header

		*(u32*)(0x027FA680) = 0x02FD4D80;
		*(u32*)(0x027FA684) = 0x00000000;
		*(u32*)(0x027FA688) = 0x00001980;

		*(u32*)(0x027FF00C) = 0x0000007F;
		*(u32*)(0x027FF010) = 0x550E25B8;
		*(u32*)(0x027FF014) = 0x02FF4000;

		// Set region flag
		if (strncmp(getRomTid(ndsHeader)+3, "J", 1) == 0) {
			*(u8*)(0x027FFD70) = 0;
		} else if (strncmp(getRomTid(ndsHeader)+3, "E", 1) == 0) {
			*(u8*)(0x027FFD70) = 1;
		} else if (strncmp(getRomTid(ndsHeader)+3, "P", 1) == 0) {
			*(u8*)(0x027FFD70) = 2;
		} else if (strncmp(getRomTid(ndsHeader)+3, "U", 1) == 0) {
			*(u8*)(0x027FFD70) = 3;
		} else if (strncmp(getRomTid(ndsHeader)+3, "C", 1) == 0) {
			*(u8*)(0x027FFD70) = 4;
		} else if (strncmp(getRomTid(ndsHeader)+3, "K", 1) == 0) {
			*(u8*)(0x027FFD70) = 5;
		}
	}

    // Set memory values expected by loaded NDS
    // from NitroHax, thanks to Chism
	*((u32*)0x027FF800) = chipID;					// CurrentCardID
	*((u32*)0x027FF804) = chipID;					// Command10CardID
	*((u16*)0x027FF808) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)0x027FF80A) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]
	*((u16*)0x027FF850) = 0x5835;
	// Copies of above
	*((u32*)0x027FFC00) = chipID;					// CurrentCardID
	*((u32*)0x027FFC04) = chipID;					// Command10CardID
	*((u16*)0x027FFC08) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)0x027FFC0A) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]
	*((u16*)0x027FFC10) = 0x5835;
	*((u16*)0x027FFC40) = 0x01;						// Boot Indicator -- EXTREMELY IMPORTANT!!! Thanks to cReDiAr
}

u32 arm7_loadBinary (void) {
	u32 errorCode;
	
	tDSiHeader* twlHeaderTemp = (tDSiHeader*)CHEAT_ENGINE_LOCATION; // Use same region cheat engine goes. Cheat engine will replace this later when it's not needed.

	// Init card
	errorCode = cardInit((sNDSHeaderExt*)twlHeaderTemp, &chipID);
	if (errorCode)return errorCode;
	
	// Fix Pokemon games needing header data.
	copyLoop((u32*)NDS_HEADER_POKEMON, (u32*)ndsHeader, 0x170);

	char* romTid = (char*)NDS_HEADER_POKEMON+0xC;
	if (   memcpy(romTid, "ADA", 3) == 0 // Diamond
		|| memcmp(romTid, "APA", 3) == 0 // Pearl
		|| memcmp(romTid, "CPU", 3) == 0 // Platinum
		|| memcmp(romTid, "IPK", 3) == 0 // HG
		|| memcmp(romTid, "IPG", 3) == 0 // SS
	) {
		// Make the Pokemon game code ADAJ.
		const char gameCodePokemon[] = { 'A', 'D', 'A', 'J' };
		memcpy((char*)NDS_HEADER_POKEMON+0xC, gameCodePokemon, 4);
	}
	
	cardRead(twlHeaderTemp->ndshdr.arm9romOffset, (u32*)twlHeaderTemp->ndshdr.arm9destination, twlHeaderTemp->ndshdr.arm9binarySize);
	cardRead(twlHeaderTemp->ndshdr.arm7romOffset, (u32*)twlHeaderTemp->ndshdr.arm7destination, twlHeaderTemp->ndshdr.arm7binarySize);
	
	ndsHeader = loadHeader(twlHeaderTemp); // copy twlHeaderTemp to ndsHeader location
	
	return ERR_NONE;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Main function
void arm7_main (void) {
	
	if (REG_SNDEXTCNT != 0) {
		if (REG_SCFG_EXT & BIT(31)) {
			REG_MBK9=0xFCFFFF0F;
			*((vu32*)REG_MBK1)=0x8D898581;
			*((vu32*)REG_MBK2)=0x8C888480;
			*((vu32*)REG_MBK3)=0x9C989490;
			*((vu32*)REG_MBK4)=0x8C888480;
			*((vu32*)REG_MBK5)=0x9C989490;
			REG_MBK6=0x09403900;
			REG_MBK7=0x09803940;
			REG_MBK8=0x09C03980;
		}
	}
	
	u32 errorCode;
	int I;

	// Synchronise start
	while (ipcRecvState() != ARM9_START);
	ipcSendState(ARM7_START);

	// Wait until ARM9 is ready
	while (ipcRecvState() != ARM9_READY);

	errorOutput(ERR_STS_CLR_MEM, false);

	ipcSendState(ARM7_MEMCLR);

	// Get ARM7 to clear RAM
	arm7_resetMemory();
	
	if (REG_SNDEXTCNT != 0) {
		if (REG_SCFG_EXT & BIT(31))REG_SCFG_ROM = 0x703;
	}

	errorOutput(ERR_STS_LOAD_BIN, false);
	
	ipcSendState(ARM7_LOADBIN);

	// Load the NDS file
	errorCode = arm7_loadBinary();
	if (errorCode)errorOutput(errorCode, true);
	
	if (REG_SNDEXTCNT != 0 && (REG_SCFG_EXT & BIT(31))) {
		*(u16*)0x4000500 = 0x807F;
		REG_GPIO_WIFI |= BIT(8);	// Old NDS-Wifi mode
		REG_SCFG_CLK = 0x100;
		REG_SCFG_EXT = 0x12A00000;
	}
	
	errorOutput(ERR_STS_HOOK_BIN, false);
	
	ipcSendState(ARM7_HOOKBIN);

	// Load the cheat engine and hook it into the ARM7 binary
	errorCode = arm7_hookGame(ndsHeader, (const u32*)CHEAT_DATA_LOCATION, (u32*)CHEAT_ENGINE_LOCATION);
	if (errorCode != ERR_NONE && errorCode != ERR_NOCHEAT) {
		errorOutput(errorCode, false);
		for (I = 0; I < 100; I++) { while(REG_VCOUNT!=191); while(REG_VCOUNT==191); } // Allow cheat hook fail message to display for a couple seconds
	}

	setMemoryAddress(ndsHeader);

	ipcSendState(ARM7_BOOTBIN);

	// Moved here to prevent interfering with arm9main's new console
	REG_POWERCNT = 1;  //turn off power to stuffs

	arm7_reset();
}

