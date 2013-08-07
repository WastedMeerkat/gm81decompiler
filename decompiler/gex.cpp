/*
 *	gex.cpp
 *	GameMaker Extension
 *	Credit for Phillip Stephens for providing information about
 *	the encryption on GEXs as well as providing source as an example.
 *	http://code.google.com/p/librarybuilder/source/browse/trunk/GameMaker/ExtensionProject/ProjectReader.cs
 */

#include <iostream>
#include "gex.hpp"

bool GmGex::Load(const std::string& filename) {
	if (!handle->Load(filename))
		return false;

	// Read header
	if (handle->ReadDword() != GMK_MAGIC) {
		std::cerr << "[Error  ] Not a valid GEX!" << std::endl;
		return false;
	}

	version = handle->ReadDword();
	if (version != 701)
		std::cout << "[Warning] GEX Version is not 701!" << std::endl;

	GmkStream* newHandle = new GmkStream();
	obfuscation->SetSeed(handle->ReadDword());

	while(newHandle->iLength < handle->iLength - 12) {
		newHandle->WriteByte(obfuscation->GetByte(handle->ReadByte()));
	}

	newHandle->Save("asdf.bin", FMODE_BINARY);

	return true;
	

	header.editable			= handle->ReadBool();
	header.name				= handle->ReadString();
	header.tempDirectory	= handle->ReadString();
	header.version			= handle->ReadString();
	header.author			= handle->ReadString();
	header.dateModified		= handle->ReadString();
	header.licence			= handle->ReadString();
	header.information		= handle->ReadString();
	header.helpFile			= handle->ReadString();
	header.hidden			= handle->ReadBool();

	int count = handle->ReadDword();
	while(count--)
		header.uses.push_back(handle->ReadString());

	// Load include files
	count = handle->ReadDword();
	while(count--) {
		GexIncludeFile* includeFile = new GexIncludeFile();

		// Read header
		handle->SkipDwords(1);
		includeFile->name			= handle->ReadString();
		includeFile->path			= handle->ReadString();
		includeFile->type			= handle->ReadDword();
		includeFile->initCode		= handle->ReadString();
		includeFile->finalCode		= handle->ReadString();

		// Read functions
		int countFunctions = handle->ReadDword();
		while(countFunctions--) {
			GexFunction* function = new GexFunction();

			handle->SkipDwords(1);
			function->name			= handle->ReadString();
			function->externalName	= handle->ReadString();
		}

		includeFiles.push_back(includeFile);
	}

	return true;
}
