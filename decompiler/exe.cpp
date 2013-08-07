/*
 *	exe.cpp
 *	GM EXE Abstraction
 */

#include <iostream>
#include <Windows.h>
#include "exe.hpp"
#include "gm81.hpp"
#include "gm80.hpp"

void GmExe::RTAddItem(const std::string& name, int rtId, int index) {
	const int rctGroupIds[] = { RCT_GROUP_SPRITES, RCT_GROUP_SOUNDS, RCT_GROUP_BACKGROUNDS, RCT_GROUP_PATHS, RCT_GROUP_SCRIPTS, RCT_GROUP_FONTS, RCT_GROUP_TIMELINES, RCT_GROUP_OBJECTS, RCT_GROUP_ROOMS };
	ResourceTree* rctItem = new ResourceTree();

	rctItem->name = name;
	rctItem->group = rctGroupIds[rtId];
	rctItem->index = index;
	rctItem->status = 3;
	
	gmkHandle->resourceTree[rtId]->contents.push_back(rctItem);
}

bool GmExe::Load(const std::string& filename, Gmk* gmk, unsigned int ver) {
	if (!gmk)
		return false;

	// Set handles
	version = ver;
	gmkHandle = gmk;
	exeFilename = filename;

	// Try to load the EXE
	if (!exeHandle->Load(filename))
		return false;

	// Now we need to check the ver so we know how to treat the EXE
	switch(version) {
		case 800: // GM8.0 EXE
		{
			std::cout << "Detected GM8.0 game!" << std::endl;

			Gm80* gm80 = new Gm80();

			if (!gm80->FindGameData(exeHandle))
				return false;

			delete gm80;
			break;
		}

		case 810: // GM8.1 EXE
		{
			std::cout << "Detected GM8.1 game!" << std::endl;

			Gm81* gm81 = new Gm81();

			if (!gm81->FindGameData(exeHandle))
				return false;

			gm81->Decrypt(exeHandle);

			delete gm81;
			break;
		}

		default: // Auto-detect version
			Gm81* gm81 = new Gm81();
			Gm80* gm80 = new Gm80();

			// Attempt to detect GM8.0 first
			if (gm80->FindGameData(exeHandle)) {
				version = 800;
				std::cout << "Detected GM8.0 game!" << std::endl;
				break;
			}

			// Attempt to detect GM8.1 next
			if (gm81->FindGameData(exeHandle)) {
				version = 810;
				std::cout << "Detected GM8.1 game!" << std::endl;

				if (!gm81->Decrypt(exeHandle)) {
					std::cout << "[Error  ] Failed to decrypt game data!" << std::endl;
					return false;
				}

				break;
			}

			std::cout << "Unknown game version!" << std::endl;

			delete gm80;
			delete gm81;
			return false;
	}

	// Read header
	std::cout << "Reading header..." << std::endl;
	if (version == 800)
		exeHandle->SkipDwords(2);	// Version, Debug flag
	else if (version == 810)
		exeHandle->SkipDwords(3);	// Null Magic, Null Version, Debug flag
	else
		return false;

	// Read settings
	std::cout << "Reading settings..." << std::endl;
	if (!ReadSettings())
		return false;

	// Read wrapper
	std::cout << "Reading d3d wrapper..." << std::endl;
	if (!ReadWrapper())
		return false;

	// Decrypt main game data paragraph
	std::cout << "Decrypting game data..." << std::endl;
	if (!DecryptGameData())
		return false;

	// Skip yet another garbage field -- and the pro flag, silly pro flag
	exeHandle->SkipDwords(exeHandle->ReadDword() + 1);

	gmkHandle->gameId = exeHandle->ReadDword();

	for(int i = 0; i < 4; i++)
		exeHandle->ReadDword();

	// Read extensions
	std::cout << "Reading extensions..." << std::endl;
	if (!ReadExtensions())
		return false;

	// Read triggers
	std::cout << "Reading triggers..." << std::endl;
	if (!ReadTriggers())
		return false;

	// Read constants
	std::cout << "Reading constants..." << std::endl;
	if (!ReadConstants())
		return false;

	// Read sounds
	std::cout << "Reading sounds..." << std::endl;
	if (!ReadSounds())
		return false;

	// Read sprites
	std::cout << "Reading sprites..." << std::endl;
	if (!ReadSprites())
		return false;

	// Read backgrounds
	std::cout << "Reading backgrounds..." << std::endl;
	if (!ReadBackgrounds())
		return false;

	// Read paths
	std::cout << "Reading paths..." << std::endl;
	if (!ReadPaths())
		return false;

	// Read scripts
	std::cout << "Reading scripts..." << std::endl;
	if (!ReadScripts())
		return false;

	// Read fonts
	std::cout << "Reading fonts..." << std::endl;
	if (!ReadFonts())
		return false;

	// Read timelines
	std::cout << "Reading timelines..." << std::endl;
	if (!ReadTimelines())
		return false;

	// Read objects
	std::cout << "Reading objects..." << std::endl;
	if (!ReadObjects())
		return false;

	// Read rooms
	std::cout << "Reading rooms..." << std::endl;
	if (!ReadRooms())
		return false;

	// Read last ID of object/tile placed
	gmkHandle->lastInstance = exeHandle->ReadDword();
	gmkHandle->lastTile = exeHandle->ReadDword();

	// Read includes
	std::cout << "Reading include files..." << std::endl;
	if (!ReadIncludes())
		return false;

	// Read game information
	std::cout << "Reading game information..." << std::endl;
	if (!ReadGameInformation())
		return false;

	// Ignore library initialization code
	std::cout << "Reading library initialization code..." << std::endl;
	exeHandle->SkipDwords(1);

	unsigned int i = exeHandle->ReadDword();
	for(; i; i--)
		exeHandle->iPosition += exeHandle->ReadDword();

	// Read room order
	std::cout << "Reading room order..." << std::endl;
	exeHandle->SkipDwords(1);

	i = exeHandle->ReadDword();
	for(; i; i--)
		gmkHandle->roomExecutionOrder.push_back(exeHandle->ReadDword());

	// Correct room order
	ResourceTree* tmpTree = new ResourceTree();
	tmpTree->group = RCT_GROUP_ROOMS;
	tmpTree->name = "Rooms";
	tmpTree->index = 0;
	tmpTree->status = 1;

	for(size_t a = 0; a < gmkHandle->roomExecutionOrder.size(); a++) {
		ResourceTree* rcItem = new ResourceTree();

		rcItem->name = gmkHandle->rooms[gmkHandle->roomExecutionOrder[a]]->name;
		rcItem->group = RCT_GROUP_ROOMS;
		rcItem->index = gmkHandle->roomExecutionOrder[a];
		rcItem->status = 3;

		tmpTree->contents.push_back(rcItem);
	}

	delete gmkHandle->resourceTree[RCT_ID_ROOMS];
	gmkHandle->resourceTree[RCT_ID_ROOMS] = tmpTree;

	return true;
}

