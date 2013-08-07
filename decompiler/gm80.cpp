/*
 *	gm80.cpp
 *	Game Maker 8.0 EXE Support
 */

#include <iostream>
#include "gm80.hpp"

bool Gm80::FindGameData(GmkStream* exeHandle) {
	exeHandle->SetPosition(0x1E8480);

	return exeHandle->ReadDword() == GMK_MAGIC;
}
