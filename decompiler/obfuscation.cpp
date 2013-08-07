/*
 *	obfuscation.cpp
 *	GameMaker Obfuscation
 *	Credit to IsmAvatar for GMKrypt documentation
 */

#include <iostream>
#include "obfuscation.hpp"

void GmObfuscation::GenerateTable() {
	int a, b, j, t;

	a = (seed % 250) + 6;
	b = seed / 250;

	for(int i = 0; i < 256; i++)
		swapTable[0][i] = i;

	for(int i = 1; i < 10001; i++) {
		j = 1 + ((i * a + b) % 254);
		t = swapTable[0][j];
		swapTable[0][j] = swapTable[0][j + 1];
		swapTable[0][j + 1] = t;
	}

	for(int i = 0; i < 256; i++)
		swapTable[1][swapTable[0][i]] = i;
}