GmkStream* GmExe::GetIconData() {
	/* Note: This function was made possibly by Mike, major props to him. */
	GmkStream* iconStream = new GmkStream();
	
	HMODULE hModule = LoadLibrary(exeFilename.c_str());
	if (!hModule) {
		std::cout << "[Warning] Failed to find icon data!" << std::endl;
		return NULL;
	}

	// Get image data
	HRSRC hRc = ::FindResource(hModule,MAKEINTRESOURCE(2),RT_ICON);

	HGLOBAL hGlobal = ::LoadResource(hModule, hRc);
	DWORD size = SizeofResource(hModule,hRc);

	unsigned char* p = (unsigned char*)hGlobal;

	// Prepare header
	ICONDIR* headerData		= new ICONDIR;
	headerData->idCount		= 1;
	headerData->idReserved	= 0;
	headerData->idType		= 1;

	// Entry
	headerData->idEntries[0].wPlanes		= 1;
	headerData->idEntries[0].bColorCount	= 0;
	headerData->idEntries[0].bHeight		= 32;
	headerData->idEntries[0].bWidth			= 32;
	headerData->idEntries[0].bReserved		= 0;
	headerData->idEntries[0].wBitCount		= 32;
	headerData->idEntries[0].dwBytesInRes	= size;
	headerData->idEntries[0].dwImageOffset	= sizeof(ICONDIR);

	// Write it to the stream
	iconStream->WriteData((unsigned char*)headerData,sizeof(ICONDIR));
	iconStream->WriteData((unsigned char*)p,size);

	FreeLibrary(hModule);

	delete headerData;

	return iconStream;
}

