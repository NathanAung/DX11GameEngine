#include <Windows.h>

// Entry point for a Windows application
// CALLBACK tells the compiler to use the stdcall calling convention which is required for Windows API functions
int CALLBACK WinMain(
	HINSTANCE hInstance,		// Handle to current instance (uniquely identifies the application)
	HINSTANCE hPrevInstance,	// Handle to previous instance (always NULL in Win32)
	LPSTR     lpCmdLine,		// Command line arguments as a single string
	int       nCmdShow)			// Show state for the window (e.g., maximize, minimize)
{
	const wchar_t* pClassName = L"DX11EWindowClass";
	const wchar_t* pWindowName = L"DirectX 11 Game Engine";
	// register window class
	WNDCLASSEX wc = { 0 };				// WNDCLASSEX structure to hold information for registering the window class
	wc.cbSize = sizeof(wc);				// size of the structure
	wc.style = CS_OWNDC;				// class style: own device context for each window
	wc.lpfnWndProc = DefWindowProc;		// pointer to the window procedure function that will handle events for windows of this class, using the default procedure
	wc.cbClsExtra = 0;					// no extra bytes after the window class structure
	wc.cbWndExtra = 0;					// no extra bytes after the window instance
	wc.hInstance = hInstance;			// handle to the application instance
	wc.hIcon = nullptr;					// no icon
	wc.hCursor = nullptr;				// no cursor
	wc.hbrBackground = nullptr;			// no background brush
	wc.lpszMenuName = nullptr;			// no menu
	wc.lpszClassName = pClassName;		// name of the window class
	wc.hIconSm = nullptr;				// no small icon

	// register the window class with the operating system
	RegisterClassEx(&wc);

	// create window instance
	HWND hWnd = CreateWindowEx(
		0,												// no extended window styles
		pClassName,										// name of the window class
		pWindowName,									// window title
		WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU,		// window style: caption, minimize box, and system menu
		200,											// initial horizontal position of the window
		200,											// initial vertical position of the window
		640,											// width of the window
		480,											// height of the window
		nullptr,										// no parent window
		nullptr,										// no menu
		hInstance,										// handle to the application instance
		nullptr											// no additional application data
	);

	// display the window on the screen
	ShowWindow(hWnd, SW_SHOW);

	while (true);
	return 0;
}