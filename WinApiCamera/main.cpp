#include <Windows.h>
#include <CommCtrl.h>
#include <vfw.h>
#include "resource.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WIDTH 640
#define HEIGHT 604
#define btn_w 70
#define btn_h 64

enum button_list
{
	OnOff,
	SCREEN,
	REC,
	SETTINGS,
	CLOSE
};

char buttonCaptions[][9] = {
	"ON/OFF",
	"Screen",
	"Rec/Stop",
	"Settings",
	"Close"
};

const char className[] = "WebCam";

SYSTEMTIME sysTime;
WNDCLASSA wndClass;
INITCOMMONCONTROLSEX commCtrls;
HBRUSH hBrush = CreateSolidBrush(RGB(24, 24, 24));
HWND hwnd, hwndChild, buttons[5], hSettings, hFolderPathInput, hSavePathBtn, hCancelSettingsWndBtn;
volatile HWND camHWND;
MSG msg, msg_child;
CAPTUREPARMS capParams;
BOOL isOn = false;
BOOL isRec = false;
volatile BOOL runCamThread = true;
HANDLE camThread;
HICON hPwrOnBtnICO, hPwrOffBtnICO, hRecBtnICO, hStopBtnICO, hSettingsBtnICO, hExitBtnICO;
wchar_t folderPath[255] = {L'\0'};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK CamWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI camThreadProc(LPVOID lpParam);
wchar_t* wordToWchar(WORD &word);
void createFileName(wchar_t* filename);
void formFullFileName(wchar_t filename[279], const wchar_t* extention);
bool isDirectoryExists(wchar_t* folderPath);


