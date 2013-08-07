/*
 *	main.cpp
 *	GameMaker 8.0+ Decompiler
 */

#include <iostream>
#include <Windows.h>
#include "gui.hpp"
#include "exe.hpp"
#include "gex.hpp"

int main(int argc, char* argv[]) {
	// Load EXE
	Gmk* gmkHandle = new Gmk;
	GmExe* gmExe = new GmExe;

	gmkHandle->SetDefaults();

	if (!gmExe->Load(argv[1],gmkHandle,0))
		return 0;

	// Save GMK
	std::cout << "Writing GMK..." << std::endl;

	if (!gmkHandle->Save(std::string(argv[1]) + ((gmExe->GetExeVersion() == 810) ? ".gm81" : ".gmk"),gmExe->GetExeVersion())) {
		std::cout << "Failed to save GMK!" << std::endl;
		return 0;
	}

	std::cout << "Great success!" << std::endl;

	delete gmExe;
	delete gmkHandle;

	return 0;
}

/*INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
	Gui* gui = new Gui();

	if (!gui->CreateGui(hInstance)) {
		MessageBox(NULL,"Failed to create window!","Error",MB_ICONERROR | MB_OK);
		return 0;
	}

	while(gui->HandleGui())
		Sleep(1);

	delete gui;

	return 0;
}*/
