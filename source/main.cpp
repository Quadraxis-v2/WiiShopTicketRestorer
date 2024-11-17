/*
main.cpp --- Wii Shop Channel Ticket Restorer
Copyright (C) 2022  Juan de la Cruz Caravaca Guerrero (Quadraxis_v2)
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

	try
	{
		/* The EC file is a WSC file containing some key-value pairs related to the synchronization of tickets. */

		std::puts("Opening EC file...");

		s32 iFileDescriptor{-1};
		s32 iError{ISFS_OK};

		if ((iFileDescriptor = ISFS_Open("/title/00010002/48414241/data/ec.cfg", ISFS_OPEN_RW)) < 0)
			throw std::ios_base::failure(std::string{"Error opening ec.cfg, ret = "} + 
				std::to_string(iFileDescriptor));

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
		u32 uiPosition = sFile.find("isNeedTicketSyncImportAll", 0, 25);

		std::puts("Patching EC file...");

		if (uiPosition == std::string::npos)	// The file is corrupted
		{
			ISFS_Close(iFileDescriptor);
			ISFS_Delete("/title/00010002/48414241/data/ec.cfg");
		}
		else	// Toggle the sync value
		{
			if ((iError = ISFS_Seek(iFileDescriptor, uiPosition + 27, 0)) < 0)
				throw std::ios_base::failure(std::string{"Error seeking ec.cfg, ret = "} + 
					std::to_string(iError));

			const char CcToggle ATTRIBUTE_ALIGN(32) {'1'};

			if ((iError = ISFS_Write(iFileDescriptor, &CcToggle, 1)) < 0)
				throw std::ios_base::failure(std::string{"Error writing to ec.cfg, ret = "} + 
				std::to_string(iError));
		}

		ISFS_Close(iFileDescriptor);
		std::puts("\nEverything's good!\n\nHOME/START: exit the application\n+/Y: go to the Wii Shop Channel.");
	}
	catch (const std::ios_base::failure& CiosBaseFailure)
	{ std::printf("%s\nPress HOME to exit and try again.\n", CiosBaseFailure.what()); }

	u32 uiPressedWPAD{}, uiPressedPAD{};

	while(TRUE)
	{
		// Call WPAD_ScanPads each loop, this reads the latest controller states
		WPAD_ScanPads();
		PAD_ScanPads();

		uiPressedWPAD = WPAD_ButtonsDown(WPAD_CHAN_0);
		uiPressedPAD = PAD_ButtonsDown(PAD_CHAN0);

		if ((uiPressedWPAD & WPAD_BUTTON_HOME) || (uiPressedPAD & PAD_BUTTON_START))
		{
			PrepareExit();
			std::exit(EXIT_SUCCESS);
		}
		if ((uiPressedWPAD & WPAD_BUTTON_PLUS) || (uiPressedPAD & PAD_BUTTON_Y))
		{
			PrepareExit();
			WII_LaunchTitle(0x1000248414241);
		}

		// Wait for the next frame
		VIDEO_WaitVSync();
	}
}


void Initialise() noexcept
{
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
