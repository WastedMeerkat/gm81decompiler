/*
 *	gmk.cpp
 *	GMK parser
 */

#include <iostream>
#include <fstream>
#include "zlib.h"
#include "gmk.hpp"

// Support
void GmkStream::CheckStreamForWriting(size_t len) {
	if (iPosition + len < iLength)
		return;

	iLength += len;
	iBuffer = (unsigned char*)realloc(iBuffer,iLength);
}

void GmkStream::CheckStreamForReading(size_t len) {
	if (iPosition + len > iLength) {
		if (verbose) {
			std::cerr << "[Warning] Unforseen end of stream while reading @ " << iPosition + len << ":" << iLength << std::endl;
			std::cerr << "Press any key to continue..." << std::endl;
			std::cin.get();
		}

		throw new GmkParserException("Unforseen end of stream while reading @ " + (iPosition + len));
	}
}

void GmkStream::MoveAll(GmkStream* src) {
	CheckStreamForWriting(src->GetLength());

	memcpy(iBuffer + iPosition,src->iBuffer,src->GetLength());
	src->iPosition = src->GetLength();
	iPosition += src->GetLength();
}

void GmkStream::Move(GmkStream* src) {
	int len = src->GetLength() - src->GetPosition();
	delete[] iBuffer;

	iBuffer = new unsigned char[len];
	iLength = len;
	SetPosition(0);

	memcpy(iBuffer,src->iBuffer + src->iPosition,len);
	src->iLength = len;
}

void GmkStream::Copy(GmkStream* src) {
	CheckStreamForWriting(src->GetLength());

	memcpy(iBuffer + iPosition,src->iBuffer + src->iPosition,src->iLength);
	src->iPosition += src->iLength - src->iPosition;
}

// Files
bool GmkStream::Load(const std::string& filename) {
	std::ifstream handle;

	handle.open(filename,std::ios::in | std::ios::binary);
	if (!handle.is_open())
		return false;

	// Reset position
	SetPosition(0);

	// Grab the size
	handle.seekg(0,std::ios::end);
	CheckStreamForWriting((size_t)handle.tellg());
	handle.seekg(0,std::ios::beg);

	// Read into the buffer
	handle.read((char*)iBuffer,iLength);

	handle.close();

	return true;
}

bool GmkStream::Save(const std::string& filename, int mode) {
	std::ofstream handle;

	handle.open(filename,std::ios::out | ((mode == FMODE_BINARY) ? std::ios::binary : 0));
	if (!handle.is_open())
		return false;

	// Reset position
	SetPosition(0);

	// Write
	handle.write((const char*)iBuffer,iLength);
	handle.close();

	return true;
}

// Reading
void GmkStream::ReadData(unsigned char* buffer, size_t len) {
	CheckStreamForReading(len);

	memcpy(buffer,iBuffer + iPosition,len);
	iPosition += len;
}

bool GmkStream::ReadBool() {
	return ReadDword() >= 1;
}

unsigned char GmkStream::ReadByte() {
	CheckStreamForReading(sizeof(char));

	unsigned char value = *(unsigned char*)(iBuffer + iPosition);
	iPosition += sizeof(char);

	return value;
}

unsigned short GmkStream::ReadWord() {
	CheckStreamForReading(sizeof(short));

	unsigned short value = *(unsigned short*)(iBuffer + iPosition);
	iPosition += sizeof(short);

	return value;
}

unsigned int GmkStream::ReadDword() {
	CheckStreamForReading(sizeof(int));

	unsigned int value = *(unsigned int*)(iBuffer + iPosition);
	iPosition += sizeof(int);

	return value;
}

double GmkStream::ReadDouble() {
	CheckStreamForReading(sizeof(double));

	double value = *(double*)(iBuffer + iPosition);
	iPosition += sizeof(double);

	return value;
}

std::string GmkStream::ReadString() {
	unsigned int len = ReadDword();
	if (!len)
		return std::string("");

	CheckStreamForReading(len);

	std::string res((const char*)(iBuffer + iPosition),len);
	iPosition += len;

	return res;
}

time_t GmkStream::ReadTimestamp() {
	return (time_t)(unsigned int)ReadDouble();
}

// Writing
void GmkStream::WriteData(unsigned char* buffer, size_t len) {
	CheckStreamForWriting(len);

	memcpy(iBuffer + iPosition,buffer,len);
	iPosition += len;
}

void GmkStream::WriteBool(bool value) {
	WriteDword((unsigned int)(value == 1));
}

void GmkStream::WriteByte(unsigned char value) {
	CheckStreamForWriting(sizeof(char));

	*(unsigned char*)(iBuffer + iPosition) = value;
	iPosition += sizeof(char);
}

void GmkStream::WriteWord(unsigned short value) {
	CheckStreamForWriting(sizeof(short));

	*(unsigned short*)(iBuffer + iPosition) = value;
	iPosition += sizeof(short);
}

void GmkStream::WriteDword(unsigned int value) {
	CheckStreamForWriting(sizeof(int));

	*(unsigned int*)(iBuffer + iPosition) = value;
	iPosition += sizeof(int);
}

void GmkStream::WriteDouble(double value) {
	CheckStreamForWriting(sizeof(double));

	*(double*)(iBuffer + iPosition) = value;
	iPosition += sizeof(double);
}

void GmkStream::WriteString(const std::string& value) {
	size_t len = value.length();

	WriteDword(len);
	CheckStreamForWriting(len);

	memcpy(iBuffer + iPosition,value.data(),len);
	iPosition += len;
}

void GmkStream::WriteTimestamp() {
	// Get the base time
	struct tm baseTimeCal;

	baseTimeCal.tm_year	= 1899;
	baseTimeCal.tm_mon	= 12;
	baseTimeCal.tm_mday	= 30;
	baseTimeCal.tm_hour	= 23;
	baseTimeCal.tm_min	= 59;
	baseTimeCal.tm_sec	= 59;

	time_t baseTime = mktime(&baseTimeCal);

	// Don't ask how I got this number, seriously, you don't want to know
	// It's close enough anyway, less than a day off, which is better than several 10,000 years off
	WriteDouble((double)(((time(NULL) - baseTime)) / (double)32039.14720457607));
}

// Compression
unsigned char* GmkStream::DeflateStream(unsigned char* buffer, size_t iLen, size_t *oLen) {
	// Find the required buffer size
	uLongf destSize = (uLongf)((uLongf)iLen * 1.001) + 12;
	Bytef* destBuffer = new Bytef[destSize];

	// Deflate
	int result = compress2(destBuffer,&destSize,(const Bytef*)buffer,iLen,Z_BEST_COMPRESSION);
	if (result != Z_OK) {
		*oLen = 0;
		return 0;
	}

	*oLen = (size_t)destSize;

	return (unsigned char*)destBuffer;
}

unsigned char* GmkStream::InflateStream(unsigned char* buffer, size_t iLen, size_t* oLen) {
	z_stream stream;
	int len = iLen, offset, retval;
	char* out = new char[iLen];
	memset(&stream,0,sizeof(stream));

	stream.next_in	 = (Bytef*)buffer;
	stream.avail_in	 = iLen;
	stream.next_out	 = (Bytef*)out;
	stream.avail_out = iLen;

	inflateInit(&stream);
	retval = inflate(&stream,1);
	while(stream.avail_in && !retval) {
		offset = (int)stream.next_out - (int)out;
		len += 0x800;
		stream.avail_out += 0x800;
		out = (char*)realloc(out,len);

		stream.next_out = (Bytef*)((int)out + offset);
		retval = inflate(&stream,1);
	}

	if(!retval) {
		if (verbose)
			std::cerr << std::endl << "[Warning] Unfinished compression?";
	} else if (retval != 1) {
		if (verbose)
			std::cerr << std::endl << "[Error  ] Compression error " << retval;
	
		throw new GmkParserException("Compression error");
	}

	len = stream.total_out;
	out = (char*)realloc((void*)out,len);
	inflateEnd(&stream);

	*oLen = len;

	return (unsigned char*)out;
}

bool GmkStream::Deflate() {
	GmkStream* tmp = new GmkStream;

	tmp->iBuffer = DeflateStream(iBuffer,iLength,&tmp->iLength);
	if (!tmp->iLength) {
		if (verbose)
			std::cerr << "[Error  ] Deflation failed!" << std::endl;
		
		return false;
	}

	tmp->SetPosition(0);
	Move(tmp);

	delete tmp;

	return true;
}

bool GmkStream::Inflate() {
	GmkStream* tmp = new GmkStream;

	tmp->iBuffer = this->InflateStream(iBuffer,iLength,&tmp->iLength);
	if (!tmp->iLength)
		return false;

	tmp->SetPosition(0);

	Move(tmp);
	iLength = tmp->iLength;

	delete tmp;

	return true;
}

// Serialization
bool GmkStream::Deserialize(GmkStream* dest, bool decompress) {
	unsigned int len = ReadDword();
	CheckStreamForReading(len - 1);

	delete[] dest->iBuffer;

	dest->iBuffer = new unsigned char[len];
	dest->iLength = len;
	dest->SetPosition(0);

	memcpy(dest->iBuffer,iBuffer + iPosition,len);
	iPosition += len;

	if (decompress)
		return dest->Inflate();

	return true;
}

