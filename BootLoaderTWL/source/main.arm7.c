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
#include <nds/arm7/codec.h>
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
#include "tonccpy.h"
#include "read_card.h"
#include "module_params.h"
#include "find.h"
#include "cheat.h"

void arm7_clearmem (void* loc, size_t len);
extern void ensureBinaryDecompressed(const tNDSHeader* ndsHeader, module_params_t* moduleParams);

// Module params
static const u32 moduleParamsSignature[2] = {0xDEC00621, 0x2106C0DE};

static u32 chipID;

static module_params_t* moduleParams;

u32* findModuleParamsOffset(const tNDSHeader* ndsHeader) {
	u32* moduleParamsOffset = findOffset((u32*)ndsHeader->arm9destination, ndsHeader->arm9binarySize, moduleParamsSignature, 2);
	return moduleParamsOffset;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Important things
#define NDS_HEADER         0x027FFE00
#define NDS_HEADER_SDK5    0x02FFFE00 // __NDSHeader
#define NDS_HEADER_POKEMON 0x027FF000

#define DSI_HEADER         0x027FE000
#define DSI_HEADER_SDK5    0x02FFE000 // __DSiHeader

// #define CHEAT_ENGINE_LOCATION	0x027FE000
#define CHEAT_ENGINE_LOCATION	0x023FE800
#define CHEAT_DATA_LOCATION  	0x06030000

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Used for debugging purposes
static void debugOutput (u32 code) {
	// Wait until the ARM9 is ready
	while (arm9_stateFlag != ARM9_READY);
	// Set the error code, then tell ARM9 to display it
	arm9_errorCode = code;	
	arm9_stateFlag = ARM9_DISPERR;
	// Wait for completion	
	while (arm9_stateFlag != ARM9_READY);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Firmware stuff

static void my_readUserSettings(tNDSHeader* ndsHeader) {
	PERSONAL_DATA slot1;
	PERSONAL_DATA slot2;

	short slot1count, slot2count; //u8
	short slot1CRC, slot2CRC;

	u32 userSettingsBase;

	// Get settings location
	readFirmware(0x20, &userSettingsBase, 2);

	u32 slot1Address = userSettingsBase * 8;
	u32 slot2Address = userSettingsBase * 8 + 0x100;

	// Reload DS Firmware settings
	readFirmware(slot1Address, &slot1, sizeof(PERSONAL_DATA)); //readFirmware(slot1Address, personalData, 0x70);
	readFirmware(slot2Address, &slot2, sizeof(PERSONAL_DATA)); //readFirmware(slot2Address, personalData, 0x70);
	readFirmware(slot1Address + 0x70, &slot1count, 2); //readFirmware(slot1Address + 0x70, &slot1count, 1);
	readFirmware(slot2Address + 0x70, &slot2count, 2); //readFirmware(slot1Address + 0x70, &slot2count, 1);
	readFirmware(slot1Address + 0x72, &slot1CRC, 2);
	readFirmware(slot2Address + 0x72, &slot2CRC, 2);

	// Default to slot 1 user settings
	void *currentSettings = &slot1;

	short calc1CRC = swiCRC16(0xFFFF, &slot1, sizeof(PERSONAL_DATA));
	short calc2CRC = swiCRC16(0xFFFF, &slot2, sizeof(PERSONAL_DATA));

	// Bail out if neither slot is valid
	if (calc1CRC != slot1CRC && calc2CRC != slot2CRC)return;

	// If both slots are valid pick the most recent
	if (calc1CRC == slot1CRC && calc2CRC == slot2CRC) { 
		currentSettings = (slot2count == ((slot1count + 1) & 0x7f) ? &slot2 : &slot1);
		//if ((slot1count & 0x7F) == ((slot2count + 1) & 0x7F)) {
	} else {
		if (calc2CRC == slot2CRC)currentSettings = &slot2;
	}

	PERSONAL_DATA* personalData = (PERSONAL_DATA*)((u32)__NDSHeader - (u32)ndsHeader + (u32)PersonalData); //(u8*)((u32)ndsHeader - 0x180)

	tonccpy(PersonalData, currentSettings, sizeof(PERSONAL_DATA));

	if (personalData->language != 6 && ndsHeader->reserved1[8] == 0x80) {
		ndsHeader->reserved1[8] = 0;	// Patch iQue game to be region-free
		ndsHeader->headerCRC16 = swiCRC16(0xFFFF, ndsHeader, 0x15E);	// Fix CRC
	}
}

void memset_addrs_arm7(u32 start, u32 end) { toncset((u32*)start, 0, ((int)end - (int)start)); }

/*-------------------------------------------------------------------------
arm7_resetMemory
Clears all of the NDS's RAM that is visible to the ARM7
Written by Darkain.
Modified by Chishm:
 * Added STMIA clear mem loop
--------------------------------------------------------------------------*/
void arm7_resetMemory (void) {
	int i, reg;

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
		if ((REG_SCFG_EXT & BIT(31)))for(reg=0; reg<0x1c; reg+=4)*((u32*)(0x04004104 + ((i*0x1c)+reg))) = 0; //Reset NDMA.
	}
	// Clear out FIFO
	REG_IPC_SYNC = 0;
	REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
	REG_IPC_FIFO_CR = 0;
	// clear IWRAM - 037F:8000 to 0380:FFFF, total 96KiB
	toncset ((void*)0x037F8000, 0, 96*1024);	
	memset_addrs_arm7(0x03000000, 0x0380FFC0);
	memset_addrs_arm7(0x0380FFD0, 0x03800000 + 0x10000);	
	// clear most of EXRAM - except before 0x023F0000, which has the cheat data
	toncset ((void*)0x02004000, 0, 0x3EC000);
	// clear more of EXRAM, skipping the cheat data section
	toncset ((void*)0x023F8000, 0, 0x6000);
	// clear last part of EXRAM
	toncset ((void*)0x02400000, 0, 0xC00000);
	REG_IE = 0;
	REG_IF = ~0;
	REG_AUXIE = 0;
	REG_AUXIF = ~0;
	(*(vu32*)(0x04000000-4)) = 0;  //IRQ_HANDLER ARM7 version
	(*(vu32*)(0x04000000-8)) = ~0; //VBLANK_INTR_WAIT_FLAGS, ARM7 version
	REG_POWERCNT = 1;  //turn off power to stuffs
}

// SDK 5
static bool ROMsupportsDSiMode(const tNDSHeader* ndsHeader) { return (ndsHeader->unitCode > 0); }

// SDK 5
static bool ROMisDSiEnhanced(const tNDSHeader* ndsHeader) { return (ndsHeader->unitCode == 0x02); }

// SDK 5
static bool ROMisDSiExclusive(const tNDSHeader* ndsHeader) { return (ndsHeader->unitCode == 0x03); }

int arm7_loadBinary (const tDSiHeader* dsiHeaderTemp) {
	u32 errorCode;
	// Init card
	errorCode = cardInit((sNDSHeaderExt*)dsiHeaderTemp, &chipID);
	if (errorCode)return errorCode;
	// Fix Pokemon games needing header data.
	tonccpy((u32*)NDS_HEADER_POKEMON, (u32*)NDS_HEADER, 0x170);
	char* romTid = (char*)NDS_HEADER_POKEMON+0xC;
	if (
		memcmp(romTid, "ADA", 3) == 0    // Diamond
		|| memcmp(romTid, "APA", 3) == 0 // Pearl
		|| memcmp(romTid, "CPU", 3) == 0 // Platinum
		|| memcmp(romTid, "IPK", 3) == 0 // HG
		|| memcmp(romTid, "IPG", 3) == 0 // SS
	) {
		// Make the Pokemon game code ADAJ.
		const char gameCodePokemon[] = { 'A', 'D', 'A', 'J' };
		tonccpy((char*)NDS_HEADER_POKEMON+0xC, gameCodePokemon, 4);
	}
	cardRead(dsiHeaderTemp->ndshdr.arm9romOffset, (u32*)dsiHeaderTemp->ndshdr.arm9destination, dsiHeaderTemp->ndshdr.arm9binarySize);
	cardRead(dsiHeaderTemp->ndshdr.arm7romOffset, (u32*)dsiHeaderTemp->ndshdr.arm7destination, dsiHeaderTemp->ndshdr.arm7binarySize);
	moduleParams = (module_params_t*)findModuleParamsOffset(&dsiHeaderTemp->ndshdr);

	return ERR_NONE;
}

static tNDSHeader* loadHeader(tDSiHeader* dsiHeaderTemp) {
	tNDSHeader* ndsHeader = (tNDSHeader*)(isSdk5(moduleParams) ? NDS_HEADER_SDK5 : NDS_HEADER);
	*ndsHeader = dsiHeaderTemp->ndshdr;
	return ndsHeader;
}

/*-------------------------------------------------------------------------
arm7_startBinary
Jumps to the ARM7 NDS binary in sync with the display and ARM9
Written by Darkain, modified by Chishm.
--------------------------------------------------------------------------*/
void arm7_startBinary (void) {
	REG_IME = 0;
	// Get the ARM9 to boot
	arm9_stateFlag = ARM9_BOOTBIN;
	while(REG_VCOUNT!=191);
	while(REG_VCOUNT==191);
	// Start ARM7
	VoidFn arm7code = (VoidFn)ndsHeader->arm7executeAddress;
	arm7code();
}

static void setMemoryAddress(const tNDSHeader* ndsHeader) {
	if (ROMsupportsDSiMode(ndsHeader)) {
		tonccpy((u32*)0x02FFFA80, (u32*)NDS_HEADER_SDK5, 0x160);	// Make a duplicate of DS header
		*(u32*)(0x02FFA680) = 0x02FD4D80;
		*(u32*)(0x02FFA684) = 0x00000000;
		*(u32*)(0x02FFA688) = 0x00001980;
		*(u32*)(0x02FFF00C) = 0x0000007F;
		*(u32*)(0x02FFF010) = 0x550E25B8;
		*(u32*)(0x02FFF014) = 0x02FF4000;
	}

    // Set memory values expected by loaded NDS
    // from NitroHax, thanks to Chism
	*((u32*)(isSdk5(moduleParams) ? 0x02fff800 : 0x027ff800)) = chipID;					// CurrentCardID
	*((u32*)(isSdk5(moduleParams) ? 0x02fff804 : 0x027ff804)) = chipID;					// Command10CardID
	*((u16*)(isSdk5(moduleParams) ? 0x02fff808 : 0x027ff808)) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)(isSdk5(moduleParams) ? 0x02fff80a : 0x027ff80a)) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]

	// Copies of above
	*((u32*)(isSdk5(moduleParams) ? 0x02fffc00 : 0x027ffc00)) = chipID;					// CurrentCardID
	*((u32*)(isSdk5(moduleParams) ? 0x02fffc04 : 0x027ffc04)) = chipID;					// Command10CardID
	*((u16*)(isSdk5(moduleParams) ? 0x02fffc08 : 0x027ffc08)) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)(isSdk5(moduleParams) ? 0x02fffc0a : 0x027ffc0a)) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]

	*((u16*)(isSdk5(moduleParams) ? 0x02fffc40 : 0x027ffc40)) = 0x1;						// Boot Indicator (Booted from card for SDK5) -- EXTREMELY IMPORTANT!!! Thanks to cReDiAr
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Main function