bool GmExe::ReadSettings() {
	exeHandle->SkipDwords(1);

	GmkStream* settings = new GmkStream;
	exeHandle->Deserialize(settings,true);

	// Read icon
	GmkStream* iconData = GetIconData();
	if (iconData)
		gmkHandle->settings.iconData = iconData;
	
	// Read settings
	gmkHandle->settings.fullscreen				= settings->ReadBool();
	gmkHandle->settings.interpolate				= settings->ReadBool();
	gmkHandle->settings.dontDrawBorder			= settings->ReadBool();
	gmkHandle->settings.displayCursor			= settings->ReadBool();
	gmkHandle->settings.scaling					= settings->ReadDword();
	gmkHandle->settings.allowWindowResize		= settings->ReadBool();
	gmkHandle->settings.onTop					= settings->ReadBool();
	gmkHandle->settings.colorOutsideRoom		= settings->ReadDword();
	gmkHandle->settings.setResolution			= settings->ReadBool();
	gmkHandle->settings.colorDepth				= settings->ReadDword();
	gmkHandle->settings.resolution				= settings->ReadDword();
	gmkHandle->settings.frequency				= settings->ReadDword();
	gmkHandle->settings.dontShowButtons			= settings->ReadBool();
	gmkHandle->settings.vsync					= settings->ReadBool();
	gmkHandle->settings.disableScreen			= settings->ReadBool();
	gmkHandle->settings.letF4					= settings->ReadBool();
	gmkHandle->settings.letF1					= settings->ReadBool();
	gmkHandle->settings.letEsc					= settings->ReadBool();
	gmkHandle->settings.letF5					= settings->ReadBool();
	gmkHandle->settings.letF9					= settings->ReadBool();
	gmkHandle->settings.treatCloseAsEsc			= settings->ReadBool();
	gmkHandle->settings.priority				= settings->ReadDword();
	gmkHandle->settings.freeze					= settings->ReadBool();

	gmkHandle->settings.loadingBar = settings->ReadDword();
	if (gmkHandle->settings.loadingBar) {
		if (settings->ReadBool()) {
			gmkHandle->settings.backData = new GmkStream;
			settings->Deserialize(gmkHandle->settings.backData,true);
		}

		if (settings->ReadBool()) {
			gmkHandle->settings.frontData = new GmkStream;
			settings->Deserialize(gmkHandle->settings.frontData,true);
		}
	}

	gmkHandle->settings.customLoadImage = settings->ReadBool();
	if (gmkHandle->settings.customLoadImage) {
		gmkHandle->settings.loadBar = new GmkStream;
		settings->Deserialize(gmkHandle->settings.loadBar,true);
	}

	gmkHandle->settings.transparent				= settings->ReadBool();
	gmkHandle->settings.translucency			= settings->ReadDword();
	gmkHandle->settings.scaleProgressBar		= settings->ReadBool();

	gmkHandle->settings.errorDisplay			= settings->ReadBool();
	gmkHandle->settings.errorLog				= settings->ReadBool();
	gmkHandle->settings.errorAbort				= settings->ReadBool();
	gmkHandle->settings.treatAsZero				= settings->ReadDword();

	if (version == 810) {
		gmkHandle->settings.errorOnUninitialization = (gmkHandle->settings.treatAsZero & 0x02) ? 1 : 0;
		gmkHandle->settings.treatAsZero &= 0x01;
	}

	delete settings;

	return true;
}

bool GmExe::ReadWrapper() {
	exeHandle->iPosition += exeHandle->ReadDword();
	exeHandle->iPosition += exeHandle->ReadDword();

	return true;
}

