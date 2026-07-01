#define GLFW_INCLUDE_VULKAN

#include <chrono>

#include "Enola2Component.h"
#include "Enola2Event.h"

#include "ObjComponent.h"
#include "Shader.h"
#include "App.h"

MyRootComponent rootComponent;

//////////////////////////////////////

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#endif
#ifndef WGL_CONTEXT_MINOR_VERSION_ARB
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#endif
#ifndef WGL_CONTEXT_PROFILE_MASK_ARB
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#endif
#ifndef WGL_CONTEXT_CORE_PROFILE_BIT_ARB
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

using PFNWGLCREATECONTEXTATTRIBSARBPROC =
HGLRC(WINAPI*)(HDC hDC, HGLRC hShareContext, const int* attribList);
using PFNWGLSWAPINTERVALEXTPROC = BOOL(WINAPI*)(int interval);
using PFNDWMFLUSHPROC = HRESULT(WINAPI*)();

static HWND g_hwnd = nullptr;
static HDC g_hdc = nullptr;
static HGLRC g_glrc = nullptr;
static bool g_running = true;
static bool g_vsyncEnabled = false;
static PFNDWMFLUSHPROC g_dwmFlush = nullptr;

static void* GetGLProcAddress(const char* name)
{
	void* p = (void*)wglGetProcAddress(name);

	if (p == nullptr ||
		p == (void*)0x1 ||
		p == (void*)0x2 ||
		p == (void*)0x3 ||
		p == (void*)-1)
	{
		static HMODULE opengl32 = LoadLibraryA("opengl32.dll");
		p = (void*)GetProcAddress(opengl32, name);
	}

	return p;
}

static void InitDwmFlush()
{
	HMODULE dwmapi = LoadLibraryA("dwmapi.dll");
	if (dwmapi)
		g_dwmFlush = (PFNDWMFLUSHPROC)GetProcAddress(dwmapi, "DwmFlush");
}

static void DispatchResize(int width, int height)
{
	if (width <= 0 || height <= 0)
		return;

	rootComponent.SetBounds({ 0, 0, (float)width, (float)height });
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	using namespace Enola2;

	switch (msg)
	{
	case WM_CLOSE:
		g_running = false;
		PostQuitMessage(0);
		return 0;

	case WM_DESTROY:
		g_running = false;
		PostQuitMessage(0);
		return 0;

	case WM_SIZE:
	{
		int width = LOWORD(lParam);
		int height = HIWORD(lParam);
		DispatchResize(width, height);
		return 0;
	}

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		KeyEvent e;
		e.key = (int)wParam;
		e.state = KeyState::Down;
		EventListener::DispatchKey(e);

		if (wParam == VK_ESCAPE)
		{
			g_running = false;
			PostQuitMessage(0);
		}

		return 0;
	}

	case WM_KEYUP:
	case WM_SYSKEYUP:
	{
		KeyEvent e;
		e.key = (int)wParam;
		e.state = KeyState::Up;
		EventListener::DispatchKey(e);
		return 0;
	}

	case WM_MOUSEMOVE:
	{
		MouseEvent e;
		e.x = GET_X_LPARAM(lParam);
		e.y = GET_Y_LPARAM(lParam);
		e.state = MouseState::Move;
		EventListener::DispatchMouse(e);
		return 0;
	}

	case WM_LBUTTONDOWN:
	{
		SetCapture(hwnd);

		MouseEvent e;
		e.x = GET_X_LPARAM(lParam);
		e.y = GET_Y_LPARAM(lParam);
		e.state = MouseState::LeftDown;
		EventListener::DispatchMouse(e);
		return 0;
	}

	case WM_RBUTTONDOWN:
	{
		SetCapture(hwnd);

		MouseEvent e;
		e.x = GET_X_LPARAM(lParam);
		e.y = GET_Y_LPARAM(lParam);
		e.state = MouseState::RightDown;
		EventListener::DispatchMouse(e);
		return 0;
	}

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		ReleaseCapture();
		return 0;

	case WM_MOUSEWHEEL:
	{
		POINT p;
		p.x = GET_X_LPARAM(lParam);
		p.y = GET_Y_LPARAM(lParam);
		ScreenToClient(hwnd, &p);

		MouseEvent e;
		e.x = p.x;
		e.y = p.y;
		e.state = MouseState::Wheel;
		e.wheel = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
		EventListener::DispatchMouse(e);
		return 0;
	}
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void SetupPixelFormat(HDC hdc)
{
	PIXELFORMATDESCRIPTOR pfd{};
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int pixelFormat = ChoosePixelFormat(hdc, &pfd);
	SetPixelFormat(hdc, pixelFormat, &pfd);
}

static bool CreateOpenGLContext(HWND hwnd)
{
	g_hdc = GetDC(hwnd);
	SetupPixelFormat(g_hdc);

	HGLRC tempContext = wglCreateContext(g_hdc);
	wglMakeCurrent(g_hdc, tempContext);

	auto wglCreateContextAttribsARB =
		(PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

	if (wglCreateContextAttribsARB)
	{
		const int attribs[] =
		{
			WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
			WGL_CONTEXT_MINOR_VERSION_ARB, 3,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
			0
		};

		g_glrc = wglCreateContextAttribsARB(g_hdc, nullptr, attribs);

		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(tempContext);

		if (!g_glrc)
			return false;

		wglMakeCurrent(g_hdc, g_glrc);
	}
	else
	{
		g_glrc = tempContext;
	}

	if (!gladLoadGLLoader((GLADloadproc)GetGLProcAddress))
		return false;

	auto wglSwapIntervalEXT =
		(PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	if (wglSwapIntervalEXT)
		g_vsyncEnabled = wglSwapIntervalEXT(1) == TRUE;

	return true;
}

static HWND CreateMainWindow(HINSTANCE instance, int width, int height)
{
	const wchar_t* className = L"FractalDumpsetWindow";

	WNDCLASSW wc{};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = instance;
	wc.lpszClassName = className;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	RegisterClassW(&wc);

	RECT rect{ 0, 0, width, height };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

	return CreateWindowExW(
		0,
		className,
		L".dumpset00",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rect.right - rect.left,
		rect.bottom - rect.top,
		nullptr,
		nullptr,
		instance,
		nullptr
	);
}

int main()
{
	HINSTANCE instance = GetModuleHandleW(nullptr);

	g_hwnd = CreateMainWindow(instance, 800, 600);
	if (!g_hwnd)
		return -1;

	if (!CreateOpenGLContext(g_hwnd))
		return -1;
	InitDwmFlush();

	RECT client{};
	GetClientRect(g_hwnd, &client);

	rootComponent.SetBounds({
		0,
		0,
		(float)(client.right - client.left),
		(float)(client.bottom - client.top)
		});

	rootComponent.DoInit();

	MSG msg{};
	using Clock = std::chrono::steady_clock;
	constexpr auto swapWaitThreshold = std::chrono::microseconds(1000);

	while (g_running)
	{
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				g_running = false;

			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		if (!g_running)
			break;

		rootComponent.DoRender();
		const auto beforeSwap = Clock::now();
		SwapBuffers(g_hdc);
		const auto afterSwap = Clock::now();

		if (afterSwap - beforeSwap > swapWaitThreshold)
			continue;

		if (g_dwmFlush && SUCCEEDED(g_dwmFlush()))
			continue;
	}

	if (g_glrc)
	{
		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(g_glrc);
		g_glrc = nullptr;
	}

	if (g_hwnd && g_hdc)
	{
		ReleaseDC(g_hwnd, g_hdc);
		g_hdc = nullptr;
	}

	return 0;
}
