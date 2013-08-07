/* 
 *	gmk.hpp
 *	GMK parser
 */

#ifndef __GMK_HPP
#define __GMK_HPP

#include <iostream>
#include <vector>
#include <string>
#include <time.h>

// GMK Constants
#define GMK_MAGIC			1234321
#define GMK_VERSION			800
#define GMK_MAX_ID			100000000

// GMK Stream Constants
#define FMODE_BINARY		0
#define FMODE_TEXT			1

// Resource tree constants
#define RCT_GROUP_SPRITES		2
#define RCT_GROUP_SOUNDS		3
#define RCT_GROUP_BACKGROUNDS	6
#define RCT_GROUP_PATHS			8
#define RCT_GROUP_SCRIPTS		7
#define RCT_GROUP_FONTS			9
#define RCT_GROUP_TIMELINES		12
#define RCT_GROUP_OBJECTS		1
#define RCT_GROUP_ROOMS			4

#define RCT_ID_SPRITES			0
#define RCT_ID_SOUNDS			1
#define RCT_ID_BACKGROUNDS		2
#define RCT_ID_PATHS			3
#define RCT_ID_SCRIPTS			4
#define RCT_ID_FONTS			5
#define RCT_ID_TIMELINES		6
#define RCT_ID_OBJECTS			7
#define RCT_ID_ROOMS			8

class GmkStream;

// Structures
typedef struct _GameSettings {
	bool			fullscreen, interpolate, dontDrawBorder, displayCursor, allowWindowResize, onTop, setResolution;
	bool			dontShowButtons, vsync, disableScreen, letF4, letF1, letEsc, letF5, letF9, treatCloseAsEsc;
	bool			freeze, transparent, scaleProgressBar, customLoadImage;
	bool			errorDisplay, errorLog, errorAbort;
	int				treatAsZero, errorOnUninitialization, translucency, majorVersion, minorVersion, releaseVersion, buildVersion;
	int				priority, loadingBar;
	int				scaling, colorOutsideRoom, colorDepth, resolution, frequency;
	time_t			lastChanged, lastSettingsChanged;
	std::string		author, version, information, company, product, copyright, description;
	GmkStream		*backData, *frontData, *loadBar, *iconData;
} GameSettings;

typedef struct _Constant {
	std::string		name, value;
} Constant;

typedef struct _Trigger {
	std::string		name, condition, constantName;
	int				checkMoment;
} Trigger;

typedef struct _Script {
	std::string		name, value;
	time_t			lastChanged;
} Script;

typedef struct _SubImage {
	int				width, height;
	GmkStream*		data;
} SubImage;

typedef struct _Sprite {
	std::string		name;
	bool			seperateMask;
	time_t			lastChanged;
	int				originX, originY, left, right, top, bottom;
	int				shape, alphaTolerance, boundingBox;
	std::vector<SubImage*>	images;
} Sprite;

typedef struct _Sound {
	std::string			name, fileType, fileName;
	time_t				lastChanged;
	double				volume, pan;
	int					kind, effects;
	bool				preload;
	GmkStream*			data;
} Sound;

typedef struct _Background {
	std::string			name;
	time_t				lastChanged;
	bool				tileset;
	int					width, height, tileWidth, tileHeight, hOffset, vOffset, hSep, vSep;
	GmkStream*			data;
} Background;

typedef struct _PathPoint {
	double				x, y, speed;
} PathPoint;

typedef struct _Path {
	std::string			name;
	time_t				lastChanged;
	int					kind;
	bool				closed;
	int					prec, roomIndexBG, snapX, snapY;
	std::vector<PathPoint*>	points;
} Path;

typedef struct _Font {
	std::string			name, fontName;
	time_t				lastChanged;
	int					rangeBegin, rangeEnd, aaLevel, charset;
	int					size;
	bool				bold, italic;
} Font;

