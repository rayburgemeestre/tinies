#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#include <windows.h>
#include <shlobj.h>

#include "inc-getDesktopWindow.cpp"

enum Hotkeys {
	ONTOP,
	TERMINAL,
	LOCKOFF,
	EXIT,
};

/** MSDN Blogs > The Old New Thing > Querying information from an Explorer window:
 *	 http://blogs.msdn.com/b/oldnewthing/archive/2004/07/20/188696.aspx */
bool explorerWindow(HWND hwnd, WCHAR *buf) {
	bool ret = false;
	IShellWindows *psw;
	if (SUCCEEDED(CoCreateInstance(CLSID_ShellWindows, NULL, CLSCTX_ALL, IID_IShellWindows, (void**)&psw))) {
		VARIANT v;
		V_VT(&v) = VT_I4;
		IDispatch	*pdisp;
		BOOL fFound = FALSE;
		for (V_I4(&v) = 0; !fFound && psw->Item(v, &pdisp) == S_OK; V_I4(&v)++) {
			IWebBrowserApp *pwba;
			if (SUCCEEDED(pdisp->QueryInterface(IID_IWebBrowserApp, (void**)&pwba))) {
				HWND hwndWBA;
				if (SUCCEEDED(pwba->get_HWND((LONG_PTR*)&hwndWBA)) && hwndWBA == hwnd) {
					fFound = TRUE;
					IServiceProvider *psp;
					if (SUCCEEDED(pwba->QueryInterface(IID_IServiceProvider, (void**)&psp))) {
						IShellBrowser *psb;
						if (SUCCEEDED(psp->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void**)&psb))) {
							IShellView *psv;
							if (SUCCEEDED(psb->QueryActiveShellView(&psv))) {
								IFolderView *pfv;
								if (SUCCEEDED(psv->QueryInterface(IID_IFolderView, (void**)&pfv))) {
									IPersistFolder2 *ppf2;
									if (SUCCEEDED(pfv->GetFolder(IID_IPersistFolder2, (void**)&ppf2))) {
										LPITEMIDLIST pidlFolder;
										if (SUCCEEDED(ppf2->GetCurFolder(&pidlFolder))) {
											ret = TRUE == SHGetPathFromIDList(pidlFolder, buf);
											CoTaskMemFree(pidlFolder);
										}
										ppf2->Release();
									}
									pfv->Release();
								}
								psv->Release();
							}
							psb->Release();
						}
						psp->Release();
					}
				}
				pwba->Release();
			}
			pdisp->Release();
		}
		psw->Release();
	}
	return ret;
}

bool getKnownFolderPath(REFKNOWNFOLDERID id, WCHAR *buf) {
	PWSTR path;
	if (FAILED(SHGetKnownFolderPath(id, 0, NULL, &path)))
		return false;

	const bool ret = 0 == wcscpy_s(buf, MAX_PATH, path);
	CoTaskMemFree(path);
	return ret;
}

bool desktopWindow(HWND h, WCHAR *buf) {
	HWND desktop = getDesktopWindow();
	while (h != desktop && NULL != (desktop = GetParent(desktop)));

	if (NULL == desktop)
		return false;

	return getKnownFolderPath(FOLDERID_Desktop, buf);
}

bool getDefaultFolder(WCHAR *buf) {
	return getKnownFolderPath(FOLDERID_Profile, buf);
}

/** Repeatedly trim a string at '\'s and ' 's until it's a directory */
void clean(WCHAR *buf) {
	while (*buf) {
		DWORD attr = GetFileAttributes(buf);
		if (INVALID_FILE_ATTRIBUTES != attr && (attr & FILE_ATTRIBUTE_DIRECTORY))
			return;

		WCHAR *slash = wcsrchr(buf, '\\');
		WCHAR *space = wcsrchr(buf, ' ');
		if (slash || space)
			if (slash > space)
				*slash = 0;
			else
				*space = 0;
		else {
			const size_t len = wcsnlen_s(buf, MAX_PATH);
			if (len > 0)
				buf[len-1] = 0;
			else
				return;
		}
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	CoInitialize(NULL);
	HWND msgwnd = CreateWindow(L"STATIC", L"Topkey window", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, GetModuleHandle(NULL), 0);

	if ((msgwnd == NULL) || (RegisterHotKey(msgwnd, ONTOP, MOD_CONTROL | MOD_ALT, 'W') == 0)
						 || (RegisterHotKey(msgwnd, TERMINAL, MOD_CONTROL | MOD_ALT, VK_RETURN) == 0)
						 || (RegisterHotKey(msgwnd, LOCKOFF, MOD_CONTROL | MOD_ALT | MOD_SHIFT, 'L') == 0)
						 || (RegisterHotKey(msgwnd, EXIT, MOD_CONTROL | MOD_ALT | MOD_SHIFT, 'W') == 0))
	{
		MessageBox(msgwnd, L"Unable to register hotkeys, exiting.", L"Error", MB_ICONERROR | MB_OK);
		DestroyWindow(msgwnd);
		return 1;
	}

	MSG msg;
	BOOL ret;
	while (0 != (ret = GetMessage(&msg, msgwnd, 0, 0))) {
		if (-1 == ret)
			return 1;

		switch (msg.message) {
		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_HOTKEY: {
			switch (msg.wParam)
			{
				case ONTOP: {
					HWND fg = GetForegroundWindow();
					HWND insertAfter;
					if (GetWindowLong(fg, GWL_EXSTYLE) & WS_EX_TOPMOST)
						insertAfter = HWND_NOTOPMOST;
					else
						insertAfter = HWND_TOPMOST;
					SetWindowPos(fg, insertAfter, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
					MessageBeep(MB_OK);
				} break;

				case LOCKOFF: {
					Sleep(5);
					LockWorkStation();
					Sleep(500);
					SendMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
				} break;

				case TERMINAL: {
					const int MAX = 512;
					HWND fg = GetForegroundWindow();
					WCHAR buf[MAX] = {};
					explorerWindow(fg, buf)
						|| desktopWindow(fg, buf)
						|| 0 != GetWindowText(fg, buf, MAX);
					clean(buf);

					if (!*buf)
						getDefaultFolder(buf);

					ShellExecute(fg, NULL, L"cmd", NULL, buf, SW_SHOW);
				} break;
				
				case EXIT:
					PostQuitMessage(0);
					break;
			}
			
		} break;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}
