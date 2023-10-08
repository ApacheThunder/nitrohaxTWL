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
#include <nds/arm7/i2c.h>
#include <nds/ipc.h>
#include <string.h>



#ifndef NULL
#define NULL 0
#endif

#include "common.h"
#include "read_card.h"
#include "cheat.h"
#include "module_params.h"
#include "find.h"
#include "tonccpy.h"

/*-------------------------------------------------------------------------
External functions
--------------------------------------------------------------------------*/
extern void arm7_clearmem (void* loc, size_t len);
extern void arm7_reset (void);

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Important things
#define NDS_HEADER		   0x027FFE00
#define NDS_HEADER_SDK5    0x02FFFE00 // __NDSHeader
#define NDS_HEADER_POKEMON 0x027FF000
#define DSI_HEADER         0x027FE000
#define DSI_HEADER_SDK5    0x02FFE000 // __DSiHeader

#define REG_GPIO_WIFI *(vu16*)0x4004C04

// #define CHEAT_ENGINE_LOCATION	0x023FE800
#define CHEAT_ENGINE_LOCATION	0x027FE000
#define CHEAT_DATA_LOCATION  	0x06030000

static u32 chipID;
// Module params
static const u32 moduleParamsSignature[2] = {0xDEC00621, 0x2106C0DE};

// SDK 5
static bool ROMsupportsDsiMode(const tNDSHeader* ndsHeader) { return (ndsHeader->unitCode > 0); }

void memset_addrs_arm7(u32 start, u32 end) { toncset((u32*)start, 0, ((int)end - (int)start)); }

static module_params_t* moduleParams;

u32* findModuleParamsOffset(const tNDSHeader* ndsHeader) {
	u32* moduleParamsOffset = findOffset((u32*)ndsHeader->arm9destination, ndsHeader->arm9binarySize, moduleParamsSignature, 2);
	return moduleParamsOffset;
}

static tNDSHeader* loadHeader(tDSiHeader* dsiHeaderTemp) {
	tNDSHeader* ndsHeader = (tNDSHeader*)(isSdk5(moduleParams) ? NDS_HEADER_SDK5 : NDS_HEADER);

	*ndsHeader = dsiHeaderTemp->ndshdr;
	if (REG_SNDEXTCNT != 0 && (REG_SCFG_EXT & BIT(31))) {
		tDSiHeader* dsiHeader = (tDSiHeader*)(isSdk5(moduleParams) ? DSI_HEADER_SDK5 : DSI_HEADER); // __DSiHeader
		*dsiHeader = *dsiHeaderTemp;
	}

	return ndsHeader;
}

const char* getRomTid(const tNDSHeader* ndsHeader) {
	static char romTid[5];
	strncpy(romTid, ndsHeader->gameCode, 4);
	romTid[4] = '\0';
	return romTid;
}


