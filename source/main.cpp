/*
main.cpp --- Wii Shop Channel Ticket Restorer
Copyright (C) 2025  Juan de la Cruz Caravaca Guerrero (Quadraxis_v2)
juan.dlcruzcg@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <cstdio>
#include <ios>
#include <string>
#include <cstdlib>

#include <gccore.h>
#include <ogc/pad.h>
#include <wiiuse/wpad.h>
#include <gctypes.h>
#include <gcutil.h>
#include <ogc/isfs.h>
#include <ogc/wiilaunch.h>

#include "libpatcher.hpp"

static void* SpXfb{nullptr};	
static GXRModeObj* SpGXRmode{nullptr};


void Initialise() noexcept;
void PrepareExit() noexcept;


//---------------------------------------------------------------------------------
int main(int argc, char **argv) 
{
//---------------------------------------------------------------------------------
	Initialise();

	std::printf("\x1b[2;0H");
	std::puts("Wii Shop Channel Ticket Restorer v1.0");
	std::puts("-------------------------------------");

	s32 iFileDescriptor{-1};

	bool bIsKorean{false};

	try
	{
		/* The EC file is a WSC file containing some key-value pairs related to the synchronization of tickets. */

		std::puts("Opening EC file...");

		const std::string CsPathWorld{"/title/00010002/48414241/data/ec.cfg"};
		const std::string CsPathKorea{"/title/00010002/4841424B/data/ec.cfg"};

		s32 iError{ISFS_OK};

		if ((iFileDescriptor = ISFS_Open(CsPathWorld.c_str(), ISFS_OPEN_RW)) < 0)	// World
		{
			if ((iFileDescriptor = ISFS_Open(CsPathKorea.c_str(), ISFS_OPEN_RW)) < 0)	// Korea
				throw std::ios_base::failure(std::string{"Error opening ec.cfg, ret = "} + 
					std::to_string(iFileDescriptor));
			else bIsKorean = true;
		}

		fstats fStatsEC ATTRIBUTE_ALIGN(32) {};

		if ((iError = ISFS_GetFileStats(iFileDescriptor, &fStatsEC)) < 0)
			throw std::ios_base::failure(std::string{"Error getting file stats from ec.cfg, ret = "} + 
				std::to_string(iError));

		char acECContent[fStatsEC.file_length] ATTRIBUTE_ALIGN(32) {};

		std::puts("Reading EC file...");

		if ((iError = ISFS_Read(iFileDescriptor, acECContent, fStatsEC.file_length)) < 0)
			throw std::ios_base::failure(std::string{"Error reading ec.cfg, ret = "} + 
				std::to_string(iError));

		std::string sFile{acECContent, fStatsEC.file_length};
		u32 uiPosition = sFile.find("isNeedTicketSync\0", 0, 17);

		std::puts("Patching EC file...");

		if (uiPosition == std::string::npos)	// The file is corrupted
		{
			ISFS_Close(iFileDescriptor);
			ISFS_Delete(bIsKorean ? CsPathKorea.c_str() : CsPathWorld.c_str());
		}
		else	// Toggle the sync value
		{
			if ((iError = ISFS_Seek(iFileDescriptor, uiPosition + 18, 0)) < 0)
				throw std::ios_base::failure(std::string{"Error seeking ec.cfg, ret = "} + 
					std::to_string(iError));

			const char CcToggle ATTRIBUTE_ALIGN(32) {'1'};

			if ((iError = ISFS_Write(iFileDescriptor, &CcToggle, 1)) < 0)
				throw std::ios_base::failure(std::string{"Error writing to ec.cfg, ret = "} + 
				std::to_string(iError));
		}

		ISFS_Close(iFileDescriptor);
		std::puts("\nEverything's good!");
	}
	catch (const std::ios_base::failure& CiosBaseFailure)
	{
		switch (iFileDescriptor)
		{
		case -1: 
		case -102: std::puts("Permission denied. Are you running me from the Homebrew Channel?"); break;
		case -6: 
		case -106: std::puts("File not found. If you have the Wii Shop Channel installed, you should be good."); break;
		default: break;
		}
		std::printf("\n%s\nPress HOME/START to exit and try again.\n", CiosBaseFailure.what()); 
	}

	std::puts("\nHOME/START: exit the application\n+/Y: go to the Wii Shop Channel.");

	u32 uiPressedWPAD{}, uiPressedPAD{};
	expansion_t expansionType{};

	while(TRUE)
	{
		// Call WPAD_ScanPads each loop, this reads the latest controller states
		WPAD_ScanPads();
		PAD_ScanPads();

		uiPressedWPAD = WPAD_ButtonsDown(WPAD_CHAN_0);
		uiPressedPAD = PAD_ButtonsDown(PAD_CHAN0);
		WPAD_Expansion(WPAD_CHAN_0, &expansionType);

		if ((uiPressedWPAD & WPAD_BUTTON_HOME) || (uiPressedPAD & PAD_BUTTON_START) ||
			(expansionType.type == EXP_CLASSIC && expansionType.classic.btns & CLASSIC_CTRL_BUTTON_HOME))
		{
			PrepareExit();
			std::exit(EXIT_SUCCESS);
		}
		if ((uiPressedWPAD & WPAD_BUTTON_PLUS) || (uiPressedPAD & PAD_BUTTON_Y) ||
			(expansionType.type == EXP_CLASSIC && expansionType.classic.btns & CLASSIC_CTRL_BUTTON_PLUS))
		{
			PrepareExit();
			WII_LaunchTitle(bIsKorean ? 0x100024841424B : 0x1000248414241);
		}

		// Wait for the next frame
		VIDEO_WaitVSync();
	}
}


void Initialise() noexcept
{
	if (!patch_ahbprot_reset()) std::exit(EXIT_FAILURE);
	if (!patch_isfs_permissions()) std::exit(EXIT_FAILURE);

	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	WPAD_Init();
	PAD_Init();

	// Initialise filesystem
	ISFS_Initialize();

	// Initialise NAND title launching
	WII_Initialize();

	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	SpGXRmode = VIDEO_GetPreferredMode(nullptr);

	// Allocate memory for the display in the uncached region
	SpXfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(SpGXRmode));

	console_init(SpXfb, 20, 20, SpGXRmode->fbWidth, SpGXRmode->xfbHeight, 
		SpGXRmode->fbWidth * VI_DISPLAY_PIX_SZ);

	// Set up the video registers with the chosen mode
	VIDEO_Configure(SpGXRmode);

	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(SpXfb);

	// Make the display visible
	VIDEO_SetBlack(FALSE);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(SpGXRmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
}


void PrepareExit() noexcept
{
	std::puts("Exiting...");
	ISFS_Deinitialize();
}
