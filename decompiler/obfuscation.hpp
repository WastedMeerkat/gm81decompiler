/*
 *	obfuscation.hpp
 *	GameMaker Obfuscation
 */

#ifndef __OBFUSCATION_HPP
#define __OBFUSCATION_HPP

#include <iostream>

class GmObfuscation {
private:
	int swapTable[2][256];
	int seed;

	void GenerateTable();

public:
	GmObfuscation(int _seed) : seed(_seed) { GenerateTable(); }

	void SetSeed(int _seed) { seed = _seed; GenerateTable(); }
	const int GetSeed() const { return seed; }

	const unsigned char GetByte(size_t i) const { return swapTable[1][i]; }
};

#endif