bool GmkStream::Serialize(bool compress) {
	GmkStream* tmp = new GmkStream;
	iPosition = 0;

	if (compress) {
		if (!Deflate())
			return false;
	}

	tmp->WriteDword(GetLength());
	tmp->Copy(this);

	delete[] iBuffer;

	iBuffer = tmp->iBuffer;
	iPosition = tmp->GetPosition();
	iLength = tmp->GetLength();

	return true;
}

// Support
void Gmk::Log(const std::string& value, bool error) {
	if (!verbose)
		return;

	if (error)
		std::cerr << "[Error  ] ";
	
	std::cout << value.c_str() << std::endl;
}

// Files
bool Gmk::Load(const std::string& filename) {
	if (!gmkHandle->Load(filename))
		return false;

	// Check magic
	if (gmkHandle->ReadDword() != GMK_MAGIC)
		return false;

	// Check version
	gmkVer = gmkHandle->ReadDword();
	if (gmkVer != 800 && gmkVer != 810)
		return false;

	// Game ID & GUID
	gameId = gmkHandle->ReadDword();
	gmkHandle->SkipDwords(4);

	// Read data
	Log("Reading settings...",false);
	if (!ReadSettings())
		return false;

	Log("Reading triggers...",false);
	if (!ReadTriggers())
		return false;
	
	Log("Reading constants...",false);
	if (!ReadConstants())
		return false;

	Log("Reading sounds...",false);
	if (!ReadSounds())
		return false;

	Log("Reading sprites...",false);
	if (!ReadSprites())
		return false;

	Log("Reading backgrounds...",false);
	if (!ReadBackgrounds())
		return false;

	Log("Reading paths...",false);
	if (!ReadPaths())
		return false;

	Log("Reading scripts...",false);
	if (!ReadScripts())
		return false;

	Log("Reading fonts... ",false);
	if (!ReadFonts())
		return false;

	Log("Reading timelines...",false);
	if (!ReadTimelines())
		return false;

	Log("Reading objects...",false);
	if (!ReadObjects())
		return false;

	Log("Reading rooms...",false);
	if (!ReadRooms())
		return false;

	lastInstance = gmkHandle->ReadDword();
	lastTile = gmkHandle->ReadDword();

	Log("Reading include files...",false);
	if (!ReadIncludedFiles())
		return false;

	Log("Reading packages...",false);
	gmkHandle->SkipDwords(1);
	int count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++)
		packages.push_back(gmkHandle->ReadString());

	Log("Reading game info...",false);
	if (!ReadGameInformation())
		return false;

	gmkHandle->SkipDwords(1);
	count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++)
		libCreationCode.push_back(gmkHandle->ReadString());

	gmkHandle->SkipDwords(1);
	count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++)
		roomExecutionOrder.push_back(gmkHandle->ReadDword());

	Log("Reading resource tree...",false);
	if (!ReadResourceTree())
		return false;

	return true;
}

bool Gmk::Save(const std::string& filename, unsigned int ver) {
	// Check if ver is supported
	if (ver != 800 && ver != 810)
		return false;

	gmkVer = ver;

	// Write magic & version
	gmkSaveHandle->WriteDword(GMK_MAGIC);
	gmkSaveHandle->WriteDword(gmkVer);

	// Game ID and pseudo GUID
	gmkSaveHandle->WriteDword(gameId);
	for(int i = 0; i < 4; i++)
		gmkSaveHandle->WriteDword(2147483648 - (gameId * 16) - i * 8);

	// Write settings
	Log("Writing settings...",false);
	if (!WriteSettings())
		return false;

	Log("Writing triggers...",false);
	if (!WriteTriggers())
		return false;

	Log("Writing constants...",false);
	if (!WriteConstants())
		return false;

	Log("Writing sounds...",false);
	if (!WriteSounds())
		return false;

	Log("Writing sprites...",false);
	if (!WriteSprites())
		return false;

	Log("Writing backgrounds...",false);
	if (!WriteBackgrounds())
		return false;

	Log("Writing paths...",false);
	if (!WritePaths())
		return false;

	Log("Writing scripts...",false);
	if (!WriteScripts())
		return false;

	Log("Writing fonts...",false);
	if (!WriteFonts())
		return false;

	Log("Writing time lines...",false);
	if (!WriteTimelines())
		return false;

	Log("Writing objects...",false);
	if (!WriteObjects())
		return false;

	Log("Writing rooms...",false);
	if (!WriteRooms())
		return false;

	gmkSaveHandle->WriteDword(lastInstance);
	gmkSaveHandle->WriteDword(lastTile);

	Log("Writing included files...",false);
	if (!WriteIncludedFiles())
		return false;

	Log("Writing packages...",false);
	gmkSaveHandle->WriteDword(700);
	gmkSaveHandle->WriteDword(packages.size());
	for(size_t i = 0; i < packages.size(); i++)
		gmkSaveHandle->WriteString(packages[i]);

	Log("Writing game information...",false);
	if (!WriteGameInformation())
		return false;

	gmkSaveHandle->WriteDword(500);
	gmkSaveHandle->WriteDword(libCreationCode.size());
	for(size_t i = 0; i < libCreationCode.size(); i++)
		gmkSaveHandle->WriteString(libCreationCode[i]);

	gmkSaveHandle->WriteDword(700);
	gmkSaveHandle->WriteDword(roomExecutionOrder.size());
	for(size_t i = 0; i < roomExecutionOrder.size(); i++)
		gmkSaveHandle->WriteDword(roomExecutionOrder[i]);

	Log("Writing resource tree...",false);
	if (!WriteResourceTree())
		return false;

	if (!gmkSaveHandle->Save(filename,FMODE_BINARY))
		return false;

	return true;
}

void Gmk::SetDefaults() {
	// Resource tree
	ResourceTree* rtEntry = new ResourceTree;
	rtEntry->status = 1;
	rtEntry->group = 2;
	rtEntry->index = 0;
	rtEntry->name = "Sprites";
	resourceTree.push_back(rtEntry);

	rtEntry = new ResourceTree;
	rtEntry->status = 1;
	rtEntry->group = 3;
	rtEntry->index = 0;
	rtEntry->name = "Sounds";
	resourceTree.push_back(rtEntry);

	rtEntry = new ResourceTree;
	rtEntry->status = 1;
	rtEntry->group = 6;
	rtEntry->index = 0;
	rtEntry->name = "Backgrounds";
	resourceTree.push_back(rtEntry);

	rtEntry = new ResourceTree;
	rtEntry->status = 1;
	rtEntry->group = 8;
	rtEntry->index = 0;
	rtEntry->name = "Paths";
	resourceTree.push_back(rtEntry);

	rtEntry = new ResourceTree;
	rtEntry->status = 1;
	rtEntry->group = 7;
	rtEntry->index = 0;
	rtEntry->name = "Scripts";
	resourceTree.push_back(rtEntry);

	rtEntry = new ResourceTree;
	rtEntry->status = 1;
	rtEntry->group = 9;
	rtEntry->index = 0;
	rtEntry->name = "Fonts";
	resourceTree.push_back(rtEntry);

	rtEntry = new ResourceTree;
	rtEntry->status = 1;
	rtEntry->group = 12;
	rtEntry->index = 0;
	rtEntry->name = "Time Lines";
	resourceTree.push_back(rtEntry);

	rtEntry = new ResourceTree;
	rtEntry->status = 1;
	rtEntry->group = 1;
	rtEntry->index = 0;
	rtEntry->name = "Objects";
	resourceTree.push_back(rtEntry);

	rtEntry = new ResourceTree;
	rtEntry->status = 1;
	rtEntry->group = 4;
	rtEntry->index = 0;
	rtEntry->name = "Rooms";
	resourceTree.push_back(rtEntry);

	rtEntry = new ResourceTree;
	rtEntry->status = 3;
	rtEntry->group = 10;
	rtEntry->index = 0;
	rtEntry->name = "Game Information";
	resourceTree.push_back(rtEntry);

	rtEntry = new ResourceTree;
	rtEntry->status = 3;
	rtEntry->group = 11;
	rtEntry->index = 0;
	rtEntry->name = "Global Game Settings";
	resourceTree.push_back(rtEntry);

	rtEntry = new ResourceTree;
	rtEntry->status = 3;
	rtEntry->group = 13;
	rtEntry->index = 0;
	rtEntry->name = "Extention Packages";
	resourceTree.push_back(rtEntry);

	// Global Game Settings
	srand((unsigned int)time(NULL));
	gameId = (rand() * rand()) % GMK_MAX_ID;

	settings.fullscreen = false;
	settings.dontDrawBorder = false;
	settings.displayCursor = true;
	settings.scaling = -1;
	settings.allowWindowResize = false;
	settings.onTop = false;
	settings.colorOutsideRoom = 0;
	settings.setResolution = false;
	settings.colorDepth = settings.resolution = settings.frequency = 0;
	settings.dontShowButtons = false;
	settings.vsync = false;
	settings.disableScreen = true;
	settings.letF4 = settings.letF1 = settings.letF5 = settings.letF9 = settings.letEsc = settings.treatCloseAsEsc = true;
	settings.priority = 0;
	settings.freeze = false;
	settings.loadingBar = 1;
	settings.customLoadImage = false;
	settings.transparent = false;
	settings.translucency = 255;
	settings.scaleProgressBar = true;

	// Icons are just bitmaps -- 22486 bytes for GM8.0+
	settings.iconData = new GmkStream;

	// Icon Header
	settings.iconData->WriteWord(0);		// Reserved
	settings.iconData->WriteWord(1);		// 1 = Icon, 2 = Cursor
	settings.iconData->WriteWord(1);		// One image in file

	// Image header
	settings.iconData->WriteByte(32);		// 32px width
	settings.iconData->WriteByte(32);		// 32px height
	settings.iconData->WriteByte(0);		// Truecolor
	settings.iconData->WriteByte(0);		// Reserved
	settings.iconData->WriteWord(0);		// Color planes
	settings.iconData->WriteWord(32);		// 32bits per pixel
	settings.iconData->WriteDword(22486 - 16 - 6);
	settings.iconData->WriteDword(16 + 6);	// Offset

	// DIB Header
	settings.iconData->WriteDword(40);		// Size of this header
	settings.iconData->WriteDword(32);		// Width
	settings.iconData->WriteDword(64);		// Height * 2
	settings.iconData->WriteWord(1);		// Color planes, must be 1
	settings.iconData->WriteDword(32);		// 32 bit color
	settings.iconData->WriteDword(0);		// No compression
	settings.iconData->WriteDword(22486 - 40 - 16 - 6);
	settings.iconData->WriteDword(1);		// Pixels per meter
	settings.iconData->WriteDword(1);		// Pixels per meter
	settings.iconData->WriteDword(0);		// Number of colors, set to 0 to default to 2 ^ n
	settings.iconData->WriteDword(0);		// Number of important colors, lolwat

	// Make white pixels
	for(int i = 0; i < 22486 - 40 - 16 - 6; i++)
		settings.iconData->WriteByte(0xFF);

	settings.errorDisplay = true;
	settings.errorLog = false;
	settings.errorAbort = false;
	settings.treatAsZero = false;
	settings.errorOnUninitialization = true;

	settings.author = "";
	settings.version = "100";
	settings.lastChanged = 0;
	settings.information = "";
	settings.majorVersion = 1;
	settings.minorVersion = 0;
	settings.releaseVersion = 0;
	settings.buildVersion = 0;
	settings.company = "";
	settings.product = "";
	settings.copyright = "";
	settings.description = "";
	settings.lastSettingsChanged = 0;

	gameInfo.seperateWindow = false;
	gameInfo.left = -1;
	gameInfo.top = -1;
	gameInfo.width = 600;
	gameInfo.height = 400;
	gameInfo.showWindowBorder = true;
	gameInfo.allowResize = true;
	gameInfo.stayOnTop = false;
	gameInfo.freezeGame = true;

	gameInfo.caption = "Game Information";
	gameInfo.infoRtf = "{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033{\\fonttbl{\\f0\\fnil\\fcharset0 Calibri;}}{\\*\\generator Msftedit 5.41.21.2509;}\\viewkind4\\uc1\\pard\\sa200\\sl276\\slmult1\\lang9\\f0\\fs22\\par}\0";
}

