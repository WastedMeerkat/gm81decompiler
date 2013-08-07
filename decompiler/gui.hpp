/*
 *	gui.hpp
 *	Windows' Graphical User Interface
 */

#ifndef __GUI_HPP
#define __GUI_HPP

#include <iostream>
#include <Windows.h>
#include <CommCtrl.h>

class Gui {
private:
	HINSTANCE winInstance;
	WNDCLASS windowClass;
	HWND winHandle, filepathInput, filepathButton, decompileButton, versionDropdown, infoLabel, progressBar;

public:
	friend LRESULT CALLBACK OnEvent(HWND handle, DWORD message, WPARAM wParam, LPARAM lParam);
	friend BOOL CALLBACK SetChildFont(HWND hWnd, LPARAM lParam);

	bool CreateGui(HINSTANCE hInstance);
	bool HandleGui();
};

#endif
