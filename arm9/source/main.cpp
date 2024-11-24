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

#include <nds.h>
#include <nds/arm9/dldi.h>
#include <stdio.h>
#include <fat.h>
#include <string.h>
#include <malloc.h>
#include <list>

#include "cheat.h"
#include "ui.h"
#include "nds_card.h"
#include "cheat_engine.h"
#include "crc.h"
#include "version.h"
#include "read_card.h"
#include "tonccpy.h"

const char TITLE_STRING[] = "Nitro Hax " VERSION_STRING "\nWritten by Chishm";

sNDSHeaderExt* ndsHeaderExt;

const char* defaultFiles[] = {
	"usrcheat.dat",
	"/DS/NitroHax/usrcheat.dat",
	"/NitroHax/usrcheat.dat",
	"/data/NitroHax/usrcheat.dat",
	"/usrcheat.dat",
	"/_nds/usrcheat.dat",
	"/_nds/TWiLightMenu/extras/usrcheat.dat"
};

static bool ROMisDSiExclusive(const tNDSHeader* ndsHeader) { return (ndsHeader->unitCode == 0x03); }
static bool ROMisDSiEnhanced(const tNDSHeader* ndsHeader) { return (ndsHeader->unitCode == 0x02); }

static inline void ensure (bool condition, const char* errorMsg) {
	if (!condition) {
		ui.showMessage (errorMsg);
		while(1)swiWaitForVBlank();
	}
	return;
}

void DoWait(int waitTime = 30){
	for (int i = 0; i < waitTime; i++) swiWaitForVBlank();
};

void ResetSlot1() {
	if (REG_SCFG_MC == 0x11) return;
	disableSlot1();
	DoWait();
	enableSlot1();
}

void DoCartCheck() {
	if (REG_SCFG_MC == 0x11) {
		do { swiWaitForVBlank(); } while (REG_SCFG_MC != 0x10);
		enableSlot1();
		DoWait(60);
	}
}

//---------------------------------------------------------------------------------
int main(int argc, const char* argv[]) {
    (void)argc;
    (void)argv;

	u32 ndsHeader[0x80];
	u32* cheatDest;
	int curCheat = 0;
	u32 gameid;
	uint32_t headerCRC;
	std::string filename;
	FILE* cheatFile;
	// bool doFilter=false;
	
	ui.showMessage (UserInterface::TEXT_TITLE, TITLE_STRING);

#ifdef DEMO
	ui.demo();
	while(1);
#endif

	ensure(!isDSiMode() || (REG_SCFG_EXT & BIT(31)), "Nitrohax (for DSi) doesn't have the required permissions to run.");
	// ensure(isDSiMode(), "This version of NitroHax requires DSi/3DS!");
	// ensure((REG_SCFG_EXT & BIT(31)), "Nitrohax doesn't have the required SCFG permissions to run.");
		
	ensure (fatInitDefault(), "FAT init failed");
	
	if (!isDSiMode())ensure((io_dldi_data->ioInterface.features & FEATURE_SLOT_GBA), "Must boot from SLOT2 if on DS/DS Lite!");

	ui.showMessage (UserInterface::TEXT_TITLE, TITLE_STRING);
		
	ui.showMessage ("Loading codes");
	
	// Read cheat file
	for (u32 i = 0; i < sizeof(defaultFiles)/sizeof(const char*); i++) {
		cheatFile = fopen (defaultFiles[i], "rb");
		if (NULL != cheatFile) break;
		// doFilter=true;
	}
	if (NULL == cheatFile) {
		filename = ui.fileBrowser ("usrcheat.dat");
		ensure (filename.size() > 0, "No file specified");
		cheatFile = fopen (filename.c_str(), "rb");
		ensure (cheatFile != NULL, "Couldn't load cheats"); 
	}

	ui.showMessage (UserInterface::TEXT_TITLE, TITLE_STRING);
	
	sysSetCardOwner (BUS_OWNER_ARM9);
	
	// Check if on DSi with unlocked SCFG, if not, then assume standard NTR precedure.
	if (isDSiMode()) {
		ui.showMessage ("Checking if a cart is inserted...");
		if (REG_SCFG_MC != 0x18)ui.showMessage ("Insert Game...");
		while (REG_SCFG_MC != 0x18)DoCartCheck();
		cardInit(ndsHeaderExt);
	} else {
		ui.showMessage ("Loaded codes\nYou can remove your flash card\nRemove DS Card");
		do {
			swiWaitForVBlank();
			getHeader (ndsHeader);
		} while (ndsHeader[0] != 0xffffffff);
	
		ui.showMessage ("Insert Game");
		do {
			swiWaitForVBlank();
			getHeader (ndsHeader);
		} while (ndsHeader[0] == 0xffffffff);
	}

	// Delay half a second for the DS card to stabilise
	DoWait();
	
	ui.showMessage ("Finding game...");
		
	// getHeader (ndsHeader);
	cardReadHeader((u8*)ndsHeader);
	
	ensure(!ROMisDSiExclusive((const tNDSHeader*)ndsHeader), "TWL exclusive games are not supported!");
	
	if (!isDSiMode())ensure(!ROMisDSiEnhanced((const tNDSHeader*)ndsHeader), "TWL Enhanced games not supported on DS/DS Lite!");
	
	gameid = ndsHeader[3];
	headerCRC = crc32((const char*)ndsHeader, sizeof(ndsHeader));

	ui.showMessage ("Loading codes...");
	
	CheatCodelist* codelist = new CheatCodelist();
	ensure (codelist->load(cheatFile, gameid, headerCRC/*, doFilter*/), "Game not found in cheat list!\n");
	fclose (cheatFile);
	CheatFolder *gameCodes = codelist->getGame (gameid, headerCRC);
	
	if (!gameCodes)gameCodes = codelist;
	
	if(codelist->getContents().empty()) {
		filename = ui.fileBrowser ("usrcheat.dat");
		ensure (filename.size() > 0, "No file specified");
		cheatFile = fopen (filename.c_str(), "rb");
		ensure (cheatFile != NULL, "Couldn't load cheats");
		
		ui.showMessage ("Loading codes...");
		
		CheatCodelist* codelist = new CheatCodelist();
		ensure (codelist->load(cheatFile, gameid, headerCRC/*, doFilter*/), "Game not found in cheat list!\n");
		fclose (cheatFile);
		gameCodes = codelist->getGame (gameid, headerCRC);
		
		if (!gameCodes)gameCodes = codelist;
	}

	ui.cheatMenu (gameCodes, gameCodes);
	
	cheatDest = (u32*) malloc(CHEAT_MAX_DATA_SIZE);
	ensure (cheatDest != NULL, "Bad malloc\n");
	
	std::vector<CheatWord> cheatList = gameCodes->getEnabledCodeData();
	
	for (std::vector<CheatWord>::iterator cheat = cheatList.begin(); cheat != cheatList.end(); cheat++)cheatDest[curCheat++] = (*cheat);

	ui.showMessage (UserInterface::TEXT_TITLE, TITLE_STRING);
	ui.showMessage ("Running game");

	runCheatEngine (cheatDest, curCheat * sizeof(u32), ROMisDSiEnhanced((const tNDSHeader*)ndsHeader));

	while(1)swiWaitForVBlank();

	return 0;
}