// Reading
bool Gmk::ReadSettings() {
	gmkHandle->SkipDwords(1);

	// Decompress settings block
	GmkStream* setHandle = new GmkStream;
	gmkHandle->Deserialize(setHandle,true);

	// Read settings
	settings.fullscreen			= setHandle->ReadBool();
	settings.interpolate		= setHandle->ReadBool();
	settings.dontDrawBorder		= setHandle->ReadBool();
	settings.displayCursor		= setHandle->ReadBool();
	settings.scaling			= setHandle->ReadDword();
	settings.allowWindowResize	= setHandle->ReadBool();
	settings.onTop				= setHandle->ReadBool();
	settings.colorOutsideRoom	= setHandle->ReadDword();
	settings.setResolution		= setHandle->ReadBool();
	settings.colorDepth			= setHandle->ReadDword();
	settings.resolution			= setHandle->ReadDword();
	settings.frequency			= setHandle->ReadDword();
	settings.dontShowButtons	= setHandle->ReadBool();
	settings.vsync				= setHandle->ReadBool();
	settings.disableScreen		= setHandle->ReadBool();
	settings.letF4				= setHandle->ReadBool();
	settings.letF1				= setHandle->ReadBool();
	settings.letEsc				= setHandle->ReadBool();
	settings.letF5				= setHandle->ReadBool();
	settings.letF9				= setHandle->ReadBool();
	settings.treatCloseAsEsc	= setHandle->ReadBool();
	settings.priority			= setHandle->ReadDword();
	settings.freeze				= setHandle->ReadBool();

	// Loading bar
	settings.loadingBar = setHandle->ReadDword();
	if (settings.loadingBar == 2) {
		if (setHandle->ReadDword()) {
			settings.backData = new GmkStream;
			setHandle->Deserialize(settings.backData,true);
		}

		if (setHandle->ReadDword()) {
			settings.frontData = new GmkStream;
			setHandle->Deserialize(settings.frontData,true);
		}
	}

	settings.customLoadImage = setHandle->ReadBool();
	if (settings.customLoadImage) {
		if (setHandle->ReadDword()) {
			settings.loadBar = new GmkStream;
			setHandle->Deserialize(settings.loadBar,true);
		}
	}

	settings.transparent		= setHandle->ReadBool();
	settings.translucency		= setHandle->ReadDword();
	settings.scaleProgressBar	= setHandle->ReadBool();
	
	// Icon
	settings.iconData = new GmkStream;
	setHandle->Deserialize(settings.iconData,false);

	settings.errorDisplay		= setHandle->ReadBool();
	settings.errorLog			= setHandle->ReadBool();
	settings.errorAbort			= setHandle->ReadBool();
	settings.treatAsZero		= setHandle->ReadDword();		// GM8.1 support
	settings.author				= setHandle->ReadString();
	settings.version			= setHandle->ReadString();
	settings.lastChanged		= setHandle->ReadTimestamp();
	settings.information		= setHandle->ReadString();
	settings.majorVersion		= setHandle->ReadDword();
	settings.minorVersion		= setHandle->ReadDword();
	settings.releaseVersion		= setHandle->ReadDword();
	settings.buildVersion		= setHandle->ReadDword();
	settings.company			= setHandle->ReadString();
	settings.product			= setHandle->ReadString();
	settings.copyright			= setHandle->ReadString();
	settings.description		= setHandle->ReadString();
	settings.lastSettingsChanged= setHandle->ReadTimestamp();

	if (gmkVer == 810) {
		settings.errorOnUninitialization = settings.treatAsZero & 0x00000002;
		settings.treatAsZero &= 0x00000001;
	}

	delete setHandle;

	return true;
}

bool Gmk::ReadTriggers() {
	GmkStream* trigHandle;
	Trigger* trigger;
	gmkHandle->SkipDwords(1);

	int count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++) {
		trigger = new Trigger;
		trigHandle = new GmkStream;
		gmkHandle->Deserialize(trigHandle,true);

		if (!trigHandle->ReadBool()) {
			triggers.push_back(trigger);
			delete trigHandle;
			continue;
		}

		trigHandle->SkipDwords(1);
		trigger->name = trigHandle->ReadString();
		trigger->condition = trigHandle->ReadString();
		trigger->checkMoment = trigHandle->ReadDword();
		trigger->constantName = trigHandle->ReadString();

		triggers.push_back(trigger);

		delete trigHandle;
	}

	triggersChanged = gmkHandle->ReadTimestamp();

	return true;
}

bool Gmk::ReadConstants() {
	Constant* constant;
	gmkHandle->SkipDwords(1);

	int count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++) {
		constant = new Constant;
		constant->name = gmkHandle->ReadString();
		constant->value = gmkHandle->ReadString();

		constants.push_back(constant);
	}

	constantsChanged = gmkHandle->ReadTimestamp();

	return true;
}

bool Gmk::ReadSounds() {
	GmkStream* soundHandle;
	Sound* sound;

	gmkHandle->SkipDwords(1);
	int count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++) {
		sound = new Sound;
		soundHandle = new GmkStream;
		gmkHandle->Deserialize(soundHandle,true);

		if (!soundHandle->ReadDword()) {
			sounds.push_back(0);
			delete soundHandle;
			delete sound;
			continue;
		}

		sound->name = soundHandle->ReadString();
		sound->lastChanged = soundHandle->ReadTimestamp();
		soundHandle->SkipDwords(1);
		sound->kind = soundHandle->ReadDword();
		sound->fileType = soundHandle->ReadString();
		sound->fileName = soundHandle->ReadString();
		
		if (soundHandle->ReadBool()) {
			sound->data = new GmkStream;
			soundHandle->Deserialize(sound->data,false);
		}

		sound->effects = soundHandle->ReadDword();
		sound->volume = soundHandle->ReadDouble();
		sound->pan = soundHandle->ReadDouble();
		sound->preload = soundHandle->ReadBool();

		sounds.push_back(sound);
		delete soundHandle;
	}

	return true;
}