bool GmExe::DecryptGameData() {
	unsigned char forwardTable[SWAP_TABLE_SIZE], reverseTable[SWAP_TABLE_SIZE];
	signed int i, b;
	unsigned char a;

	GmkStream* gameData = new GmkStream;

	// Find the swap table
	unsigned int d1 = exeHandle->ReadDword();
	unsigned int d2 = exeHandle->ReadDword();

	// Read swap table
	exeHandle->SkipDwords(d1);
	exeHandle->ReadData(forwardTable, SWAP_TABLE_SIZE);
	exeHandle->SkipDwords(d2);

	// Populate reverse table
	for(int z = 0; z < SWAP_TABLE_SIZE; z++)
		reverseTable[forwardTable[z]] = z;

	// Deserialize game data
	exeHandle->Deserialize(gameData, false);

	// Decrypt - Pass One
	for(i = gameData->iLength - 1; i; i--)
		gameData->iBuffer[i] = reverseTable[gameData->iBuffer[i]] - gameData->iBuffer[i - 1] - i;

	// Decrypt - Pass Two
	for(i = gameData->iLength - 1; i >= 0; i--) {
		a = forwardTable[i & 0xFF];
		b = i - (int)a;
		if (b < 0)
			b = 0;

		a = gameData->iBuffer[i];
		gameData->iBuffer[i] = gameData->iBuffer[b];
		gameData->iBuffer[b] = a;
	}

	// Replace current stream
	delete[] exeHandle->iBuffer;
	*exeHandle = *gameData;

	return true;
}

// Main game data
bool GmExe::ReadExtensions() {
	exeHandle->SkipDwords(1);

	unsigned int count = exeHandle->ReadDword();
	for(; count; count--) {
		// Pretty much skip everything except for the package name -- for now
		exeHandle->SkipDwords(1);

		// Package information
		gmkHandle->packages.push_back(exeHandle->ReadString());
		exeHandle->iPosition += exeHandle->ReadDword();

		unsigned int i = exeHandle->ReadDword();
		for(; i; i--) {
			exeHandle->SkipDwords(1);

			exeHandle->iPosition += exeHandle->ReadDword();
			exeHandle->SkipDwords(1);
			exeHandle->iPosition += exeHandle->ReadDword();
			exeHandle->iPosition += exeHandle->ReadDword();

			// Compiled data
			unsigned int ii = exeHandle->ReadDword();
			for(; ii; ii--) {
				exeHandle->SkipDwords(1);
				exeHandle->iPosition += exeHandle->ReadDword();
				exeHandle->iPosition += exeHandle->ReadDword();
				exeHandle->SkipDwords(3);

				for(unsigned int iii = 17; iii; iii--)
					exeHandle->SkipDwords(1);

				exeHandle->SkipDwords(1);
			}

			unsigned int iii = exeHandle->ReadDword();
			for(; iii; iii--) {
				exeHandle->SkipDwords(1);
				exeHandle->iPosition += exeHandle->ReadDword();
				exeHandle->iPosition += exeHandle->ReadDword();
			}
		}

		/* Compressed resource files -- also encrypted!
			   Oh wait, I don't care, just skip over this */
			exeHandle->iPosition += exeHandle->ReadDword();
	}

	return true;
}

bool GmExe::ReadTriggers() {
	GmkStream* stream;
	Trigger* trigger;

	exeHandle->SkipDwords(1);

	unsigned int count = exeHandle->ReadDword();
	for(; count; count--) {
		stream = new GmkStream();
		exeHandle->Deserialize(stream, true);

		if (!stream->ReadBool()) {
			gmkHandle->triggers.push_back(0);
			delete stream;
			continue;
		}

		trigger = new Trigger();

		stream->SkipDwords(1);

		trigger->name = stream->ReadString();
		trigger->condition = stream->ReadString();
		trigger->checkMoment = stream->ReadDword();
		trigger->constantName = stream->ReadString();

		gmkHandle->triggers.push_back(trigger);

		delete stream;
	}

	return true;
}

bool GmExe::ReadConstants() {
	Constant* constant;

	exeHandle->SkipDwords(1);

	unsigned int count = exeHandle->ReadDword();
	for(; count; count--) {
		constant = new Constant();

		constant->name = exeHandle->ReadString();
		constant->value = exeHandle->ReadString();

		gmkHandle->constants.push_back(constant);
	}

	return true;
}