void arm7_main (void) {

	if (REG_SNDEXTCNT != 0) {
		if ((REG_SCFG_EXT & BIT(31))) {
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
	
	int errorCode;
	
	// Wait for ARM9 to at least start
	while (arm9_stateFlag < ARM9_START);

	// debugOutput (ERR_STS_CLR_MEM);
	
	// Get ARM7 to clear RAM
	arm7_resetMemory();	

	// debugOutput (ERR_STS_LOAD_BIN);
	
	if ((REG_SCFG_EXT & BIT(31)))REG_SCFG_ROM = 0x703;
	
	tDSiHeader* dsiHeaderTemp = (tDSiHeader*)0x02FFC000;

	// Load the NDS file
	errorCode = arm7_loadBinary(dsiHeaderTemp);
	if (errorCode)debugOutput(errorCode);
	
	// Override some settings depending on if DSi Enhanced cart or DSi Exclusive cart is inserted
	// if (ROMisDSiEnhanced(&dsiHeaderTemp->ndshdr)) { extendRam = true; } // Required for TWL carts to boot properly. Disabled by default for NTR carts to allow WoodR4 to operate correctly.
	
	/*if (twlMode) {
		if (dsiHeaderTemp->arm9ibinarySize > 0) {			
			cardRead((u32)dsiHeaderTemp->arm9iromOffset, (u32*)dsiHeaderTemp->arm9idestination, dsiHeaderTemp->arm9ibinarySize);
		}
		if (dsiHeaderTemp->arm7ibinarySize > 0) {
			cardRead((u32)dsiHeaderTemp->arm7iromOffset, (u32*)dsiHeaderTemp->arm7idestination, dsiHeaderTemp->arm7ibinarySize);
		}
	}*/

	ndsHeader = loadHeader(dsiHeaderTemp);
	
	if (!ROMisDSiExclusive(&dsiHeaderTemp->ndshdr)) { tonccpy((u32*)0x023FF000, (u32*)(isSdk5(moduleParams) ? 0x02FFF000 : 0x027FF000), 0x1000); }
	
	my_readUserSettings(ndsHeader); // Header has to be loaded first
	
	toncset ((void*)0x023F0000, 0, 0x8000);		// Clear cheat data from main memory
	
	// debugOutput (ERR_STS_START);
	
	if (ROMisDSiExclusive(&dsiHeaderTemp->ndshdr)) { REG_SCFG_CLK = 0x0184; } else { REG_SCFG_CLK = 0x0100; }
	
	if (REG_SNDEXTCNT != 0) {
		if ((REG_SCFG_EXT & BIT(31)))REG_SCFG_EXT = 0x12A00000;
		//REG_SCFG_EXT &= ~(1UL << 31);
	}
	
	while (arm9_stateFlag != ARM9_READY);
	arm9_stateFlag = ARM9_SETSCFG;
	while (arm9_stateFlag != ARM9_READY);
		
	setMemoryAddress(ndsHeader);
	
	// Load the cheat engine and hook it into the ARM7 binary
	errorCode = arm7_hookGame(ndsHeader, (const u32*)CHEAT_DATA_LOCATION, (u32*)CHEAT_ENGINE_LOCATION);
	if (errorCode != ERR_NONE && errorCode != ERR_NOCHEAT)debugOutput(errorCode);
	
	arm7_startBinary();

	while (1);
}

