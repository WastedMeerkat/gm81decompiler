/*
 *	gm80.hpp
 *	Game Maker 8.0 EXE Support
 */

#ifndef __GM80_HPP
#define __GM80_HPP

#include <iostream>
#include <string>
#include <gmk.hpp>

class Gm80 {
private:

public:
	bool FindGameData(GmkStream* exeHandle);
};

#endif