bool GmExe::ReadSounds() {
	GmkStream* stream;
	Sound* sound;

	exeHandle->SkipDwords(1);

	unsigned int count = exeHandle->ReadDword();
	for(size_t a = 0; a < count; a++) {
		stream = new GmkStream();
		exeHandle->Deserialize(stream, true);

		if (!stream->ReadBool()) {
			gmkHandle->sounds.push_back(0);
			delete stream;
			continue;
		}

		sound = new Sound();

		sound->name = stream->ReadString();
		stream->SkipDwords(1);

		sound->kind = stream->ReadDword();
		sound->fileType = stream->ReadString();
		sound->fileName = stream->ReadString();

		if (stream->ReadBool()) {
			sound->data = new GmkStream();
			stream->Deserialize(sound->data, false);
		} else
			sound->data = NULL;

		sound->effects = stream->ReadDword();
		sound->volume = stream->ReadDouble();
		sound->pan = stream->ReadDouble();
		sound->preload = stream->ReadBool();

		RTAddItem(sound->name,RCT_ID_SOUNDS,a);
		gmkHandle->sounds.push_back(sound);
		delete stream;
	}

	return true;
}

bool GmExe::ReadSprites() {
	GmkStream* stream;
	Sprite* sprite;

	exeHandle->SkipDwords(1);

	unsigned int count = exeHandle->ReadDword();
	for(size_t a = 0; a < count; a++) {
		stream = new GmkStream();
		exeHandle->Deserialize(stream, true);

		if (!stream->ReadBool()) {
			gmkHandle->sprites.push_back(0);
			delete stream;
			continue;
		}

		sprite = new Sprite();

		// Sprite info
		sprite->name = stream->ReadString();
		stream->SkipDwords(1);

		sprite->originX = stream->ReadDword();
		sprite->originY = stream->ReadDword();

		sprite->boundingBox = 2;
		sprite->alphaTolerance = 0;
		sprite->shape = 0;

		unsigned int i = stream->ReadDword();
		if (i) {
			// Frames
			for(; i; i--) {
				SubImage* image = new SubImage();

				stream->SkipDwords(1);
				image->width = stream->ReadDword();
				image->height = stream->ReadDword();

				image->data = new GmkStream();
				stream->Deserialize(image->data, false);

				sprite->images.push_back(image);
			}

			// Collision bitmap
			sprite->seperateMask = stream->ReadBool();
			if (sprite->seperateMask) {
				for(size_t ii = 0; ii < sprite->images.size(); ii++) {
					stream->SkipDwords(1);

					unsigned int maskSize = stream->ReadDword() * stream->ReadDword();
					sprite->left = stream->ReadDword();
					sprite->right = stream->ReadDword();
					sprite->bottom = stream->ReadDword();
					sprite->top = stream->ReadDword();

					for(; maskSize; maskSize--)
						if (stream->ReadDword() > 0)
							sprite->boundingBox = 1;
				}
			} else {
				stream->SkipDwords(1);

				unsigned int maskSize = stream->ReadDword() * stream->ReadDword();
				sprite->left = stream->ReadDword();
				sprite->right = stream->ReadDword();
				sprite->bottom = stream->ReadDword();
				sprite->top = stream->ReadDword();

				for(; maskSize; maskSize--)
					if (stream->ReadDword() > 0)
						sprite->boundingBox = 1;
			}
		}

		RTAddItem(sprite->name,RCT_ID_SPRITES,a);
		gmkHandle->sprites.push_back(sprite);
		delete stream;
	}

	return true;
}

bool GmExe::ReadBackgrounds() {
	GmkStream* stream;
	Background* background;

	exeHandle->SkipDwords(1);

	unsigned int count = exeHandle->ReadDword();
	for(size_t a = 0; a < count; a++) {
		stream = new GmkStream();
		exeHandle->Deserialize(stream, true);

		if (!stream->ReadBool()) {
			gmkHandle->backgrounds.push_back(0);
			delete stream;
			continue;
		}

		background = new Background();

		background->name = stream->ReadString();
		stream->SkipDwords(2);

		background->width = stream->ReadDword();
		background->height = stream->ReadDword();

		background->hOffset = background->vOffset = background->hSep = background->vSep = 0;
		background->tileWidth = background->tileHeight = 16;
		background->tileset = false;

		if (background->width > 0 && background->height > 0) {
			background->data = new GmkStream();
			stream->Deserialize(background->data,false);
		}

		RTAddItem(background->name,RCT_ID_BACKGROUNDS,a);
		gmkHandle->backgrounds.push_back(background);
		delete stream;
	}

	return true;
}