static void NDSTouchscreenMode(void) {
	bool specialSetting = false;
	u8 volLevel;

	if (specialSetting) {
		// special setting (when found special gamecode)
		volLevel = 0xAC;
	} else {
		// normal setting (for any other gamecodes)
		volLevel = 0xA7;
	}

	// Touchscreen
	cdcReadReg (0x63, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x3A, 0x00);
	cdcReadReg (CDC_CONTROL, 0x51);
	cdcReadReg (CDC_TOUCHCNT, 0x02);
	cdcReadReg (CDC_CONTROL, 0x3F);
	cdcReadReg (CDC_SOUND, 0x28);
	cdcReadReg (CDC_SOUND, 0x2A);
	cdcReadReg (CDC_SOUND, 0x2E);
	cdcWriteReg(CDC_CONTROL, 0x52, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x40, 0x0C);
	cdcWriteReg(CDC_SOUND, 0x24, 0xFF);
	cdcWriteReg(CDC_SOUND, 0x25, 0xFF);
	cdcWriteReg(CDC_SOUND, 0x26, 0x7F);
	cdcWriteReg(CDC_SOUND, 0x27, 0x7F);
	cdcWriteReg(CDC_SOUND, 0x28, 0x4A);
	cdcWriteReg(CDC_SOUND, 0x29, 0x4A);
	cdcWriteReg(CDC_SOUND, 0x2A, 0x10);
	cdcWriteReg(CDC_SOUND, 0x2B, 0x10);
	cdcWriteReg(CDC_CONTROL, 0x51, 0x00);
	cdcReadReg (CDC_TOUCHCNT, 0x02);
	cdcWriteReg(CDC_TOUCHCNT, 0x02, 0x98);
	cdcWriteReg(CDC_SOUND, 0x23, 0x00);
	cdcWriteReg(CDC_SOUND, 0x1F, 0x14);
	cdcWriteReg(CDC_SOUND, 0x20, 0x14);
	cdcWriteReg(CDC_CONTROL, 0x3F, 0x00);
	cdcReadReg (CDC_CONTROL, 0x0B);
	cdcWriteReg(CDC_CONTROL, 0x05, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x0B, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x0C, 0x02);
	cdcWriteReg(CDC_CONTROL, 0x12, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x13, 0x02);
	cdcWriteReg(CDC_SOUND, 0x2E, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x3A, 0x60);
	cdcWriteReg(CDC_CONTROL, 0x01, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x39, 0x66);
	cdcReadReg (CDC_SOUND, 0x20);
	cdcWriteReg(CDC_SOUND, 0x20, 0x10);
	cdcWriteReg(CDC_CONTROL, 0x04, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x12, 0x81);
	cdcWriteReg(CDC_CONTROL, 0x13, 0x82);
	cdcWriteReg(CDC_CONTROL, 0x51, 0x82);
	cdcWriteReg(CDC_CONTROL, 0x51, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x04, 0x03);
	cdcWriteReg(CDC_CONTROL, 0x05, 0xA1);
	cdcWriteReg(CDC_CONTROL, 0x06, 0x15);
	cdcWriteReg(CDC_CONTROL, 0x0B, 0x87);
	cdcWriteReg(CDC_CONTROL, 0x0C, 0x83);
	cdcWriteReg(CDC_CONTROL, 0x12, 0x87);
	cdcWriteReg(CDC_CONTROL, 0x13, 0x83);
	cdcReadReg (CDC_TOUCHCNT, 0x10);
	cdcWriteReg(CDC_TOUCHCNT, 0x10, 0x08);
	cdcWriteReg(0x04, 0x08, 0x7F);
	cdcWriteReg(0x04, 0x09, 0xE1);
	cdcWriteReg(0x04, 0x0A, 0x80);
	cdcWriteReg(0x04, 0x0B, 0x1F);
	cdcWriteReg(0x04, 0x0C, 0x7F);
	cdcWriteReg(0x04, 0x0D, 0xC1);
	cdcWriteReg(CDC_CONTROL, 0x41, 0x08);
	cdcWriteReg(CDC_CONTROL, 0x42, 0x08);
	cdcWriteReg(CDC_CONTROL, 0x3A, 0x00);
	cdcWriteReg(0x04, 0x08, 0x7F);
	cdcWriteReg(0x04, 0x09, 0xE1);
	cdcWriteReg(0x04, 0x0A, 0x80);
	cdcWriteReg(0x04, 0x0B, 0x1F);
	cdcWriteReg(0x04, 0x0C, 0x7F);
	cdcWriteReg(0x04, 0x0D, 0xC1);
	cdcWriteReg(CDC_SOUND, 0x2F, 0x2B);
	cdcWriteReg(CDC_SOUND, 0x30, 0x40);
	cdcWriteReg(CDC_SOUND, 0x31, 0x40);
	cdcWriteReg(CDC_SOUND, 0x32, 0x60);
	cdcReadReg (CDC_CONTROL, 0x74);
	cdcWriteReg(CDC_CONTROL, 0x74, 0x02);
	cdcReadReg (CDC_CONTROL, 0x74);
	cdcWriteReg(CDC_CONTROL, 0x74, 0x10);
	cdcReadReg (CDC_CONTROL, 0x74);
	cdcWriteReg(CDC_CONTROL, 0x74, 0x40);
	cdcWriteReg(CDC_SOUND, 0x21, 0x20);
	cdcWriteReg(CDC_SOUND, 0x22, 0xF0);
	cdcReadReg (CDC_CONTROL, 0x51);
	cdcReadReg (CDC_CONTROL, 0x3F);
	cdcWriteReg(CDC_CONTROL, 0x3F, 0xD4);
	cdcWriteReg(CDC_SOUND, 0x23, 0x44);
	cdcWriteReg(CDC_SOUND, 0x1F, 0xD4);
	cdcWriteReg(CDC_SOUND, 0x28, 0x4E);
	cdcWriteReg(CDC_SOUND, 0x29, 0x4E);
	cdcWriteReg(CDC_SOUND, 0x24, 0x9E);
	cdcWriteReg(CDC_SOUND, 0x25, 0x9E);
	cdcWriteReg(CDC_SOUND, 0x20, 0xD4);
	cdcWriteReg(CDC_SOUND, 0x2A, 0x14);
	cdcWriteReg(CDC_SOUND, 0x2B, 0x14);
	cdcWriteReg(CDC_SOUND, 0x26, 0xA7);
	cdcWriteReg(CDC_SOUND, 0x27, 0xA7);
	cdcWriteReg(CDC_CONTROL, 0x40, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x3A, 0x60);
	cdcWriteReg(CDC_SOUND, 0x26, volLevel);
	cdcWriteReg(CDC_SOUND, 0x27, volLevel);
	cdcWriteReg(CDC_SOUND, 0x2E, 0x03);
	cdcWriteReg(CDC_TOUCHCNT, 0x03, 0x00);
	cdcWriteReg(CDC_SOUND, 0x21, 0x20);
	cdcWriteReg(CDC_SOUND, 0x22, 0xF0);
	cdcReadReg (CDC_SOUND, 0x22);
	cdcWriteReg(CDC_SOUND, 0x22, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x52, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x51, 0x00);
	
	// Set remaining values
	cdcWriteReg(CDC_CONTROL, 0x03, 0x44);
	cdcWriteReg(CDC_CONTROL, 0x0D, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x0E, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x0F, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x10, 0x08);
	cdcWriteReg(CDC_CONTROL, 0x14, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x15, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x16, 0x04);
	cdcWriteReg(CDC_CONTROL, 0x1A, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x1E, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x24, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x33, 0x34);
	cdcWriteReg(CDC_CONTROL, 0x34, 0x32);
	cdcWriteReg(CDC_CONTROL, 0x35, 0x12);
	cdcWriteReg(CDC_CONTROL, 0x36, 0x03);
	cdcWriteReg(CDC_CONTROL, 0x37, 0x02);
	cdcWriteReg(CDC_CONTROL, 0x38, 0x03);
	cdcWriteReg(CDC_CONTROL, 0x3C, 0x19);
	cdcWriteReg(CDC_CONTROL, 0x3D, 0x05);
	cdcWriteReg(CDC_CONTROL, 0x44, 0x0F);
	cdcWriteReg(CDC_CONTROL, 0x45, 0x38);
	cdcWriteReg(CDC_CONTROL, 0x49, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x4A, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x4B, 0xEE);
	cdcWriteReg(CDC_CONTROL, 0x4C, 0x10);
	cdcWriteReg(CDC_CONTROL, 0x4D, 0xD8);
	cdcWriteReg(CDC_CONTROL, 0x4E, 0x7E);
	cdcWriteReg(CDC_CONTROL, 0x4F, 0xE3);
	cdcWriteReg(CDC_CONTROL, 0x58, 0x7F);
	cdcWriteReg(CDC_CONTROL, 0x74, 0xD2);
	cdcWriteReg(CDC_CONTROL, 0x75, 0x2C);
	cdcWriteReg(CDC_SOUND, 0x22, 0x70);
	cdcWriteReg(CDC_SOUND, 0x2C, 0x20);

	// Finish up!
	cdcReadReg (CDC_TOUCHCNT, 0x02);
	cdcWriteReg(CDC_TOUCHCNT, 0x02, 0x98);
	cdcWriteReg(0xFF, 0x05, 0x00); //writeTSC(0x00, 0xFF);

	// Power management
	writePowerManagement(PM_READ_REGISTER, 0x00); //*(unsigned char*)0x40001C2 = 0x80, 0x00; // read PWR[0]   ;<-- also part of TSC !
	writePowerManagement(PM_CONTROL_REG, 0x0D); //*(unsigned char*)0x40001C2 = 0x00, 0x0D; // PWR[0]=0Dh    ;<-- also part of TSC !
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Used for debugging purposes
static void errorOutput (u32 code, bool isError) {
	// Set the error code, then set our state to "error"
	// If it's just a status readout we don't send IPC or stay in while loop for eternaty.
	arm9_errorCode = code;
	if (isError) { 
		ipcSendState(ARM7_ERR);
		while(1);// Stop
	}
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
		currentSettings = (slot2count == ((slot1count + 1) & 0x7f) ? &slot2 : &slot1); //if ((slot1count & 0x7F) == ((slot2count + 1) & 0x7F)) {
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
		if (REG_SNDEXTCNT != 0 && (REG_SCFG_EXT & BIT(31)))for(reg=0; reg<0x1c; reg+=4)*((u32*)(0x04004104 + ((i*0x1c)+reg))) = 0; //Reset NDMA.
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
	if (REG_SNDEXTCNT != 0) {
		REG_AUXIE = 0;
		REG_AUXIF = ~0;
	}
	(*(vu32*)(0x04000000-4)) = 0;  //IRQ_HANDLER ARM7 version
	(*(vu32*)(0x04000000-8)) = ~0; //VBLANK_INTR_WAIT_FLAGS, ARM7 version
}

u32 arm7_loadBinary (const tDSiHeader* dsiHeaderTemp) {
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

static void setMemoryAddress(const tNDSHeader* ndsHeader) {
	if (ROMsupportsDsiMode(ndsHeader)) {
		tonccpy((u32*)0x02FFFA80, (u32*)NDS_HEADER_SDK5, 0x160);	// Make a duplicate of DS header

		*(u32*)(0x02FFA680) = 0x02FD4D80;
		*(u32*)(0x02FFA684) = 0x00000000;
		*(u32*)(0x02FFA688) = 0x00001980;

		*(u32*)(0x02FFF00C) = 0x0000007F;
		*(u32*)(0x02FFF010) = 0x550E25B8;
		*(u32*)(0x02FFF014) = 0x02FF4000;

		// Set region flag
		if (strncmp(getRomTid(ndsHeader)+3, "J", 1) == 0) {
			*(u8*)(0x02FFFD70) = 0;
		} else if (strncmp(getRomTid(ndsHeader)+3, "E", 1) == 0) {
			*(u8*)(0x02FFFD70) = 1;
		} else if (strncmp(getRomTid(ndsHeader)+3, "P", 1) == 0) {
			*(u8*)(0x02FFFD70) = 2;
		} else if (strncmp(getRomTid(ndsHeader)+3, "U", 1) == 0) {
			*(u8*)(0x02FFFD70) = 3;
		} else if (strncmp(getRomTid(ndsHeader)+3, "C", 1) == 0) {
			*(u8*)(0x02FFFD70) = 4;
		} else if (strncmp(getRomTid(ndsHeader)+3, "K", 1) == 0) {
			*(u8*)(0x02FFFD70) = 5;
		}

		/*if ((REG_SCFG_EXT & BIT(31)) && REG_SNDEXTCNT != 0) {
			i2cWriteRegister(I2C_PM, I2CREGPM_MMCPWR, 1);		// Have IRQ check for power button press
			i2cWriteRegister(I2C_PM, I2CREGPM_RESETFLAG, 1);		// SDK 5 --> Bootflag = Warmboot/SkipHealthSafety
		}*/
	}

    // Set memory values expected by loaded NDS
    // from NitroHax, thanks to Chism
	*((u32*)(isSdk5(moduleParams) ? 0x02fff800 : 0x027ff800)) = chipID;					// CurrentCardID
	*((u32*)(isSdk5(moduleParams) ? 0x02fff804 : 0x027ff804)) = chipID;					// Command10CardID
	*((u16*)(isSdk5(moduleParams) ? 0x02fff808 : 0x027ff808)) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)(isSdk5(moduleParams) ? 0x02fff80a : 0x027ff80a)) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]

	*((u16*)(isSdk5(moduleParams) ? 0x02fff850 : 0x027ff850)) = 0x5835;

	// Copies of above
	*((u32*)(isSdk5(moduleParams) ? 0x02fffc00 : 0x027ffc00)) = chipID;					// CurrentCardID
	*((u32*)(isSdk5(moduleParams) ? 0x02fffc04 : 0x027ffc04)) = chipID;					// Command10CardID
	*((u16*)(isSdk5(moduleParams) ? 0x02fffc08 : 0x027ffc08)) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)(isSdk5(moduleParams) ? 0x02fffc0a : 0x027ffc0a)) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]

	*((u16*)(isSdk5(moduleParams) ? 0x02fffc10 : 0x027ffc10)) = 0x5835;

	*((u16*)(isSdk5(moduleParams) ? 0x02fffc40 : 0x027ffc40)) = 0x1;						// Boot Indicator (Booted from card for SDK5) -- EXTREMELY IMPORTANT!!! Thanks to cReDiAr
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Main function