typedef struct _RoomBackground {
	bool				visible, foreground, stretch, tileH, tileV;
	int					x, y, hSpeed, vSpeed, bgIndex;
} RoomBackground;

typedef struct _RoomView {
	bool				visible;
	int					viewX, viewY, viewW, viewH, portX, portY, portW, portH, vBorder, hBorder, vSpd, hSpd, objFollow;
} RoomView;

typedef struct _RoomInstance {
	int					x, y, objIndex, id;
	bool				locked;
	std::string			creationCode;
} RoomInstance;

typedef struct _RoomTile {
	int					x, y, bgIndex, tileX, tileY, width, height, depth, id;
	bool				locked;
} RoomTile;

typedef struct _Room {
	std::string			name, caption, creationCode;
	time_t				lastChanged;
	int					speed;
	int					width, height, snapX, snapY, bgColor, editorWidth, editorHeight, xPosScroll, yPosScroll;
	bool				isoGrid, persistent, bgColorDraw, enableViews, rememberRoomEditorInfo, showGrid, showObjects, showTiles, showBackgrounds, showForegrounds, showViews, deleteUnderlyingObjects, deleteUnderlyingTiles;
	int					tab;
	std::vector<RoomBackground*>	backgrounds;
	std::vector<RoomView*>			views;
	std::vector<RoomInstance*>		instances;
	std::vector<RoomTile*>			tiles;
} Room;

typedef struct _ObjectAction {
	std::string			functionName, functionCode, argumentValue[8];
	int					libId, actId, kind, type, argumentsUsed, argumentKind[8], appliesToObject;
	bool				isRelative, appliesToSomething, question, mayBeRelative, not;
} ObjectAction;

typedef struct _ObjectEvent {
	int					eventType, eventKind;
	std::vector<ObjectAction*>	actions;
} ObjectEvent;

typedef struct _Object {
	std::string			name;
	time_t				lastChanged;
	int					spriteIndex, parentIndex, maskIndex, depth, objId;
	bool				solid, visible, persistent;
	std::vector<ObjectEvent*>	events;
} Object;

typedef struct _TimelineMoment {
	std::vector<ObjectAction*>	actions;
	int					moment;
} TimelineMoment;

typedef struct _Timeline {
	std::string			name;
	time_t				lastChanged;
	std::vector<TimelineMoment*>	moments;
} Timeline;

typedef struct _IncludedFile {
	std::string			fileName, filePath, folderExport;
	time_t				lastChanged;
	int					exportFlags, originalSize;
	bool				originalFile, storedInGmk, overWrite, freeMem, removeGameEnd;
	GmkStream*			data;
} IncludeFile;

typedef struct _ResourceTree {
	std::string			name;
	int					status, group;
	int					index;
	std::vector<_ResourceTree*>	contents;
} ResourceTree;

typedef struct _GameInfo {
	std::string			caption, infoRtf;
	time_t				lastChanged;
	bool				seperateWindow, showWindowBorder, allowResize, stayOnTop, freezeGame;
	int					left, top, width, height, bgColor;
} GameInfo;

// GMK Stream class
class GmkStream {
private:
	bool verbose;

	// Support
	void CheckStreamForWriting(size_t len);
	void CheckStreamForReading(size_t len);

	void Copy(GmkStream* src);
	void Move(GmkStream* src);

	// Compression
	unsigned char* DeflateStream(unsigned char* buffer, size_t iLen, size_t *oLen);
	unsigned char* InflateStream(unsigned char* buffer, size_t iLen, size_t *oLen);

public:
	unsigned char* iBuffer;
	size_t iPosition, iLength;

	// Constructor/Deconstructor
	GmkStream() {
		iBuffer = new unsigned char[1];
		iLength = iPosition = 0;
		verbose = true;
	}
	
	~GmkStream() {
		delete[] iBuffer;
	}

	// Verbosity
	inline void SetVerbose(bool v) { verbose = v; }

