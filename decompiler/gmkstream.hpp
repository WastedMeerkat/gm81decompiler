/*
 *	gmkstream.hpp
 *	GMK Stream
 */

#if 0
#ifndef __GMK_STREAM_HPP
#define __GMK_STREAM_HPP

#include <iostream>
#include <string>
#include <time.h>

// Constants
#define FMODE_BINARY			0
#define FMODE_TEXT				1

// GMK Stream class
class GmkStream {
private:
	// Support
	void CheckStreamForWriting(size_t len);
	void CheckStreamForReading(size_t len);

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
	}
	
	~GmkStream() {
		if (iBuffer)
			delete[] iBuffer;
	}

	// Buffer
	inline unsigned char* GetBuffer() { return iBuffer; }
	inline size_t GetPosition() { return iPosition; }
	inline size_t GetLength() { return iLength; }

	inline void SetPosition(size_t p) { iPosition = p; }
	inline void SkipDwords(size_t count) { iPosition += count * 4; }

	void MoveAll(GmkStream* src);
	void Copy(GmkStream* src);

	// File
	bool Load(std::string& filename);
	bool Save(std::string& filename, int mode);

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
	void WriteString(std::string& value);
	void WriteTimestamp();

	// Compression
	bool Deflate();
	bool Inflate();

	// Serialization
	bool Deserialize(GmkStream* dest, bool decompress);
	bool Serialize(bool compress);
};

#endif
#endif