bool GmExe::ReadPaths() {
	GmkStream* stream;
	Path* path;

	exeHandle->SkipDwords(1);

	unsigned int count = exeHandle->ReadDword();
	for(size_t a = 0; a < count; a++) {
		stream = new GmkStream();
		exeHandle->Deserialize(stream,true);

		if (!stream->ReadBool()) {
			gmkHandle->paths.push_back(0);
			delete stream;
			continue;
		}

		path = new Path();

		path->name = stream->ReadString();
		stream->SkipDwords(1);

		path->kind = stream->ReadDword();
		path->closed = stream->ReadBool();
		path->prec = stream->ReadDword();

		path->snapX = path->snapY = 16;
		path->roomIndexBG = -1;

		unsigned int i = stream->ReadDword();
		for(; i; i--) {
			PathPoint* point = new PathPoint();
			point->x = stream->ReadDouble();
			point->y = stream->ReadDouble();
			point->speed = stream->ReadDouble();

			path->points.push_back(point);
		}

		RTAddItem(path->name,RCT_ID_PATHS,a);
		gmkHandle->paths.push_back(path);
		delete stream;
	}

	return true;
}

bool GmExe::ReadScripts() {
	GmkStream* stream;
	Script* script;

	exeHandle->SkipDwords(1);

	unsigned int count = exeHandle->ReadDword();
	for(size_t a = 0; a < count; a++) {
		stream = new GmkStream();
		exeHandle->Deserialize(stream,true);

		if (!stream->ReadBool()) {
			gmkHandle->scripts.push_back(0);
			delete stream;
			continue;
		}

		script = new Script();

		script->name = stream->ReadString();
		stream->SkipDwords(1);
		script->value = stream->ReadString();

		RTAddItem(script->name,RCT_ID_SCRIPTS,a);
		gmkHandle->scripts.push_back(script);
		delete stream;
	}

	return true;
}

bool GmExe::ReadFonts() {
	GmkStream* stream;
	Font* font;

	exeHandle->SkipDwords(1);

	unsigned int count = exeHandle->ReadDword();
	for(size_t a = 0; a < count; a++) {
		stream = new GmkStream();
		exeHandle->Deserialize(stream,true);

		if (!stream->ReadBool()) {
			gmkHandle->fonts.push_back(0);
			delete stream;
			continue;
		}

		font = new Font();

		font->name = stream->ReadString();
		stream->SkipDwords(1);

		font->fontName = stream->ReadString();
		font->size = stream->ReadDword();
		font->bold = stream->ReadBool();
		font->italic = stream->ReadBool();
		font->rangeBegin = stream->ReadDword();
		font->rangeEnd = stream->ReadDword();

		if (version == 810) {
			font->charset = font->rangeBegin & 0xFF000000;
			font->aaLevel = font->rangeBegin & 0x00FF0000;
			font->rangeBegin &= 0x0000FFFF;
		}

		RTAddItem(font->name,RCT_ID_FONTS,a);
		gmkHandle->fonts.push_back(font);
		delete stream;
	}

	return true;
}

void GmExe::ReadAction(GmkStream* stream, ObjectAction* action) {
	stream->SkipDwords(1);

	action->libId = stream->ReadDword();
	action->actId = stream->ReadDword();
	action->kind = stream->ReadDword();
	action->mayBeRelative = stream->ReadBool();
	action->question = stream->ReadBool();
	action->appliesToSomething = stream->ReadBool();
	action->type = stream->ReadDword();

	action->functionName = stream->ReadString();
	action->functionCode = stream->ReadString();

	action->argumentsUsed = stream->ReadDword();

	// Argument types
	stream->SkipDwords(1);
	for(size_t i = 0; i < 8; i++)
		action->argumentKind[i] = stream->ReadDword();

	action->appliesToObject = stream->ReadDword();
	action->isRelative = stream->ReadBool();

	// Arguments
	stream->SkipDwords(1);
	for(size_t i = 0; i < 8; i++)
		action->argumentValue[i] = stream->ReadString();

	action->not = stream->ReadBool();
}