	// Buffer
	inline unsigned char* GetBuffer() { return iBuffer; }
	inline size_t GetPosition() { return iPosition; }
	inline size_t GetLength() { return iLength; }

	inline void SetPosition(size_t p) { iPosition = p; }
	inline void SkipDwords(size_t count) { iPosition += count * 4; }

	void MoveAll(GmkStream* src);

	// File
	bool Load(const std::string& filename);
	bool Save(const std::string& filename, int mode);

	// Reading
	void ReadData(unsigned char* buffer, size_t len);
	bool ReadBool();
	unsigned char ReadByte();
	unsigned short ReadWord();
	unsigned int ReadDword();
	double ReadDouble();
	std::string ReadString();
	time_t ReadTimestamp();

	// Writing
	void WriteData(unsigned char* buffer, size_t len);
	void WriteBool(bool value);
	void WriteByte(unsigned char value);
	void WriteWord(unsigned short value);
	void WriteDword(unsigned int value);
	void WriteDouble(double value);
	void WriteString(const std::string& value);
	void WriteTimestamp();

	// Compression
	bool Deflate();
	bool Inflate();

	// Serialization
	bool Deserialize(GmkStream* dest, bool decompress);
	bool Serialize(bool compress);
};

// GMK Class
class Gmk {
private:
	GmkStream *gmkHandle, *gmkSaveHandle;
	bool verbose;

	void Log(const std::string& value, bool error);

	// Reading
	bool ReadSettings();
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
	bool ReadIncludedFiles();
	bool ReadGameInformation();
	bool ReadResourceTree();

	bool ReadAction(GmkStream* handle, ObjectAction* action);
	void ReadRecursiveTree(ResourceTree* handle, int children);

	// Writing
	bool WriteSettings();
	bool WriteTriggers();
	bool WriteConstants();
	bool WriteSounds();
	bool WriteSprites();
	bool WriteBackgrounds();
	bool WritePaths();
	bool WriteScripts();
	bool WriteFonts();
	bool WriteTimelines();
	bool WriteObjects();
	bool WriteRooms();
	bool WriteIncludedFiles();
	bool WriteGameInformation();
	bool WriteResourceTree();

	bool WriteAction(GmkStream* handle, ObjectAction* action);
	void WriteRecursiveTree(ResourceTree* handle, int children);

public:
	// Information
	unsigned int				gmkVer, gameId, lastTile, lastInstance;
	time_t						triggersChanged, constantsChanged;
	GameSettings				settings;
	GameInfo					gameInfo;
	std::vector<Constant*>		constants;
	std::vector<Sprite*>		sprites;
	std::vector<Sound*>			sounds;
	std::vector<Background*>	backgrounds;
	std::vector<Path*>			paths;
	std::vector<Script*>		scripts;
	std::vector<Font*>			fonts;
	std::vector<Timeline*>		timelines;
	std::vector<Object*>		objects;
	std::vector<Room*>			rooms;
	std::vector<ResourceTree*>	resourceTree;

	std::vector<IncludeFile*>	includes;
	std::vector<Trigger*>		triggers;
	std::vector<std::string>	packages;
	std::vector<std::string>	libCreationCode;
	std::vector<int>			roomExecutionOrder;

	Gmk() {
		gmkHandle = new GmkStream;
		gmkSaveHandle = new GmkStream;
	}

	Gmk::~Gmk() {
		// Clean up the obvious
		delete gmkHandle;
		delete gmkSaveHandle;
	}

	// Files
	bool Load(const std::string& filename);
	bool Save(const std::string& filename, unsigned int ver);

	void SetDefaults();
	void Defragment();

	void SetVerbose(bool v) { verbose = v; }
};

// exception for GmkParser
class GmkParserException {
private:
	std::string err;
public:
	GmkParserException(const std::string& e) : err(e) { };
	const char* what() { return err.c_str(); }
};

#endif