bool Gmk::ReadSprites() {
	GmkStream* spriteHandle;
	Sprite* sprite;

	gmkHandle->SkipDwords(1);
	int count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++) {
		sprite = new Sprite;
		spriteHandle = new GmkStream;
		gmkHandle->Deserialize(spriteHandle,true);

		if (!spriteHandle->ReadBool()) {
			sprites.push_back(0);
			delete spriteHandle;
			delete sprite;
			continue;
		}

		sprite->name = spriteHandle->ReadString();
		sprite->lastChanged = spriteHandle->ReadTimestamp();
		spriteHandle->SkipDwords(1);

		sprite->originX = spriteHandle->ReadDword();
		sprite->originY = spriteHandle->ReadDword();

		int imgCount = spriteHandle->ReadDword();
		for(int j = 0; j < imgCount; j++) {
			SubImage* subImageHandle = new SubImage;
			
			spriteHandle->SkipDwords(1);
			subImageHandle->width = spriteHandle->ReadDword();
			subImageHandle->height = spriteHandle->ReadDword();

			subImageHandle->data = new GmkStream;
			spriteHandle->Deserialize(subImageHandle->data,false);

			sprite->images.push_back(subImageHandle);
		}

		sprite->shape = spriteHandle->ReadDword();
		sprite->alphaTolerance = spriteHandle->ReadDword();
		sprite->seperateMask = spriteHandle->ReadBool();
		sprite->boundingBox = spriteHandle->ReadDword();
		sprite->left = spriteHandle->ReadDword();
		sprite->right = spriteHandle->ReadDword();
		sprite->bottom = spriteHandle->ReadDword();
		sprite->top = spriteHandle->ReadDword();

		sprites.push_back(sprite);
		delete spriteHandle;
	}

	return true;
}

bool Gmk::ReadBackgrounds() {
	GmkStream* backgroundHandle;
	Background* background;

	gmkHandle->SkipDwords(1);

	int count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++) {
		background = new Background;
		backgroundHandle = new GmkStream;
		gmkHandle->Deserialize(backgroundHandle,true);

		if (!backgroundHandle->ReadBool()) {
			backgrounds.push_back(0);
			delete backgroundHandle;
			delete background;
			continue;
		}

		background->name = backgroundHandle->ReadString();
		background->lastChanged = backgroundHandle->ReadTimestamp();
		backgroundHandle->SkipDwords(1);

		background->tileset = backgroundHandle->ReadBool();
		background->tileWidth = backgroundHandle->ReadDword();
		background->tileHeight = backgroundHandle->ReadDword();
		background->hOffset = backgroundHandle->ReadDword();
		background->vOffset = backgroundHandle->ReadDword();
		background->hSep = backgroundHandle->ReadDword();
		background->vSep = backgroundHandle->ReadDword();

		backgroundHandle->SkipDwords(1);
		background->width = backgroundHandle->ReadDword();
		background->height = backgroundHandle->ReadDword();

		background->data = 0;
		if (background->width > 0 && background->height > 0) {
			background->data = new GmkStream;
			backgroundHandle->Deserialize(background->data,false);
		}

		backgrounds.push_back(background);
		delete backgroundHandle;
	}

	return true;
}

bool Gmk::ReadPaths() {
	GmkStream* pathHandle;
	Path* path;

	gmkHandle->SkipDwords(1);

	int count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++) {
		path = new Path;
		pathHandle = new GmkStream;
		gmkHandle->Deserialize(pathHandle,true);

		if (!pathHandle->ReadBool()) {
			paths.push_back(0);
			delete pathHandle;
			delete path;
			continue;
		}

		path->name = pathHandle->ReadString();
		path->lastChanged = pathHandle->ReadTimestamp();

		pathHandle->SkipDwords(1);
		path->kind = pathHandle->ReadDword();
		path->closed = pathHandle->ReadBool();
		path->prec = pathHandle->ReadDword();
		path->roomIndexBG = pathHandle->ReadDword();
		path->snapX = pathHandle->ReadDword();
		path->snapY = pathHandle->ReadDword();

		int count = pathHandle->ReadDword();
		for(int j = 0; j < count; j++) {
			PathPoint* point = new PathPoint;
			point->x = pathHandle->ReadDouble();
			point->y = pathHandle->ReadDouble();
			point->speed = pathHandle->ReadDouble();
		}

		paths.push_back(path);
		delete pathHandle;
	}

	return true;
}

bool Gmk::ReadScripts() {
	GmkStream* scriptHandle;
	Script* script;

	gmkHandle->SkipDwords(1);
	int count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++) {
		script = new Script;
		scriptHandle = new GmkStream;
		gmkHandle->Deserialize(scriptHandle,true);

		if (!scriptHandle->ReadBool()) {
			scripts.push_back(0);
			delete scriptHandle;
			delete script;
			continue;
		}

		script->name = scriptHandle->ReadString();
		script->lastChanged = scriptHandle->ReadTimestamp();
		scriptHandle->SkipDwords(1);
		script->value = scriptHandle->ReadString();

		scripts.push_back(script);
		delete scriptHandle;
	}

	return true;
}

bool Gmk::ReadFonts() {
	GmkStream* fontHandle;
	Font* font;

	gmkHandle->SkipDwords(1);
	int count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++) {
		font = new Font;
		fontHandle = new GmkStream;
		gmkHandle->Deserialize(fontHandle,true);

		if (!fontHandle->ReadBool()) {
			fonts.push_back(0);
			delete fontHandle;
			delete font;
			continue;
		}

		font->name = fontHandle->ReadString();
		font->lastChanged = fontHandle->ReadTimestamp();
		fontHandle->SkipDwords(1);

		font->fontName = fontHandle->ReadString();
		font->size = fontHandle->ReadDword();
		font->bold = fontHandle->ReadBool();
		font->italic = fontHandle->ReadBool();
		font->rangeBegin = fontHandle->ReadDword();
		font->rangeEnd = fontHandle->ReadDword();

		if (gmkVer == 810) {
			font->charset = font->rangeBegin & 0xFF000000;
			font->aaLevel = font->rangeBegin & 0x00FF0000;
			font->rangeBegin &= 0x0000FFFF;
		}

		fonts.push_back(font);
		delete fontHandle;
	}

	return true;
}

bool Gmk::ReadAction(GmkStream* handle, ObjectAction* action) {
	handle->SkipDwords(1);

	action->libId = handle->ReadDword();
	action->actId = handle->ReadDword();
	action->kind = handle->ReadDword();
	action->mayBeRelative = handle->ReadBool();
	action->question = handle->ReadBool();
	action->appliesToSomething = handle->ReadBool();
	action->type = handle->ReadDword();
	action->functionName = handle->ReadString();
	action->functionCode = handle->ReadString();

	action->argumentsUsed = handle->ReadDword();
	int ac = handle->ReadDword();
	if (ac > 8)
		return false;

	for(int i = 0; i < ac; i++)
		action->argumentKind[i] = handle->ReadDword();

	action->appliesToObject = handle->ReadDword();
	action->isRelative = handle->ReadBool();

	ac = handle->ReadDword();
	if (ac > 8)
		return false;

	for(int i = 0; i < ac; i++)
		action->argumentValue[i] = handle->ReadString();

	action->not = handle->ReadBool();

	return true;
}

bool Gmk::ReadTimelines() {
	GmkStream* timelineHandle;
	Timeline* timeline;

	gmkHandle->SkipDwords(1);
	int count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++) {
		timeline = new Timeline;
		timelineHandle = new GmkStream;
		gmkHandle->Deserialize(timelineHandle,true);

		if (!timelineHandle->ReadBool()) {
			timelines.push_back(0);
			delete timelineHandle;
			delete timeline;
			continue;
		}

		timeline->name = timelineHandle->ReadString();
		timeline->lastChanged = timelineHandle->ReadTimestamp();
		timelineHandle->SkipDwords(1);

		int m = timelineHandle->ReadDword();
		for(int j = 0; j < m; j++) {
			TimelineMoment* moment = new TimelineMoment;
			moment->moment = timelineHandle->ReadDword();

			timelineHandle->SkipDwords(1);
			int c = timelineHandle->ReadDword();
			for(int k = 0; k < c; k++) {
				ObjectAction* action = new ObjectAction;
				ReadAction(timelineHandle,action);

				moment->actions.push_back(action);
			}

			timeline->moments.push_back(moment);
		}

		timelines.push_back(timeline);
		delete timelineHandle;
	}

	return true;
}

bool Gmk::ReadObjects() {
	GmkStream* objectHandle;
	Object* object;

	gmkHandle->SkipDwords(1);
	int count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++) {
		object = new Object;
		objectHandle = new GmkStream;
		gmkHandle->Deserialize(objectHandle,true);

		if (!objectHandle->ReadBool()) {
			objects.push_back(0);
			delete objectHandle;
			delete object;
			continue;
		}

		object->name = objectHandle->ReadString();
		object->lastChanged = objectHandle->ReadTimestamp();
		objectHandle->SkipDwords(1);
		
		object->spriteIndex = objectHandle->ReadDword();
		object->solid = objectHandle->ReadBool();
		object->visible = objectHandle->ReadBool();
		object->depth = objectHandle->ReadDword();
		object->persistent = objectHandle->ReadBool();
		object->parentIndex = objectHandle->ReadDword();
		object->maskIndex = objectHandle->ReadDword();
		
		int evCount = objectHandle->ReadDword() + 1;
		for(int j = 0; j < evCount; j++) {
			for(;;) {
				int first = objectHandle->ReadDword();
				if (first == -1)
					break;

				ObjectEvent* objEvent = new ObjectEvent;
				objEvent->eventType = j;
				objEvent->eventKind = first;

				objectHandle->SkipDwords(1);

				int ec = objectHandle->ReadDword();
				for(int k = 0; k < ec; k++) {
					ObjectAction* action = new ObjectAction;
					if (!ReadAction(objectHandle,action))
						return false;

					objEvent->actions.push_back(action);
				}

				object->events.push_back(objEvent);
			}
		}

		objects.push_back(object);
		delete objectHandle;
	}

	return true;
}