int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR pCmdLine, int nCmdShow)
{
	commCtrls.dwSize = sizeof(commCtrls);
	commCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&commCtrls);

	wndClass.hInstance = hInst;
	wndClass.lpszClassName = className;
	wndClass.lpszMenuName = NULL;
	wndClass.lpfnWndProc = WndProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hIcon = LoadIconA(hInst, MAKEINTRESOURCEA(IDI_ICON1));
	wndClass.hCursor = LoadCursorA(hInst, (LPCSTR)IDC_CROSS);
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.hbrBackground = hBrush;
	RegisterClassA(&wndClass);
	
	hwnd = CreateWindowA(
		wndClass.lpszClassName,
		"Camera",
		WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU,
		0, 0, WIDTH, HEIGHT, NULL, NULL, hInst, NULL);

	if (!hwnd)
	{
		MessageBoxA(NULL, "FAILED TO CREATE WINDOW!", "Error", MB_ICONERROR);
		return -1;
	}

	wndClass.lpszClassName = "WebCam_Child";
	wndClass.lpfnWndProc = CamWndProc;
	wndClass.hbrBackground = (HBRUSH)(RGB(0,0,0));
	RegisterClassA(&wndClass);

	buttons[SETTINGS] = CreateWindowA("button", buttonCaptions[SETTINGS], BS_ICON | WS_VISIBLE | WS_CHILD, 5, 483, 40, 40, hwnd, NULL, NULL, NULL);
	buttons[CLOSE] = CreateWindowA("button", buttonCaptions[CLOSE], BS_ICON | WS_VISIBLE | WS_CHILD, 5, 523, 40, 40, hwnd, NULL, NULL, NULL);
	for (unsigned short i = OnOff; i <= REC; i++)
	{
		buttons[i] = CreateWindowA("button", buttonCaptions[i], WS_VISIBLE | WS_CHILD, WIDTH / 2 - 3 / 2.f * btn_w + btn_w * i, 492, btn_w, btn_h, hwnd, NULL, NULL, NULL);
	}
	SetWindowLongPtrA(buttons[OnOff], GWL_STYLE, BS_ICON | WS_VISIBLE | WS_CHILD);
	SetWindowLongPtrA(buttons[REC], GWL_STYLE, BS_ICON | WS_VISIBLE | WS_CHILD);
	hPwrOnBtnICO = (HICON)LoadImageA(hInst, "pwr_on.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	hPwrOffBtnICO = (HICON)LoadImageA(hInst, "pwr_off.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	hRecBtnICO = (HICON)LoadImageA(hInst, "rec48.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	hStopBtnICO = (HICON)LoadImageA(hInst, "stop_btn48.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	hSettingsBtnICO = (HICON)LoadImageA(hInst, "settings32.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	hExitBtnICO = (HICON)LoadImageA(hInst, "exit32.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	SendMessageA(buttons[OnOff], BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hPwrOffBtnICO);
	SendMessageA(buttons[REC], BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hRecBtnICO);
	SendMessageA(buttons[SETTINGS], BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hSettingsBtnICO);
	SendMessageA(buttons[CLOSE], BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hExitBtnICO);

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	camThread = CreateThread(0, 0, camThreadProc, 0, 0, 0);
	while (GetMessage(&msg, NULL, NULL, NULL))
	{
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
		if (lParam == (LPARAM)buttons[SCREEN])
		{
			if (isOn)
			{
				wchar_t filename[279] = { L'\0' };
				formFullFileName(filename, L".bmp");
				SendMessageA(camHWND, WM_CAP_FILE_SAVEDIBW, (WPARAM)0, (LPARAM)filename);
			}
			else
				MessageBoxA(hwnd, "Turn on your camera!", "Warning", MB_ICONWARNING);
		}
		else if (lParam == (LPARAM)buttons[REC])
		{
			if (isOn)
			{
				if (!isRec)
				{
					wchar_t filename[279] = { L'\0' };
					formFullFileName(filename, L".avi");
					SendMessage(camHWND, WM_CAP_FILE_SET_CAPTURE_FILEW, (WPARAM)0, (LPARAM)filename);
					isRec = capCaptureSequence(camHWND);
					if (isRec)
						SendMessageA(buttons[REC], BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hStopBtnICO);
				}
				else
				{
					isRec = capCaptureStop(camHWND);
					isRec = !isRec;
					if (!isRec)
						SendMessageA(buttons[REC], BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hRecBtnICO);
				}
			}
			else
				MessageBoxA(hwnd, "Turn on your camera!", "Warning", MB_ICONWARNING);
		}
		else if (lParam == (LPARAM)buttons[OnOff])
		{
			if (!isOn)
			{
				SendMessageA(camHWND, WM_CAP_DRIVER_CONNECT, (WPARAM)(0), 0L);
				isOn = SendMessageA(camHWND, WM_CAP_SET_PREVIEW, (WPARAM)(TRUE), 0L);
				if (isOn)
					SendMessageA(buttons[OnOff], BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hPwrOnBtnICO);
			}
			else
			{
				isOn = SendMessageA(camHWND, WM_CAP_SET_PREVIEW, (WPARAM)(FALSE), 0L);
				isOn = !isOn;
				if (!isOn)
					SendMessageA(buttons[OnOff], BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hPwrOffBtnICO);
				SendMessageA(camHWND, WM_CAP_DRIVER_DISCONNECT, (WPARAM)(0), 0L);
			}
		}
		else if (lParam == (LPARAM)buttons[SETTINGS])
		{
			wndClass.lpszClassName = "Camera settings";
			wndClass.lpfnWndProc = SettingsWndProc;
			RegisterClassA(&wndClass);
			if (hSettings == NULL)
			{
				RECT rect;
				GetWindowRect(hwnd, &rect);
				hSettings = CreateWindowA(wndClass.lpszClassName, "Camera settings", 
					WS_OVERLAPPED | WS_SYSMENU | WS_VISIBLE, rect.left, rect.top, 300, 200, 
					hwnd, NULL, wndClass.hInstance, NULL);
				ShowWindow(hSettings, SW_NORMAL);
				UpdateWindow(hSettings);
				hFolderPathInput = CreateWindowA("edit", "File path", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 20, 20, 240, 20, hSettings, NULL, wndClass.hInstance, NULL);
				hSavePathBtn = CreateWindowA("button", "Save", WS_CHILD | WS_VISIBLE, 80, 80, 54, 30, hSettings, NULL, wndClass.hInstance, NULL);
				hCancelSettingsWndBtn = CreateWindowA("button", "Cancel", WS_CHILD | WS_VISIBLE, 150, 80, 54, 30, hSettings, NULL, wndClass.hInstance, NULL);
			}
		}
		else if (lParam == (LPARAM)buttons[CLOSE])
		{
			capDriverDisconnect(camHWND);
			runCamThread = false;
			CloseHandle(camThread);
			PostQuitMessage(0);
			ExitProcess(0);
		}
		break;
	case WM_DESTROY:
		capDriverDisconnect(camHWND);
		runCamThread = false;
		CloseHandle(camThread);
		PostQuitMessage(0);
		ExitProcess(0);
		break;
	default:
		return DefWindowProcA(hwnd, message, wParam, lParam);
	}
	return 0;
}

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_COMMAND:
		if (lParam == (LPARAM)hSavePathBtn)
		{
			GetWindowTextW(hFolderPathInput, folderPath, 253);
			if (!isDirectoryExists(folderPath))
			{
				MessageBoxW(NULL, folderPath, L"No such directory or access denied!", MB_ICONERROR);
				folderPath[0] = L'\0';
			}
			else
				MessageBoxW(NULL, folderPath, L"Current saves directory", MB_OK);
		}
		else if (lParam == (LPARAM)hCancelSettingsWndBtn)
		{
			DestroyWindow(hFolderPathInput);
			DestroyWindow(hSettings);
			hSettings = NULL;
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hFolderPathInput);
		DestroyWindow(hSettings);
		hSettings = NULL;
		break;
	default:
		return DefWindowProcA(hwnd, msg, wParam, lParam);
	}
	return 0;
}

