/*
 *	gmkstream.cpp
 *	GMK Stream
 */

#if 0
#include <iostream>
#include <fstream>
#include "gmkstream.hpp"
#include "zlib.h"

// Support
void GmkStream::CheckStreamForWriting(size_t len) {
	if (iPosition + len < iLength)
		return;

	iLength += len;
	iBuffer = (unsigned char*)realloc(iBuffer,iLength);
}

void GmkStream::CheckStreamForReading(size_t len) {
	if (iPosition + len > iLength) {
		std::cerr << "[Warning] Unforseen end of stream while reading @ " << iPosition + len << std::endl;
		exit(0);
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
	if (!iBuffer)
		delete[] iBuffer;

	iBuffer = new unsigned char[len];
	iLength = len;
	SetPosition(0);

	memcpy(iBuffer,src->iBuffer + src->iPosition,len);
	src->iLength += len;
}

void GmkStream::Copy(GmkStream* src) {
	CheckStreamForWriting(src->GetLength());

	memcpy(iBuffer + iPosition,src->iBuffer + src->iPosition,src->iLength);
	src->iPosition += src->iLength - src->iPosition;
}

// Files
bool GmkStream::Load(std::string& filename) {
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

bool GmkStream::Save(std::string& filename, int mode) {
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
	std::string res;

	CheckStreamForReading(len);
	res.reserve(len);

	memcpy((void*)res.data(),iBuffer + iPosition,len);
	iPosition += len;

	res[len] = 0;

	return res;
}

time_t GmkStream::ReadTimestamp() {
	return (time_t)(int)ReadDouble();
}

// Writing
void GmkStream::WriteData(unsigned char* buffer, size_t len) {
	CheckStreamForWriting(len);

	memcpy(iBuffer + iPosition,buffer,len);
	iPosition += len;
}

void GmkStream::WriteBool(bool value) {
	WriteDword((int)(value == 1));
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

void GmkStream::WriteString(std::string& value) {
	WriteDword(value.length());
	CheckStreamForWriting(value.length());

	memcpy(iBuffer + iPosition,value.c_str(),value.length());
	iPosition += value.length();
}

void GmkStream::WriteTimestamp() {
	WriteDouble(0);
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

	if(!retval)
		std::cout << std::endl << "[Warning] Unfinished compression?";
	else if (retval != 1) {
		std::cerr << std::endl << "[Error  ] Compression error " << retval;
		exit(0);
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
	iLength += tmp->GetLength() - tmp->iPosition;

	delete tmp;

	return true;
}

// Serialization
bool GmkStream::Deserialize(GmkStream* dest, bool decompress) {
	unsigned int len = ReadDword();
	CheckStreamForReading(len - 1);

	if (dest->iBuffer)
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

	if (iBuffer)
		delete[] iBuffer;

	iBuffer = tmp->iBuffer;
	iPosition = tmp->GetPosition();
	iLength = tmp->GetLength();

	return true;
}

#endif