bool Gmk::ReadRooms() {
	GmkStream* roomHandle;
	Room* room;

	gmkHandle->SkipDwords(1);
	int count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++) {
		room = new Room;
		roomHandle = new GmkStream;
		gmkHandle->Deserialize(roomHandle,true);

		if (!roomHandle->ReadBool()) {
			rooms.push_back(0);
			delete roomHandle;
			delete room;
			continue;
		}

		room->name = roomHandle->ReadString();
		room->lastChanged = roomHandle->ReadTimestamp();
		roomHandle->SkipDwords(1);

		room->caption = roomHandle->ReadString();
		room->width = roomHandle->ReadDword();
		room->height = roomHandle->ReadDword();
		room->snapY = roomHandle->ReadDword();
		room->snapX = roomHandle->ReadDword();
		room->isoGrid = roomHandle->ReadBool();
		room->speed = roomHandle->ReadDword();
		room->persistent = roomHandle->ReadBool();
		room->bgColor = roomHandle->ReadDword();
		room->bgColorDraw = roomHandle->ReadBool();
		room->creationCode = roomHandle->ReadString();
		
		int bgCount = roomHandle->ReadDword();
		for(int j = 0; j < bgCount; j++) {
			RoomBackground* rBackground = new RoomBackground;
			rBackground->visible = roomHandle->ReadBool();
			rBackground->foreground = roomHandle->ReadBool();
			rBackground->bgIndex = roomHandle->ReadDword();
			rBackground->x = roomHandle->ReadDword();
			rBackground->y = roomHandle->ReadDword();
			rBackground->tileH = roomHandle->ReadBool();
			rBackground->tileV = roomHandle->ReadBool();
			rBackground->vSpeed = roomHandle->ReadDword();
			rBackground->hSpeed = roomHandle->ReadDword();
			rBackground->stretch = roomHandle->ReadBool();

			room->backgrounds.push_back(rBackground);
		}

		room->enableViews = roomHandle->ReadBool();
		int viewCount = roomHandle->ReadDword();
		for(int j = 0; j < viewCount; j++) {
			RoomView* rView = new RoomView;
			rView->visible = roomHandle->ReadBool();
			rView->viewX = roomHandle->ReadDword();
			rView->viewY = roomHandle->ReadDword();
			rView->viewW = roomHandle->ReadDword();
			rView->viewH = roomHandle->ReadDword();
			rView->portX = roomHandle->ReadDword();
			rView->portY = roomHandle->ReadDword();
			rView->portW = roomHandle->ReadDword();
			rView->portH = roomHandle->ReadDword();
			rView->hBorder = roomHandle->ReadDword();
			rView->vBorder = roomHandle->ReadDword();
			rView->hSpd = roomHandle->ReadDword();
			rView->vSpd = roomHandle->ReadDword();
			rView->objFollow = roomHandle->ReadDword();

			room->views.push_back(rView);
		}

		int instCount = roomHandle->ReadDword();
		for(int j = 0; j < instCount; j++) {
			RoomInstance* rInstance = new RoomInstance;
			rInstance->x = roomHandle->ReadDword();
			rInstance->y = roomHandle->ReadDword();
			rInstance->objIndex = roomHandle->ReadDword();
			rInstance->id = roomHandle->ReadDword();
			rInstance->creationCode = roomHandle->ReadString();
			rInstance->locked = roomHandle->ReadBool();

			room->instances.push_back(rInstance);
		}

		int tileCount = roomHandle->ReadDword();
		for(int j = 0; j < tileCount; j++) {
			RoomTile* rTile = new RoomTile;
			rTile->x = roomHandle->ReadDword();
			rTile->y = roomHandle->ReadDword();
			rTile->bgIndex = roomHandle->ReadDword();
			rTile->tileX = roomHandle->ReadDword();
			rTile->tileY = roomHandle->ReadDword();
			rTile->width = roomHandle->ReadDword();
			rTile->height = roomHandle->ReadDword();
			rTile->depth = roomHandle->ReadDword();
			rTile->id = roomHandle->ReadDword();
			rTile->locked = roomHandle->ReadBool();

			room->tiles.push_back(rTile);
		}

		room->rememberRoomEditorInfo = roomHandle->ReadBool();
		room->editorWidth = roomHandle->ReadDword();
		room->editorHeight = roomHandle->ReadDword();
		room->showGrid = roomHandle->ReadBool();
		room->showObjects = roomHandle->ReadBool();
		room->showTiles = roomHandle->ReadBool();
		room->showBackgrounds = roomHandle->ReadBool();
		room->showForegrounds = roomHandle->ReadBool();
		room->showViews = roomHandle->ReadBool();
		room->deleteUnderlyingObjects = roomHandle->ReadBool();
		room->deleteUnderlyingTiles = roomHandle->ReadBool();
		room->tab = roomHandle->ReadDword();
		room->xPosScroll = roomHandle->ReadDword();
		room->yPosScroll = roomHandle->ReadDword();

		rooms.push_back(room);
		delete roomHandle;
	}

	return true;
}

bool Gmk::ReadIncludedFiles() {
	GmkStream* ifHandle;
	IncludeFile* iFile;

	gmkHandle->SkipDwords(1);
	int count = gmkHandle->ReadDword();
	for(int i = 0; i < count; i++) {
		iFile = new IncludeFile;
		ifHandle = new GmkStream;
		gmkHandle->Deserialize(ifHandle,true);

		iFile->lastChanged = ifHandle->ReadTimestamp();
		ifHandle->SkipDwords(1);

		iFile->fileName = ifHandle->ReadString();
		iFile->filePath = ifHandle->ReadString();
		iFile->originalFile = ifHandle->ReadBool();
		iFile->originalSize = ifHandle->ReadDword();
		iFile->storedInGmk = ifHandle->ReadBool();

		if (iFile->storedInGmk) {
			iFile->data = new GmkStream;
			ifHandle->Deserialize(iFile->data,false);
		}

		iFile->exportFlags = ifHandle->ReadDword();
		iFile->folderExport = ifHandle->ReadString();
		iFile->overWrite = ifHandle->ReadBool();
		iFile->freeMem = ifHandle->ReadBool();
		iFile->removeGameEnd = ifHandle->ReadBool();

		includes.push_back(iFile);
		delete ifHandle;
	}

	return true;
}

bool Gmk::ReadGameInformation() {
	GmkStream* giHandle = new GmkStream;
	gmkHandle->SkipDwords(1);
	gmkHandle->Deserialize(giHandle,true);

	gameInfo.bgColor = giHandle->ReadDword();
	gameInfo.seperateWindow = giHandle->ReadBool();
	gameInfo.caption = giHandle->ReadString();
	gameInfo.left = giHandle->ReadDword();
	gameInfo.top = giHandle->ReadDword();
	gameInfo.width = giHandle->ReadDword();
	gameInfo.height = giHandle->ReadDword();
	gameInfo.showWindowBorder = giHandle->ReadBool();
	gameInfo.allowResize = giHandle->ReadBool();
	gameInfo.stayOnTop = giHandle->ReadBool();
	gameInfo.freezeGame = giHandle->ReadBool();
	gameInfo.lastChanged = giHandle->ReadTimestamp();
	gameInfo.infoRtf = giHandle->ReadString();

	delete giHandle;

	return true;
}

bool Gmk::ReadResourceTree() {
	ResourceTree* rcEntry;

	// Quickly clear the RC tree vector so we may read it proper
	resourceTree.clear();

	for(int i = 0; i < 12; i++) {
		rcEntry = new ResourceTree;
		rcEntry->status = gmkHandle->ReadDword();
		rcEntry->group = gmkHandle->ReadDword();
		rcEntry->index = gmkHandle->ReadDword();
		rcEntry->name = gmkHandle->ReadString();
		
		int count = gmkHandle->ReadDword();
		ReadRecursiveTree(rcEntry,count);
		resourceTree.push_back(rcEntry);
	}

	return true;
}

void Gmk::ReadRecursiveTree(ResourceTree* handle, int children) {
	ResourceTree* rcEntry;
	for(int i = 0; i < children; i++) {
		rcEntry = new ResourceTree;
		rcEntry->status = gmkHandle->ReadDword();
		rcEntry->group = gmkHandle->ReadDword();
		rcEntry->index = gmkHandle->ReadDword();
		rcEntry->name = gmkHandle->ReadString();

		int count = gmkHandle->ReadDword();
		ReadRecursiveTree(rcEntry,count);

		handle->contents.push_back(rcEntry);
	}
}

