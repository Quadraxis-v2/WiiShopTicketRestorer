#include <cstdio>
#include <ios>
#include <string>
#include <cstdlib>

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <gctypes.h>
#include <gcutil.h>
#include <ogc/isfs.h>
#include <ogc/wiilaunch.h>

static void* SpXfb{nullptr};	
static GXRModeObj* SpGXRmode{nullptr};


void Initialise() noexcept;


//---------------------------------------------------------------------------------
int main(int argc, char **argv) 
{
//---------------------------------------------------------------------------------
	Initialise();

	s32 iFileDescriptor{-1};
	s32 iError{ISFS_OK};

	WPADData* pWPADData0{};
	u32 uiExpansionType{WPAD_EXP_NONE};

	std::printf("\x1b[2;0H");
	std::puts("Wii Shop Channel Ticket Restorer");
	std::puts("--------------------------------");

	try
	{
		std::puts("Opening EC file...");

		if ((iFileDescriptor = ISFS_Open("/title/00010002/48414241/data/ec.cfg", ISFS_OPEN_RW)) < 0)
			throw std::ios_base::failure(std::string{"Error opening ec.cfg, ret = "} + 
				std::to_string(iFileDescriptor));

		fstats fStatsEC ATTRIBUTE_ALIGN(32) {};

		if ((iError = ISFS_GetFileStats(iFileDescriptor, &fStatsEC)) < 0)
			throw std::ios_base::failure(std::string{"Error getting file stats from ec.cfg, ret = "} + 
				std::to_string(iError));

		char uyOffset[fStatsEC.file_length] ATTRIBUTE_ALIGN(32);

		std::puts("Reading EC file...");

		if ((iError = ISFS_Read(iFileDescriptor, uyOffset, fStatsEC.file_length)) < 0)
			throw std::ios_base::failure(std::string{"Error reading ec.cfg, ret = "} + 
				std::to_string(iError));

		std::string sFile{uyOffset, fStatsEC.file_length};
		u32 uiPosition = sFile.find("isNeedTicketSyncImportAll", 0, 25);

		std::puts("Patching EC file...");

		if (uiPosition == std::string::npos)
		{
			ISFS_Close(iFileDescriptor);
			ISFS_Delete("/title/00010002/48414241/data/ec.cfg");
		}
		else
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
		std::puts("Everything's good! Press HOME to exit the application or press + to go to the Wii Shop Channel.");
	}
	catch (const std::ios_base::failure& CiosBaseFailure)
	{ std::printf("%s\nPress HOME to exit and try again.\n", CiosBaseFailure.what()); }

	while(TRUE)
	{
		// Call WPAD_ScanPads each loop, this reads the latest controller states
		WPAD_ScanPads();

		if (WPAD_Probe(WPAD_CHAN_0, &uiExpansionType) == WPAD_ERR_NONE)
		{
			pWPADData0 = WPAD_Data(WPAD_CHAN_0);

			if (pWPADData0->btns_d)
			{
				if (pWPADData0->btns_d & WPAD_BUTTON_HOME)
				{
					std::puts("Exiting...");
					ISFS_Deinitialize();
					std::exit(EXIT_SUCCESS);
				}
				if (pWPADData0->btns_d & WPAD_BUTTON_PLUS)
				{
					std::puts("Exiting...");
					ISFS_Deinitialize();
					WII_LaunchTitle(0x1000248414241);
				}
			}
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