LRESULT CALLBACK CamWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) 
{
	switch (msg)
	{
	default:
		return DefWindowProcA(hwnd, msg, wParam, lParam);
	}
	return 0;
}

DWORD WINAPI camThreadProc(LPVOID lpParam)
{
	hwndChild = CreateWindowA(wndClass.lpszClassName,
		"Camera_child",
		WS_CHILD,
		0, 0, WIDTH, HEIGHT - 124,
		hwnd, NULL, wndClass.hInstance, NULL);

	if (!hwndChild)
	{
		MessageBoxA(NULL, "FAILED TO CREATE CHILD WINDOW!", "Error", NULL);
		return -1;
	}

	camHWND = capCreateCaptureWindowA("Camera Capture Window",
		WS_CHILD | WS_VISIBLE, 0, 0, WIDTH, HEIGHT - 100, hwndChild, NULL);

	capCaptureGetSetup(camHWND, &capParams, sizeof(capParams));
	capParams.fYield = TRUE;
	capParams.fAbortLeftMouse = FALSE;
	capParams.fAbortRightMouse = FALSE;
	capParams.vKeyAbort = 0;
	capParams.dwRequestMicroSecPerFrame = (DWORD)(1.0e6 / 60);
	capCaptureSetSetup(camHWND, &capParams, sizeof(capParams));

	SendMessageA(camHWND, WM_CAP_SET_PREVIEWRATE, (WPARAM)(1), 0L);
	ShowWindow(hwndChild, 10);
	UpdateWindow(hwndChild);
	while (GetMessage(&msg_child, hwndChild, NULL, NULL))
	{
		TranslateMessage(&msg_child);
		DispatchMessageA(&msg_child);
	}
	return (int)msg_child.wParam;
}

wchar_t* wordToWchar(WORD& word)
{
	unsigned short number = word;
	unsigned short i = 0;
	unsigned short middle = 0;
	wchar_t str[6] = { L'\0' };
	wchar_t buf;
	if (number == 0)
	{
		str[0] = L'0';
		str[1] = L'0';
		i = 2;
	}
	while (number != 0)
	{
		str[i] = number % 10 + wchar_t(L'0');
		i++;
		number /= 10;
	}
	middle = i / 2;
	if (middle != 0)
		for (i = 0; i < middle; i++)
		{
			buf = str[lstrlenW(str) - 1 - i];
			str[lstrlenW(str) - 1 - i] = str[i];
			str[i] = buf;
		}
	wchar_t* dest = new wchar_t[lstrlenW(str) + 1];
	i = 0;
	while (str[i])
	{
		dest[i] = str[i];
		i++;
	}
	dest[i] = L'\0';
	return dest;
}

void createFileName(wchar_t* filename)
{
	wchar_t* buf = nullptr;
	buf = wordToWchar(sysTime.wHour);
	wcscat_s(filename, 24, buf);
	delete[] buf;
	buf = nullptr;
	wcscat_s(filename, 24, L"-");
	buf = wordToWchar(sysTime.wMinute);
	wcscat_s(filename, 24, buf);
	delete[] buf;
	buf = nullptr;
	wcscat_s(filename, 24, L"-");
	buf = wordToWchar(sysTime.wSecond);
	wcscat_s(filename, 24, buf);
	delete[] buf;
	buf = nullptr;
	wcscat_s(filename, 24, L" ");
	buf = wordToWchar(sysTime.wDay);
	wcscat_s(filename, 24, buf);
	delete[] buf;
	buf = nullptr;
	wcscat_s(filename, 24, L"-");
	buf = wordToWchar(sysTime.wMonth);
	wcscat_s(filename, 24, buf);
	delete[] buf;
	buf = nullptr;
	wcscat_s(filename, 24, L"-");
	buf = wordToWchar(sysTime.wYear);
	wcscat_s(filename, 24, buf);
	delete[] buf;
	buf = nullptr;
}

void formFullFileName(wchar_t filename[279], const wchar_t* fileExtention)
{
	if (lstrlenW(folderPath) != 0)
	{
		wcscat_s(filename, 279, folderPath);
		if (folderPath[lstrlenW(folderPath) - 1] != L'\\')
		{
			filename[lstrlenW(folderPath)] = L'\\';
			filename[lstrlenW(folderPath) + 1] = L'\0';
		}
	}
	wchar_t tmpFileName[24] = { L'\0' };
	GetLocalTime(&sysTime);
	createFileName(tmpFileName);
	wcscat_s(filename, 279, tmpFileName);
	wcscat_s(filename, 279, fileExtention);
}

bool isDirectoryExists(wchar_t* folderPath)
{
	DWORD dwFileAttributes = GetFileAttributesW(folderPath);
	return (dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY || dwFileAttributes == 22) ? true : false;
}
