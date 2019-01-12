﻿#ifdef SURFACE_WIN32
#include "os/surface.h"
#include "systems/input.h"
#include "assertions.h"
#include <cassert>

using namespace core::os;
using namespace core;

bool surface::init_surface()
{
	static uint64_t _win32_class_id_counter = 0;
	WNDCLASSEX win_class{};
	assert(m_Data->width() > 0);
	assert(m_Data->height() > 0);

	int width  = m_Data->width();
	int height = m_Data->height();

	_win32_instance   = GetModuleHandle(nullptr);
	_win32_class_name = std::to_wstring(_win32_class_id_counter);
	_win32_class_id_counter++;

	int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	// Initialize the window class structure:
	win_class.cbSize		= sizeof(WNDCLASSEX);
	win_class.style			= CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	win_class.lpfnWndProc   = core::systems::input::win_event_handler;
	win_class.cbClsExtra	= 0;
	win_class.cbWndExtra	= 0;
	win_class.hInstance		= _win32_instance; // hInstance
	win_class.hIcon			= NULL;
	win_class.hCursor		= LoadCursor(NULL, IDC_ARROW);
	win_class.hbrBackground = (HBRUSH)COLOR_WINDOW;
	win_class.lpszMenuName  = NULL;
	win_class.lpszClassName = _win32_class_name.c_str();
	win_class.hIconSm		= NULL;
	// Register window class:
	if(!RegisterClassEx(&win_class))
	{
		// It didn't work, so try to give a useful error:
		assert(0 && "Cannot create a window in which to draw!\n");
		fflush(stdout);
		std::exit(-1);
	}
	bool fullscreen = m_Data->mode() == core::gfx::surface_mode::FULLSCREEN;
	if(fullscreen)
	{
		DEVMODE dmScreenSettings;
		memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
		dmScreenSettings.dmSize		  = sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsWidth  = width;
		dmScreenSettings.dmPelsHeight = height;
		dmScreenSettings.dmBitsPerPel = 32;
		dmScreenSettings.dmFields	 = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		if((width != screenWidth) && (height != screenHeight))
		{
			if(ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			{
				if(MessageBox(NULL, L"Fullscreen Mode not supported!\n Switch to window mode?", L"Error",
							  MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
				{
					fullscreen = FALSE;
				}
				else
				{
					return FALSE;
				}
			}
		}
	}

	DWORD ex_style;
	DWORD style;

	if(fullscreen)
	{
		ex_style = WS_EX_APPWINDOW;
		style	= WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	}
	else
	{
		ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
		style = WS_BORDER | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
	}

	// width = fullscreen ? (long)screenWidth : (long)width;
	// height = fullscreen ? (long)screenHeight : (long)height;
	// Create window with the registered class:
	RECT wr = {0, 0, LONG(width), LONG(height)};
	AdjustWindowRectEx(&wr, style, FALSE, ex_style);
	_win32_window = CreateWindowEx(0,
								   _win32_class_name.c_str(),						// class name
								   psl::to_platform_string(m_Data->name()).c_str(), // app name
								   style,											// window style
								   0, 0,											// x/y coords
								   wr.right - wr.left,								// width
								   wr.bottom - wr.top,								// height
								   NULL,											// handle to parent
								   NULL,											// handle to menu
								   _win32_instance,									// hInstance
								   NULL);											// no extra parameters
	if(!_win32_window)
	{
		// It didn't work, so try to give a useful error:
		LOG_ERROR("Cannot create a window in which to draw!");
		fflush(stdout);
		std::exit(-1);
	}
	SetWindowLongPtr(_win32_window, GWLP_USERDATA, (LONG_PTR)this);

	if(!fullscreen)
	{
		// Center on screen
		// uint32_t x = (GetSystemMetrics(SM_CXSCREEN) - wr.right) / 2;
		// uint32_t y = (GetSystemMetrics(SM_CYSCREEN) - wr.bottom) / 2;
		// SetWindowPos(_win32_window, 0, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	}

	ShowWindow(_win32_window, SW_SHOW);
	focus(true);

	RAWINPUTDEVICE Rid[2];

	Rid[0].usUsagePage = 0x01;
	Rid[0].usUsage	 = 0x02;
	Rid[0].dwFlags	 = 0; // adds HID mouse and also ignores legacy mouse messages
	Rid[0].hwndTarget  = NULL;

	Rid[1].usUsagePage = 0x01;
	Rid[1].usUsage	 = 0x06;
	Rid[1].dwFlags	 = 0; // adds HID keyboard and also ignores legacy keyboard messages
	Rid[1].hwndTarget  = NULL;

	if(RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) == FALSE)
	{
		// registration failed. Call GetLastError for the cause of the error

		LOG_ERROR("Failed to init Raw Input: ", GetLastError());
	}
	m_Data->mode((fullscreen) ? core::gfx::surface_mode::FULLSCREEN : core::gfx::surface_mode::WINDOWED);

	RECT windowSize;
	GetClientRect(_win32_window, &windowSize);
	// m_Width = GetSystemMetrics(SM_CXSCREEN);
	// m_Height = GetSystemMetrics(SM_CYSCREEN);
	m_Data->width(windowSize.right - windowSize.left);
	m_Data->height(windowSize.bottom - windowSize.top);
	m_Open = true;
	return true;
}

void surface::deinit_surface()
{
	focus(false);
	DestroyWindow(_win32_window);
	UnregisterClass(_win32_class_name.c_str(), _win32_instance);
}
void surface::focus(bool value)
{
	if(m_Focused == value) return;
	if(value)
	{
		// SetCapture(_win32_window);
		if(m_IndicatorClipped)
		{
			RECT rect;
			GetWindowRect(_win32_window, &rect);
			ClipCursor(&rect);
		}
		if(!m_IndicatorVisible) ShowCursor(false);

		SetForegroundWindow(_win32_window);
		SetFocus(_win32_window);
	}
	else
	{
		if(m_IndicatorClipped)
		{
			ClipCursor(NULL);
		}
		if(!m_IndicatorVisible)
		{
			ShowCursor(true);
			SetCursor(LoadCursor(NULL, IDC_ARROW));
		}
		// auto parent = GetParent(_win32_window);
		// SendMessage(_win32_window, WM_SETCURSOR, (WPARAM)_win32_window, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
		// ReleaseCapture();
	}
	m_Focused = value;
}

void surface::update_surface() { m_InputSystem->tick(); }

void surface::resize_surface() {}
#endif