// Writing
bool Gmk::WriteSettings() {
	GmkStream* settingsHandle = new GmkStream;
	gmkSaveHandle->WriteDword(800);

	settingsHandle->WriteBool(settings.fullscreen);
	settingsHandle->WriteBool(settings.interpolate);
	settingsHandle->WriteBool(settings.dontDrawBorder);
	settingsHandle->WriteBool(settings.displayCursor);
	settingsHandle->WriteDword(settings.scaling);
	settingsHandle->WriteBool(settings.allowWindowResize);
	settingsHandle->WriteBool(settings.onTop);
	settingsHandle->WriteDword(settings.colorOutsideRoom);
	settingsHandle->WriteBool(settings.setResolution);
	settingsHandle->WriteDword(settings.colorDepth);
	settingsHandle->WriteDword(settings.resolution);
	settingsHandle->WriteDword(settings.frequency);
	settingsHandle->WriteBool(settings.dontShowButtons);
	settingsHandle->WriteBool(settings.vsync);
	settingsHandle->WriteBool(settings.disableScreen);
	settingsHandle->WriteBool(settings.letF4);
	settingsHandle->WriteBool(settings.letF1);
	settingsHandle->WriteBool(settings.letEsc);
	settingsHandle->WriteBool(settings.letF5);
	settingsHandle->WriteBool(settings.letF9);
	settingsHandle->WriteBool(settings.treatCloseAsEsc);
	settingsHandle->WriteDword(settings.priority);
	settingsHandle->WriteBool(settings.freeze);

	settingsHandle->WriteDword(settings.loadingBar);
	if (settings.loadingBar == 2) { // Custom loading bar
		bool biExists = settings.backData ? true : false;
		bool fiExists = settings.frontData ? true : false;

		settingsHandle->WriteBool(biExists);
		if (biExists) {
			settings.backData->Serialize(true);
			settingsHandle->MoveAll(settings.backData);
		}

		settingsHandle->WriteBool(fiExists);
		if (fiExists) {
			settings.frontData->Serialize(true);
			settingsHandle->MoveAll(settings.frontData);
		}
	}

	settingsHandle->WriteBool(settings.customLoadImage);
	if (settings.customLoadImage) {
		bool ciExists = settings.loadBar ? true : false;
		settingsHandle->WriteBool(ciExists);

		if (ciExists) {
			settings.loadBar->Serialize(true);
			settingsHandle->MoveAll(settings.loadBar);
		}
	}

	settingsHandle->WriteBool(settings.transparent);
	settingsHandle->WriteDword(settings.translucency);
	settingsHandle->WriteBool(settings.scaleProgressBar);

	settingsHandle->WriteDword(settings.iconData->GetLength());
	settingsHandle->MoveAll(settings.iconData);

	settingsHandle->WriteBool(settings.errorDisplay);
	settingsHandle->WriteBool(settings.errorLog);
	settingsHandle->WriteBool(settings.errorAbort);

	if (gmkVer == 800)
		settingsHandle->WriteDword(settings.treatAsZero);
	else if (gmkVer == 810)
		settingsHandle->WriteDword((settings.errorOnUninitialization << 1) | (settings.treatAsZero & 0x01));

	settingsHandle->WriteString(settings.author);
	settingsHandle->WriteString(settings.version);
	settingsHandle->WriteTimestamp();
	settingsHandle->WriteString(settings.information);
	
	settingsHandle->WriteDword(settings.majorVersion);
	settingsHandle->WriteDword(settings.minorVersion);
	settingsHandle->WriteDword(settings.releaseVersion);
	settingsHandle->WriteDword(settings.buildVersion);

	settingsHandle->WriteString(settings.company);
	settingsHandle->WriteString(settings.product);
	settingsHandle->WriteString(settings.copyright);
	settingsHandle->WriteString(settings.description);

	settingsHandle->WriteTimestamp();

	settingsHandle->Serialize(true);
	gmkSaveHandle->MoveAll(settingsHandle);

	return true;
}

bool Gmk::WriteTriggers() {
	GmkStream* triggerHandle;

	gmkSaveHandle->WriteDword(800);
	gmkSaveHandle->WriteDword(triggers.size());
	for(size_t i = 0; i < triggers.size(); i++) {
		triggerHandle = new GmkStream;

		triggerHandle->WriteBool(triggers[i] != 0);
		if (!triggers[i]) {
			triggerHandle->Serialize(true);
			gmkSaveHandle->MoveAll(triggerHandle);
			continue;
		}

		triggerHandle->WriteDword(800);
		triggerHandle->WriteString(triggers[i]->name);
		triggerHandle->WriteString(triggers[i]->condition);
		triggerHandle->WriteDword(triggers[i]->checkMoment);
		triggerHandle->WriteString(triggers[i]->constantName);

		triggerHandle->Serialize(true);
		gmkSaveHandle->MoveAll(triggerHandle);
	}

	gmkSaveHandle->WriteTimestamp();

	return true;
}

bool Gmk::WriteConstants() {
	gmkSaveHandle->WriteDword(800);
	gmkSaveHandle->WriteDword(constants.size());
	for(size_t i = 0; i < constants.size(); i++) {
		gmkSaveHandle->WriteString(constants[i]->name);
		gmkSaveHandle->WriteString(constants[i]->value);
	}

	gmkSaveHandle->WriteTimestamp();

	return true;
}

bool Gmk::WriteSounds() {
	GmkStream* soundHandle;

	gmkSaveHandle->WriteDword(800);
	gmkSaveHandle->WriteDword(sounds.size());
	for(size_t i = 0; i < sounds.size(); i++) {
		soundHandle = new GmkStream;

		soundHandle->WriteBool(sounds[i] != 0);
		if (!sounds[i]) {
			soundHandle->Serialize(true);
			gmkSaveHandle->MoveAll(soundHandle);
			continue;
		}

		soundHandle->WriteString(sounds[i]->name);
		soundHandle->WriteTimestamp();
		soundHandle->WriteDword(800);

		soundHandle->WriteDword(sounds[i]->kind);
		soundHandle->WriteString(sounds[i]->fileType);
		soundHandle->WriteString(sounds[i]->fileName);

		bool sExists = sounds[i]->data ? true : false;
		soundHandle->WriteBool(sExists);
		if (sExists) {
			sounds[i]->data->Serialize(false);
			soundHandle->MoveAll(sounds[i]->data);
		}

		soundHandle->WriteDword(sounds[i]->effects);
		soundHandle->WriteDouble(sounds[i]->volume);
		soundHandle->WriteDouble(sounds[i]->pan);
		soundHandle->WriteBool(sounds[i]->preload);

		soundHandle->Serialize(true);
		gmkSaveHandle->MoveAll(soundHandle);
	}

	return true;
}

bool Gmk::WriteSprites() {
	GmkStream* spriteHandle;

	gmkSaveHandle->WriteDword(800);
	gmkSaveHandle->WriteDword(sprites.size());
	for(size_t i = 0; i < sprites.size(); i++) {
		spriteHandle = new GmkStream;

		spriteHandle->WriteBool(sprites[i] != 0);
		if (!sprites[i]) {
			spriteHandle->Serialize(true);
			gmkSaveHandle->MoveAll(spriteHandle);
			continue;
		}

		spriteHandle->WriteString(sprites[i]->name);
		spriteHandle->WriteTimestamp();
		spriteHandle->WriteDword(800);

		spriteHandle->WriteDword(sprites[i]->originX);
		spriteHandle->WriteDword(sprites[i]->originY);

		spriteHandle->WriteDword(sprites[i]->images.size());
		for(size_t j = 0; j < sprites[i]->images.size(); j++) {
			spriteHandle->WriteDword(800);

			spriteHandle->WriteDword(sprites[i]->images[j]->width);
			spriteHandle->WriteDword(sprites[i]->images[j]->height);

			if (sprites[i]->images[j]->width != 0 && sprites[i]->images[j]->height != 0) {
				sprites[i]->images[j]->data->Serialize(false);
				spriteHandle->MoveAll(sprites[i]->images[j]->data);
			}
		}

		spriteHandle->WriteDword(sprites[i]->shape);
		spriteHandle->WriteDword(sprites[i]->alphaTolerance);
		spriteHandle->WriteBool(sprites[i]->seperateMask);
		spriteHandle->WriteDword(sprites[i]->boundingBox);
		spriteHandle->WriteDword(sprites[i]->left);
		spriteHandle->WriteDword(sprites[i]->right);
		spriteHandle->WriteDword(sprites[i]->bottom);
		spriteHandle->WriteDword(sprites[i]->top);

		spriteHandle->Serialize(true);
		gmkSaveHandle->MoveAll(spriteHandle);
	}

	return true;
}

