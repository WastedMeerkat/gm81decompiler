/*
 *	gm81.cpp
 *	GameMaker8.1 EXE Support
 */

#include <iostream>
#include "gm81.hpp"

Gm81::Gm81() {
	// Generate the CRC table
	const unsigned long crcPolynomial = 0x04C11DB7;

	for(int i = 0; i < 256; i++) {
		crcTable[i] = Crc32Reflect(i,8) << 24;
		for(int j = 0; j < 8; j++)
			crcTable[i] = (crcTable[i] << 1) ^ (crcTable[i] & (1 << 31) ? crcPolynomial : 0);

		crcTable[i] = Crc32Reflect(crcTable[i],32);
	}
}

unsigned long Gm81::Crc32Reflect(unsigned long value, char c) {
	unsigned long rValue = 0;

	for(int i = 1; i < c + 1; i++) {
		if ((value & 0x01))
			rValue |= 1 << (c - i);

		value >>= 1;
	}

	return rValue;
}

unsigned int Gm81::Crc32(const void* tmpBuffer, size_t length) {
	unsigned char* buffer = (unsigned char*)tmpBuffer;
	unsigned long result = 0xFFFFFFFF;

	while(length--)
		result = (result >> 8) ^ crcTable[(result & 0xFF) ^ *buffer++];

	// This should be xor'd but YYG forgot it
    return result;
}

unsigned int Gm81::ReadVersion(GmkStream* exeHandle) {
	unsigned int ver = exeHandle->ReadDword();
	size_t pos = exeHandle->iPosition - 4;
	pos -= GARBAGE_OFFSET + 0x11;
	if (!pos)
		pos += 3;

	// Used inline asm to prevent signed/unsigned issues with shifting
	_asm sar dword ptr [pos],  2
	
	return ver ^ pos;
}

unsigned int Gm81::GetXorMask() {
	seed1 = (0xFFFF & seed1) * PSEUDO_SEED1 + (seed1 >> 16);
	seed2 = (0xFFFF & seed2) * PSEUDO_SEED2 + (seed2 >> 16);

	return (seed1 << 16) + (seed2 & 0xFFFF);
}

bool Gm81::FindGameData(GmkStream* exeHandle) {
	// Note: Magic doesn't need to be byte aligned according to the runner.
	//       it always appears to be though,  but YYG could easily -1 this and break it.
	exeHandle->SetPosition(GARBAGE_OFFSET);
	for(size_t i = 0; i < 1024; i++) { // Garbage table shouldn't be larger than 1024 dwords
		if ((exeHandle->ReadDword() & 0xFF00FF00) == 0xF7000000) {
			if ((exeHandle->ReadDword() & 0x00FF00FF) == 0x00140067)
				return true;
		}
	}

	return false;
}

bool Gm81::Decrypt(GmkStream* exeHandle) {
	GmkStream* stream = new GmkStream;

	char* tmpBuffer = new char[64];
	char* buffer = new char[64];

	// Convert hash key into UTF-16,  not sure if there's a better way
	sprintf(tmpBuffer,"_MJD%d#RWK",exeHandle->ReadDword());
	for(size_t i = 0; i < strlen(tmpBuffer); i++) {
		buffer[i * 2] = tmpBuffer[i];
		buffer[(i * 2) + 1] = 0;
	}

	// Get seed1 and seed2
	seed2 = Crc32(buffer,strlen(tmpBuffer) * 2);
	seed1 = exeHandle->ReadDword();

	// Check if this is version 8.1
	if (ReadVersion(exeHandle) != 810) {
		std::cerr << "[Error  ] Invalid version, expected 810!" << std::endl;
		return false;
	}

	/* Note: This doesn't decrypt the last dword properly
	         Since the last few bytes of every game is random trash anyway, it doesn't matter
			 of course, it's still an issue that needs to be addressed later. */

	// Decrypt up to encrypted data offset (<261 bytes)
	size_t encryptionOffset = exeHandle->iPosition + (seed2 & 0xFF) + 6;
	while(exeHandle->iPosition < encryptionOffset)
		stream->WriteByte(exeHandle->ReadByte());

	// Decrypt everything (read in dwords)
	while(exeHandle->iPosition < exeHandle->iLength) {
		if (exeHandle->iLength - exeHandle->iPosition < 4) {
			stream->WriteDword(0);
			break;
		}

		stream->WriteDword(GetXorMask() ^ exeHandle->ReadDword());
	}

	// Reset position
	stream->iPosition = 0;

	// Copy stream handle over
	delete[] exeHandle->iBuffer;
	*exeHandle = *stream;

	// Clean up
	delete[] buffer;
	delete[] tmpBuffer;

	return true;
}