bool GmExe::ReadTimelines() {
	GmkStream* stream;
	Timeline* timeline;

	exeHandle->SkipDwords(1);

	unsigned int count = exeHandle->ReadDword();
	for(size_t a = 0; a < count; a++) {
		stream = new GmkStream();
		exeHandle->Deserialize(stream,true);

		if (!stream->ReadBool()) {
			gmkHandle->timelines.push_back(0);
			delete stream;
			continue;
		}

		timeline = new Timeline();

		timeline->name = stream->ReadString();
		stream->SkipDwords(1);

		unsigned int i = stream->ReadDword();
		for(; i; i--) {
			TimelineMoment* moment = new TimelineMoment();

			moment->moment = stream->ReadDword();
			stream->SkipDwords(1);

			unsigned int ii = stream->ReadDword();
			for(; ii; ii--) {
				ObjectAction* action = new ObjectAction();

				ReadAction(stream,action);
				moment->actions.push_back(action);
			}

			timeline->moments.push_back(moment);
		}

		RTAddItem(timeline->name,RCT_ID_TIMELINES,a);
		gmkHandle->timelines.push_back(timeline);
		delete stream;
	}

	return true;
}

bool GmExe::ReadObjects() {
	GmkStream* stream;
	Object* object;

	exeHandle->SkipDwords(1);

	unsigned int count = exeHandle->ReadDword();
	for(size_t a = 0; a < count; a++) {
		stream = new GmkStream();
		exeHandle->Deserialize(stream,true);

		if (!stream->ReadBool()) {
			gmkHandle->objects.push_back(0);
			delete stream;
			continue;
		}

		object = new Object();

		object->name = stream->ReadString();
		stream->SkipDwords(1);

		object->spriteIndex = stream->ReadDword();
		object->solid = stream->ReadBool();
		object->visible = stream->ReadBool();
		object->depth = stream->ReadDword();
		object->persistent = stream->ReadBool();
		object->parentIndex = stream->ReadDword();
		object->maskIndex = stream->ReadDword();

		unsigned int i = stream->ReadDword() + 1;
		for(unsigned int ii = 0; ii < i; ii++) {
			for(;;) {
				int first = stream->ReadDword();
				if (first == -1)
					break;

				ObjectEvent* objEvent = new ObjectEvent();
				objEvent->eventType = ii;
				objEvent->eventKind = first;

				stream->SkipDwords(1);

				unsigned int iii = stream->ReadDword();
				for(; iii; iii--) {
					ObjectAction* action = new ObjectAction();
					ReadAction(stream,action);

					objEvent->actions.push_back(action);
				}

				object->events.push_back(objEvent);
			}
		}
	
		RTAddItem(object->name,RCT_ID_OBJECTS,a);
		gmkHandle->objects.push_back(object);
		delete stream;
	}

	return true;
}

