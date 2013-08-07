/*
 *	gui.cpp
 *	Windows' Graphical User Interface
 */

#include <iostream>
#include "gui.hpp"

LRESULT CALLBACK OnEvent(HWND handle, DWORD message, WPARAM wParam, LPARAM lParam) {
	Gui* gui;

	if (message == WM_CREATE) {
		gui = (Gui*)((LPCREATESTRUCT)lParam)->lpCreateParams;
		SetWindowLongPtr(handle,GWL_USERDATA,(LONG_PTR)gui);
	} else {
		gui = (Gui*)GetWindowLongPtr(handle,GWL_USERDATA);
		if (!gui)
			return DefWindowProc(handle,message,wParam,lParam);
	}

	switch(message) {
		case WM_CLOSE:
			PostQuitMessage(0);
			return 0;
	}

	return DefWindowProc(handle,message,wParam,lParam);
}

BOOL CALLBACK SetChildFont(HWND hWnd, LPARAM lParam) {
	LOGFONT lf;
	GetObject(GetStockObject(DEFAULT_GUI_FONT),sizeof(LOGFONT),&lf);
	HFONT hFont = CreateFont(lf.lfHeight,lf.lfWidth,lf.lfEscapement,lf.lfOrientation,lf.lfWeight,lf.lfItalic,lf.lfUnderline,lf.lfStrikeOut,lf.lfCharSet,lf.lfOutPrecision,lf.lfClipPrecision,lf.lfQuality,lf.lfPitchAndFamily,lf.lfFaceName); 
	SendMessage(hWnd,WM_SETFONT,(WPARAM)hFont,true);

	return TRUE;
}

bool Gui::CreateGui(HINSTANCE hInstance) {
	// Register window class
	winInstance = hInstance;

	windowClass.style = 0;
	windowClass.lpfnWndProc = (WNDPROC)&OnEvent;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = hInstance;
	windowClass.hIcon = LoadIcon(GetModuleHandle(NULL),MAKEINTRESOURCE(1));
	windowClass.hCursor	= LoadCursor(NULL,IDC_ARROW);
	windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	windowClass.lpszMenuName = NULL;
	windowClass.lpszClassName = "Gm8XDecompiler";

	RegisterClass(&windowClass);

	// Create window
	winHandle = CreateWindow("Gm8XDecompiler","GameMaker 8.x Decompiler",WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,0,0,434,197,NULL,NULL,hInstance,this);
	MoveWindow(winHandle,(GetSystemMetrics(SM_CXSCREEN) / 2) - (434 / 2),(GetSystemMetrics(SM_CYSCREEN) / 2) - (197 / 2),434 - 13,197 - 13,false);

	// Initialize common controls
	INITCOMMONCONTROLSEX InitCtrlEx;
	InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
	InitCtrlEx.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&InitCtrlEx);

	// Create controls
	HWND container = winHandle;//CreateWindowEx(WS_EX_TRANSPARENT,"static","",WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,0,0,434 - 13, 197 - 13,winHandle,NULL,winInstance,0);//winHandle;
	filepathInput = CreateWindowEx(WS_EX_CLIENTEDGE,"edit","",WS_CHILD | WS_VISIBLE,13,13,312,20,container,NULL,winInstance,0);
	filepathButton = CreateWindow("button","Browse",WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE,331,11,75,23,container,NULL,winInstance,0);
	decompileButton = CreateWindow("Button","Decompile",WS_CHILD | BS_USERBUTTON | BS_PUSHBUTTON | WS_VISIBLE,331,37,75,23,container,NULL,winInstance,0);
	versionDropdown = CreateWindow("combobox",NULL,WS_CHILD | WS_VISIBLE | LBS_STANDARD,12,39,121,21,container,NULL,winInstance,0);
	infoLabel = CreateWindow("static",NULL,WS_CHILD | WS_VISIBLE,13,105,434 - 13 - 13,13,container,NULL,winInstance,0);
	progressBar = CreateWindow(PROGRESS_CLASS,NULL,WS_CHILD | WS_VISIBLE,13,124,393,23,container,NULL,winInstance,0);

	const char* versionValues[] = { "Auto Select", "GameMaker 8.0", "GameMaker 8.1" };

	for(int i = 0; i < 3; i++)
		SendMessage(versionDropdown,CB_ADDSTRING,0,(LPARAM)versionValues[i]);

	SendMessage(versionDropdown,CB_SETCURSEL,0,0);

	SetWindowText(infoLabel,"Decrypting game data...");

	// Make it prettier
	EnumChildWindows(winHandle,SetChildFont,0);

	ShowWindow(winHandle,true);

	return true;
}

bool Gui::HandleGui() {
	MSG message;

	if (PeekMessage(&message,0,0,0,PM_REMOVE)) {
		TranslateMessage(&message);
		DispatchMessage(&message);

		// If we're quiting
		if (message.message == WM_QUIT)
			return false;
	}

	return true;
}