bool Gmk::WriteBackgrounds() {
	GmkStream* bgHandle;

	gmkSaveHandle->WriteDword(800);
	gmkSaveHandle->WriteDword(backgrounds.size());
	for(size_t i = 0; i < backgrounds.size(); i++) {
		bgHandle = new GmkStream;

		bgHandle->WriteBool(backgrounds[i] != 0);
		if (!backgrounds[i]) {
			bgHandle->Serialize(true);
			gmkSaveHandle->MoveAll(bgHandle);
			continue;
		}

		bgHandle->WriteString(backgrounds[i]->name);
		bgHandle->WriteTimestamp();
		bgHandle->WriteDword(710);

		bgHandle->WriteBool(backgrounds[i]->tileset);
		bgHandle->WriteDword(backgrounds[i]->tileWidth);
		bgHandle->WriteDword(backgrounds[i]->tileHeight);
		bgHandle->WriteDword(backgrounds[i]->hOffset);
		bgHandle->WriteDword(backgrounds[i]->vOffset);
		bgHandle->WriteDword(backgrounds[i]->hSep);
		bgHandle->WriteDword(backgrounds[i]->vSep);

		bgHandle->WriteDword(800);
		bgHandle->WriteDword(backgrounds[i]->width);
		bgHandle->WriteDword(backgrounds[i]->height);
		
		if (backgrounds[i]->width > 0 && backgrounds[i]->height) {
			backgrounds[i]->data->Serialize(false);
			bgHandle->MoveAll(backgrounds[i]->data);
		}

		bgHandle->Serialize(true);
		gmkSaveHandle->MoveAll(bgHandle);
	}

	return true;
}

bool Gmk::WritePaths() {
	GmkStream* pathHandle;

	gmkSaveHandle->WriteDword(800);
	gmkSaveHandle->WriteDword(paths.size());
	for(size_t i = 0; i < paths.size(); i++) {
		pathHandle = new GmkStream;

		pathHandle->WriteBool(paths[i] != 0);
		if (!paths[i]) {
			pathHandle->Serialize(true);
			gmkSaveHandle->MoveAll(pathHandle);
			continue;
		}

		pathHandle->WriteString(paths[i]->name);
		pathHandle->WriteTimestamp();
		pathHandle->WriteDword(530);

		pathHandle->WriteDword(paths[i]->kind);
		pathHandle->WriteBool(paths[i]->closed);
		pathHandle->WriteDword(paths[i]->prec);
		pathHandle->WriteDword(paths[i]->roomIndexBG);
		pathHandle->WriteDword(paths[i]->snapX);
		pathHandle->WriteDword(paths[i]->snapY);

		pathHandle->WriteDword(paths[i]->points.size());
		for(size_t j = 0; j < paths[i]->points.size(); j++) {
			pathHandle->WriteDouble(paths[i]->points[j]->x);
			pathHandle->WriteDouble(paths[i]->points[j]->y);
			pathHandle->WriteDouble(paths[i]->points[j]->speed);
		}

		pathHandle->Serialize(true);
		gmkSaveHandle->MoveAll(pathHandle);
	}

	return true;
}

bool Gmk::WriteScripts() {
	GmkStream* scriptHandle;

	gmkSaveHandle->WriteDword(800);
	gmkSaveHandle->WriteDword(scripts.size());
	for(size_t i = 0; i < scripts.size(); i++) {
		scriptHandle = new GmkStream;

		scriptHandle->WriteBool(scripts[i] != 0);
		if (!scripts[i]) {
			scriptHandle->Serialize(true);
			gmkSaveHandle->MoveAll(scriptHandle);
			continue;
		}

		scriptHandle->WriteString(scripts[i]->name);
		scriptHandle->WriteTimestamp();
		scriptHandle->WriteDword(800);
		scriptHandle->WriteString(scripts[i]->value);

		scriptHandle->Serialize(true);
		gmkSaveHandle->MoveAll(scriptHandle);
	}

	return true;
}

bool Gmk::WriteFonts() {
	GmkStream* fontHandle;

	gmkSaveHandle->WriteDword(800);
	gmkSaveHandle->WriteDword(fonts.size());
	for(size_t i = 0; i < fonts.size(); i++) {
		fontHandle = new GmkStream;

		fontHandle->WriteBool(fonts[i] ? true : false);
		if (!fonts[i]) {
			fontHandle->Serialize(true);
			gmkSaveHandle->MoveAll(fontHandle);
			continue;
		}

		fontHandle->WriteString(fonts[i]->name);
		fontHandle->WriteTimestamp();
		fontHandle->WriteDword(800);

		fontHandle->WriteString(fonts[i]->fontName);
		fontHandle->WriteDword(fonts[i]->size);
		fontHandle->WriteBool(fonts[i]->bold);
		fontHandle->WriteBool(fonts[i]->italic);

		if (gmkVer == 800)
			fontHandle->WriteDword(fonts[i]->rangeBegin);
		else if (gmkVer == 810)
			fontHandle->WriteDword((fonts[i]->charset << 24) | (fonts[i]->aaLevel << 16) | fonts[i]->rangeBegin & 0x0000FFFF);

		fontHandle->WriteDword(fonts[i]->rangeEnd);

		fontHandle->Serialize(true);
		gmkSaveHandle->MoveAll(fontHandle);
	}

	return true;
}

bool Gmk::WriteAction(GmkStream* handle, ObjectAction* action) {
	handle->WriteDword(440);

	handle->WriteDword(action->libId);
	handle->WriteDword(action->actId);
	handle->WriteDword(action->kind);
	handle->WriteBool(action->mayBeRelative);
	handle->WriteBool(action->question);
	handle->WriteBool(action->appliesToSomething);
	handle->WriteDword(action->type);
	handle->WriteString(action->functionName);
	handle->WriteString(action->functionCode);

	handle->WriteDword(action->argumentsUsed);
	handle->WriteDword(8);
	for(int i = 0; i < 8; i++)
		handle->WriteDword(action->argumentKind[i]);

	handle->WriteDword(action->appliesToObject);
	handle->WriteBool(action->isRelative);

	handle->WriteDword(8);
	for(int i = 0; i < 8; i++)
		handle->WriteString(action->argumentValue[i]);

	handle->WriteBool(action->not);

	return true;
}

bool Gmk::WriteTimelines() {
	GmkStream* timelineHandle;

	gmkSaveHandle->WriteDword(800);
	gmkSaveHandle->WriteDword(timelines.size());
	for(size_t i = 0; i < timelines.size(); i++) {
		timelineHandle = new GmkStream;

		timelineHandle->WriteBool(timelines[i] != 0);
		if (!timelines[i]) {
			timelineHandle->Serialize(true);
			gmkSaveHandle->MoveAll(timelineHandle);
			continue;
		}

		timelineHandle->WriteString(timelines[i]->name);
		timelineHandle->WriteTimestamp();
		timelineHandle->WriteDword(500);

		timelineHandle->WriteDword(timelines[i]->moments.size());
		for(size_t j = 0; j < timelines[i]->moments.size(); j++) {
			timelineHandle->WriteDword(timelines[i]->moments[j]->moment);

			timelineHandle->WriteDword(400);
			timelineHandle->WriteDword(timelines[i]->moments[j]->actions.size());
			for(size_t k = 0; k < timelines[i]->moments[j]->actions.size(); k++)
				WriteAction(timelineHandle,timelines[i]->moments[j]->actions[k]);
		}

		timelineHandle->Serialize(true);
		gmkSaveHandle->MoveAll(timelineHandle);
	}

	return true;
}

bool Gmk::WriteObjects() {
	GmkStream* objectHandle;

	gmkSaveHandle->WriteDword(800);
	gmkSaveHandle->WriteDword(objects.size());
	for(size_t i = 0; i < objects.size(); i++) {
		objectHandle = new GmkStream;

		objectHandle->WriteBool(objects[i] ? true : false);
		if (!objects[i]) {
			objectHandle->Serialize(true);
			gmkSaveHandle->MoveAll(objectHandle);
			// delete objectHandle;?
			continue;
		}

		objectHandle->WriteString(objects[i]->name);
		objectHandle->WriteTimestamp();
		objectHandle->WriteDword(430);

		objectHandle->WriteDword(objects[i]->spriteIndex);
		objectHandle->WriteBool(objects[i]->solid);
		objectHandle->WriteBool(objects[i]->visible);
		objectHandle->WriteDword(objects[i]->depth);
		objectHandle->WriteBool(objects[i]->persistent);
		objectHandle->WriteDword(objects[i]->parentIndex);
		objectHandle->WriteDword(objects[i]->maskIndex);

		objectHandle->WriteDword(11);
		for(int ev = 0; ev < 12; ev++) {
			for(size_t evm = 0; evm < objects[i]->events.size(); evm++) {
				if (objects[i]->events[evm]->eventType == ev) {
					objectHandle->WriteDword(objects[i]->events[evm]->eventKind);

					objectHandle->WriteDword(400);
					objectHandle->WriteDword(objects[i]->events[evm]->actions.size());

					for(size_t j = 0; j < objects[i]->events[evm]->actions.size(); j++)
						if (!WriteAction(objectHandle,objects[i]->events[evm]->actions[j]))
							return false;
				}
			}
			
			objectHandle->WriteDword((unsigned int)-1);
		}

		objectHandle->Serialize(true);
		gmkSaveHandle->MoveAll(objectHandle);
	}

	return true;
}