bool GmExe::ReadRooms() {
	GmkStream* stream;
	Room* room;

	exeHandle->SkipDwords(1);

	unsigned int count = exeHandle->ReadDword();
	for(size_t a = 0; a < count; a++) {
		stream = new GmkStream();
		exeHandle->Deserialize(stream,true);

		if (!stream->ReadBool()) {
			gmkHandle->rooms.push_back(0);
			delete stream;
			continue;
		}

		room = new Room();

		room->name = stream->ReadString();
		stream->SkipDwords(1);

		room->caption = stream->ReadString();
		room->width = stream->ReadDword();
		room->height = stream->ReadDword();
		room->speed = stream->ReadDword();
		room->persistent = stream->ReadBool();
		room->bgColor = stream->ReadDword();
		room->bgColorDraw = stream->ReadBool();
		room->creationCode = stream->ReadString();

		if (version == 800)
			room->snapX = room->snapY = 16;
		else if (version == 810)
			room->snapX = room->snapY = 32;

		unsigned int i = stream->ReadDword();
		for(; i; i--) {
			RoomBackground* background = new RoomBackground();

			background->visible = stream->ReadBool();
			background->foreground = stream->ReadBool();
			background->bgIndex = stream->ReadDword();
			background->x = stream->ReadDword();
			background->y = stream->ReadDword();
			background->tileH = stream->ReadBool();
			background->tileV = stream->ReadBool();
			background->hSpeed = stream->ReadDword();
			background->vSpeed = stream->ReadDword();
			background->stretch = stream->ReadBool();

			room->backgrounds.push_back(background);
		}

		room->enableViews = stream->ReadBool();
		i = stream->ReadDword();
		for(; i; i--) {
			RoomView* view =  new RoomView();

			view->visible = stream->ReadBool();
			view->viewX = stream->ReadDword();
			view->viewY = stream->ReadDword();
			view->viewW = stream->ReadDword();
			view->viewH = stream->ReadDword();
			view->portX = stream->ReadDword();
			view->portY = stream->ReadDword();
			view->portW = stream->ReadDword();
			view->portH = stream->ReadDword();
			view->hBorder = stream->ReadDword();
			view->vBorder = stream->ReadDword();
			view->hSpd = stream->ReadDword();
			view->vSpd = stream->ReadDword();
			view->objFollow = stream->ReadDword();

			room->views.push_back(view);
		}

		i = stream->ReadDword();
		for(; i; i--) {
			RoomInstance* instance = new RoomInstance();

			instance->x = stream->ReadDword();
			instance->y = stream->ReadDword();
			instance->objIndex = stream->ReadDword();
			instance->id = stream->ReadDword();
			instance->creationCode = stream->ReadString();

			room->instances.push_back(instance);
		}

		i = stream->ReadDword();
		for(; i; i--) {
			RoomTile* tile = new RoomTile();

			tile->x = stream->ReadDword();
			tile->y = stream->ReadDword();
			tile->bgIndex = stream->ReadDword();
			tile->tileX = stream->ReadDword();
			tile->tileY = stream->ReadDword();
			tile->width = stream->ReadDword();
			tile->height = stream->ReadDword();
			tile->depth = stream->ReadDword();
			tile->id = stream->ReadDword();

			room->tiles.push_back(tile);
		}

		RTAddItem(room->name,RCT_ID_ROOMS,a);
		gmkHandle->rooms.push_back(room);
		delete stream;
	}

	return true;
}

bool GmExe::ReadIncludes() {
	GmkStream* stream;
	IncludeFile* include;

	exeHandle->SkipDwords(1);

	unsigned int count = exeHandle->ReadDword();
	for(; count; count--) {
		stream = new GmkStream();
		exeHandle->Deserialize(stream,true);

		include = new IncludeFile();

		stream->SkipDwords(1);

		include->fileName = stream->ReadString();
		include->filePath = stream->ReadString();

		include->originalFile = stream->ReadBool();
		include->originalSize = stream->ReadDword();

		include->storedInGmk = stream->ReadBool();

		if (include->originalFile && include->storedInGmk) {
			include->data = new GmkStream();
			stream->Deserialize(include->data,false);
		}
		else
			include->data = new GmkStream();

		include->exportFlags = stream->ReadBool();
		include->folderExport = stream->ReadString();
		include->overWrite = stream->ReadBool();
		include->freeMem = stream->ReadBool();
		include->removeGameEnd = stream->ReadBool();

		gmkHandle->includes.push_back(include);
		delete stream;
	}

	return true;
}

bool GmExe::ReadGameInformation() {
	GmkStream* stream = new GmkStream();

	exeHandle->SkipDwords(1);

	exeHandle->Deserialize(stream,true);

	gmkHandle->gameInfo.bgColor = stream->ReadDword();
	gmkHandle->gameInfo.seperateWindow = stream->ReadBool();
	gmkHandle->gameInfo.caption = stream->ReadString();
	gmkHandle->gameInfo.left = stream->ReadDword();
	gmkHandle->gameInfo.top = stream->ReadDword();
	gmkHandle->gameInfo.width = stream->ReadDword();
	gmkHandle->gameInfo.height = stream->ReadDword();
	gmkHandle->gameInfo.showWindowBorder = stream->ReadBool();
	gmkHandle->gameInfo.allowResize = stream->ReadBool();
	gmkHandle->gameInfo.stayOnTop = stream->ReadBool();
	gmkHandle->gameInfo.freezeGame = stream->ReadBool();
	gmkHandle->gameInfo.infoRtf = stream->ReadString();

	delete stream;
	return true;
}