void arm7_main (void) {
	u32 errorCode;
	
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
	
	// Synchronise start
	while (ipcRecvState() != ARM9_START);
	
	ipcSendState(ARM7_START);

	// Wait until ARM9 is ready
	while (ipcRecvState() != ARM9_READY);

	ipcSendState(ARM7_MEMCLR);
	
	errorOutput(ERR_STS_CLR_MEM, false);

	// Get ARM7 to clear RAM
	arm7_resetMemory();

	if (REG_SNDEXTCNT != 0) {
		if (REG_SCFG_EXT & BIT(31))REG_SCFG_ROM = 0x703;
	}
	
	tDSiHeader* dsiHeaderTemp = (tDSiHeader*)0x02FFC000;
	
	errorOutput(ERR_STS_LOAD_BIN, false);
	
	ipcSendState(ARM7_LOADBIN);

	// Load the NDS file
	errorCode = arm7_loadBinary(dsiHeaderTemp);
	if (errorCode)errorOutput(errorCode, true);
	
	ndsHeader = loadHeader(dsiHeaderTemp);
	
	tonccpy((u32*)0x023FF000, (u32*)(isSdk5(moduleParams) ? 0x02FFF000 : 0x027FF000), 0x1000);
	
	if (REG_SNDEXTCNT != 0) {
		if (REG_SCFG_EXT & BIT(31))REG_SCFG_CLK = 0x100;
	}
	
	my_readUserSettings(ndsHeader); // Header has to be loaded first
	
	if (REG_SNDEXTCNT != 0) {
		if (REG_SCFG_EXT & BIT(31)) {
			NDSTouchscreenMode();
			*(u16*)0x4000500 = 0x807F;
			REG_GPIO_WIFI |= BIT(8);	// Old NDS-Wifi mode
		}
	}
		
	arm9_isSdk5 = isSdk5(moduleParams);
		
	if ((REG_SCFG_EXT & BIT(31)) && REG_SNDEXTCNT != 0)REG_SCFG_EXT = 0x12A00000;
	
	ipcSendState(ARM7_SETSCFG);
	while (ipcRecvState() != ARM9_SETSCFG);
	
	setMemoryAddress(ndsHeader);
	
	errorOutput(ERR_STS_HOOK_BIN, false);
	
	ipcSendState(ARM7_HOOKBIN);
	// Load the cheat engine and hook it into the ARM7 binary
	errorCode = arm7_hookGame(ndsHeader, (const u32*)CHEAT_DATA_LOCATION, (u32*)CHEAT_ENGINE_LOCATION);
	if (errorCode != ERR_NONE && errorCode != ERR_NOCHEAT)errorOutput(errorCode, true);
	
	ipcSendState(ARM7_BOOTBIN);
	
	// Moved here to prevent interfering with arm9main's new console
	REG_POWERCNT = 1;  //turn off power to stuffs
	
	arm7_reset();
}

