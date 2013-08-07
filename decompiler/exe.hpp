/*
 *	exe.hpp
 *	GM EXE Abstraction
 */

#ifndef __EXE_HPP
#define __EXE_HPP

#include <iostream>
#include <string>
#include "gmk.hpp"

#define SWAP_TABLE_SIZE			256

// Icon headers
#pragma pack(push, 1)
typedef struct {
    unsigned char        bWidth;          // Width, in pixels, of the image
    unsigned char        bHeight;         // Height, in pixels, of the image
    unsigned char        bColorCount;     // Number of colors in image (0 if >=8bpp)
    unsigned char        bReserved;       // Reserved ( must be 0)
    unsigned short        wPlanes;         // Color Planes
    unsigned short        wBitCount;       // Bits per pixel
    unsigned int       dwBytesInRes;    // How many bytes in this resource?
    unsigned int       dwImageOffset;   // Where in the file is this image?
} ICONDIRENTRY, *LPICONDIRENTRY;

typedef struct {
    unsigned short           idReserved;   // Reserved (must be 0)
    unsigned short           idType;       // Resource Type (1 for icons)
    unsigned short           idCount;      // How many images?
    ICONDIRENTRY   idEntries[1]; // An entry for each image (idCount of 'em)
} ICONDIR, *LPICONDIR;
#pragma pack(pop)

class GmExe {
private:
	std::string exeFilename;
	unsigned int version;
	GmkStream* exeHandle;
	Gmk* gmkHandle;

	// GMK support -- Technically shouldn't be here
	void RTAddItem(const std::string& name, int rtId, int index);

	bool DecryptGameData();
	GmkStream* GetIconData();

	void ReadAction(GmkStream* stream, ObjectAction* action);

	bool ReadSettings();
	bool ReadWrapper();

	bool ReadExtensions();
	bool ReadTriggers();
	bool ReadConstants();
	
	bool ReadSounds();
	bool ReadSprites();
	bool ReadBackgrounds();
	bool ReadPaths();
	bool ReadScripts();
	bool ReadFonts();
	bool ReadTimelines();
	bool ReadObjects();
	bool ReadRooms();

	bool ReadIncludes();
	bool ReadGameInformation();

public:
	GmExe() : exeHandle(new GmkStream()), version(0), exeFilename("") { }
	~GmExe() { delete exeHandle; }

	// Interface functions
	bool Load(const std::string& filename, Gmk* gmk, unsigned int ver);
	const unsigned int GetExeVersion() const { return version; }
};

#endif