bool Gmk::WriteRooms() {
	GmkStream* roomHandle;

	gmkSaveHandle->WriteDword(800);
	gmkSaveHandle->WriteDword(rooms.size());
	for(size_t i = 0; i < rooms.size(); i++) {
		roomHandle = new GmkStream;

		roomHandle->WriteBool(rooms[i] != 0);
		if (!rooms[i]) {
			roomHandle->Serialize(true);
			gmkSaveHandle->MoveAll(roomHandle);
			continue;
		}

		roomHandle->WriteString(rooms[i]->name);
		roomHandle->WriteTimestamp();
		roomHandle->WriteDword(541);

		roomHandle->WriteString(rooms[i]->caption);
		roomHandle->WriteDword(rooms[i]->width);
		roomHandle->WriteDword(rooms[i]->height);
		roomHandle->WriteDword(rooms[i]->snapX);
		roomHandle->WriteDword(rooms[i]->snapY);

		roomHandle->WriteBool(rooms[i]->isoGrid);
		roomHandle->WriteDword(rooms[i]->speed);
		roomHandle->WriteBool(rooms[i]->persistent);
		roomHandle->WriteDword(rooms[i]->bgColor);
		roomHandle->WriteBool(rooms[i]->bgColorDraw);
		roomHandle->WriteString(rooms[i]->creationCode);
		
		roomHandle->WriteDword(8);
		for(int j = 0; j < 8; j++) {
			roomHandle->WriteBool(rooms[i]->backgrounds[j]->visible);
			roomHandle->WriteBool(rooms[i]->backgrounds[j]->foreground);
			roomHandle->WriteDword(rooms[i]->backgrounds[j]->bgIndex);
			roomHandle->WriteDword(rooms[i]->backgrounds[j]->x);
			roomHandle->WriteDword(rooms[i]->backgrounds[j]->y);
			roomHandle->WriteBool(rooms[i]->backgrounds[j]->tileH);
			roomHandle->WriteBool(rooms[i]->backgrounds[j]->tileV);
			roomHandle->WriteDword(rooms[i]->backgrounds[j]->hSpeed);
			roomHandle->WriteDword(rooms[i]->backgrounds[j]->vSpeed);
			roomHandle->WriteBool(rooms[i]->backgrounds[j]->stretch);
		}

		roomHandle->WriteBool(rooms[i]->enableViews);
		roomHandle->WriteDword(8);
		for(int j = 0; j < 8; j++) {
			roomHandle->WriteBool(rooms[i]->views[j]->visible);
			roomHandle->WriteDword(rooms[i]->views[j]->viewX);
			roomHandle->WriteDword(rooms[i]->views[j]->viewY);
			roomHandle->WriteDword(rooms[i]->views[j]->viewW);
			roomHandle->WriteDword(rooms[i]->views[j]->viewH);
			roomHandle->WriteDword(rooms[i]->views[j]->portX);
			roomHandle->WriteDword(rooms[i]->views[j]->portY);
			roomHandle->WriteDword(rooms[i]->views[j]->portW);
			roomHandle->WriteDword(rooms[i]->views[j]->portH);
			roomHandle->WriteDword(rooms[i]->views[j]->vBorder);
			roomHandle->WriteDword(rooms[i]->views[j]->hBorder);
			roomHandle->WriteDword(rooms[i]->views[j]->vSpd);
			roomHandle->WriteDword(rooms[i]->views[j]->hSpd);
			roomHandle->WriteDword(rooms[i]->views[j]->objFollow);
		}

		roomHandle->WriteDword(rooms[i]->instances.size());
		for(size_t j = 0; j < rooms[i]->instances.size(); j++) {
			roomHandle->WriteDword(rooms[i]->instances[j]->x);
			roomHandle->WriteDword(rooms[i]->instances[j]->y);
			roomHandle->WriteDword(rooms[i]->instances[j]->objIndex);
			roomHandle->WriteDword(rooms[i]->instances[j]->id);
			roomHandle->WriteString(rooms[i]->instances[j]->creationCode);
			roomHandle->WriteBool(rooms[i]->instances[j]->locked);
		}

		roomHandle->WriteDword(rooms[i]->tiles.size());
		for(size_t j = 0; j < rooms[i]->tiles.size(); j++) {
			roomHandle->WriteDword(rooms[i]->tiles[j]->x);
			roomHandle->WriteDword(rooms[i]->tiles[j]->y);
			roomHandle->WriteDword(rooms[i]->tiles[j]->bgIndex);
			roomHandle->WriteDword(rooms[i]->tiles[j]->tileX);
			roomHandle->WriteDword(rooms[i]->tiles[j]->tileY);
			roomHandle->WriteDword(rooms[i]->tiles[j]->width);
			roomHandle->WriteDword(rooms[i]->tiles[j]->height);
			roomHandle->WriteDword(rooms[i]->tiles[j]->depth);
			roomHandle->WriteDword(rooms[i]->tiles[j]->id);
			roomHandle->WriteBool(rooms[i]->tiles[j]->locked);
		}

		roomHandle->WriteBool(rooms[i]->rememberRoomEditorInfo);
		roomHandle->WriteDword(rooms[i]->editorWidth);
		roomHandle->WriteDword(rooms[i]->editorHeight);
		roomHandle->WriteBool(rooms[i]->showGrid);
		roomHandle->WriteBool(rooms[i]->showObjects);
		roomHandle->WriteBool(rooms[i]->showTiles);
		roomHandle->WriteBool(rooms[i]->showBackgrounds);
		roomHandle->WriteBool(rooms[i]->showForegrounds);
		roomHandle->WriteBool(rooms[i]->showViews);
		roomHandle->WriteBool(rooms[i]->deleteUnderlyingObjects);
		roomHandle->WriteBool(rooms[i]->deleteUnderlyingTiles);
		roomHandle->WriteDword(rooms[i]->tab);
		roomHandle->WriteDword(rooms[i]->xPosScroll);
		roomHandle->WriteDword(rooms[i]->yPosScroll);

		roomHandle->Serialize(true);
		gmkSaveHandle->MoveAll(roomHandle);
	}

	return true;
}

bool Gmk::WriteIncludedFiles() {
	GmkStream* incHandle;

	gmkSaveHandle->WriteDword(800);
	gmkSaveHandle->WriteDword(includes.size());
	for(size_t i = 0; i < includes.size(); i++) {
		incHandle = new GmkStream;

		incHandle->WriteTimestamp();
		incHandle->WriteDword(800);

		incHandle->WriteString(includes[i]->fileName);
		incHandle->WriteString(includes[i]->filePath);
		incHandle->WriteBool(includes[i]->originalFile);
		incHandle->WriteDword(includes[i]->originalSize);
		incHandle->WriteBool(includes[i]->storedInGmk);
		if (includes[i]->storedInGmk) {
			includes[i]->data->Serialize(false);
			incHandle->MoveAll(includes[i]->data);
		}

		incHandle->WriteDword(includes[i]->exportFlags);
		incHandle->WriteString(includes[i]->folderExport);
		incHandle->WriteBool(includes[i]->overWrite);
		incHandle->WriteBool(includes[i]->freeMem);
		incHandle->WriteBool(includes[i]->removeGameEnd);

		incHandle->Serialize(true);
		gmkSaveHandle->MoveAll(incHandle);
	}

	return true;
}

bool Gmk::WriteGameInformation() {
	GmkStream* giHandle = new GmkStream;

	giHandle->WriteDword(gameInfo.bgColor);
	giHandle->WriteBool(gameInfo.seperateWindow);
	giHandle->WriteString(gameInfo.caption);
	giHandle->WriteDword(gameInfo.left);
	giHandle->WriteDword(gameInfo.top);
	giHandle->WriteDword(gameInfo.width);
	giHandle->WriteDword(gameInfo.height);
	giHandle->WriteBool(gameInfo.showWindowBorder);
	giHandle->WriteBool(gameInfo.allowResize);
	giHandle->WriteBool(gameInfo.stayOnTop);
	giHandle->WriteBool(gameInfo.freezeGame);
	giHandle->WriteTimestamp();

	giHandle->WriteString(gameInfo.infoRtf);

	gmkSaveHandle->WriteDword(800);
	giHandle->Serialize(true);
	gmkSaveHandle->MoveAll(giHandle);

	return true;
}

void Gmk::WriteRecursiveTree(ResourceTree* handle, int children) {
	// Yes, I know, I feel the stack's pain

	for(int i = 0; i < children; i++) {
		gmkSaveHandle->WriteDword(handle->contents[i]->status);
		gmkSaveHandle->WriteDword(handle->contents[i]->group);
		gmkSaveHandle->WriteDword(handle->contents[i]->index);
		gmkSaveHandle->WriteString(handle->contents[i]->name);

		gmkSaveHandle->WriteDword(handle->contents[i]->contents.size());
		WriteRecursiveTree(handle->contents[i],handle->contents[i]->contents.size());
	}
}

bool Gmk::WriteResourceTree() {
	for(int i = 0; i < 12; i++) {
		gmkSaveHandle->WriteDword(resourceTree[i]->status);
		gmkSaveHandle->WriteDword(resourceTree[i]->group);
		gmkSaveHandle->WriteDword(resourceTree[i]->index);
		gmkSaveHandle->WriteString(resourceTree[i]->name);

		gmkSaveHandle->WriteDword(resourceTree[i]->contents.size());
		WriteRecursiveTree(resourceTree[i],resourceTree[i]->contents.size());
	}

	return true;
}

void Gmk::Defragment() {
	Log("Cannot defragment!",true);
}
