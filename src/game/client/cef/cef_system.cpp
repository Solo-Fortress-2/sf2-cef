/*
 * Copyright (c) 2025 The Solo Fortress 2 Team, all rights reserved.
 * cef_system.cpp, Initializes and configures Chromium Embedded Framework.
 *
 * Licensed under CC BY-NC 3.0 (Lambda Wars' original license)
 */

#include "cbase.h"
#include "cef_system.h"
#include "cef_browser.h"
#include "cef_os_renderer.h"
#include "cef_local_handler.h"
#include "cef_avatar_handler.h"
#include "cef_vtf_handler.h"

#include "cef_cxx20_stubs.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_client.h"
#include "include/cef_sandbox_win.h"

#ifdef WIN32
#include <windows.h>
#include <imm.h>
#endif // WIN32

#include <vgui/IInput.h>
#include <vgui_controls/Controls.h>
#include "sf2_shareddefs.h"
#include "inputsystem/iinputsystem.h"
#include "materialsystem/materialsystem_config.h"
#include "filesystem.h"

#ifdef _WIN32
#	ifndef _WIN64
#		define SF2_BROWSER_SUBPROCESS_PATH "bin/cef_subprocess.exe"
#	else
#		define SF2_BROWSER_SUBPROCESS_PATH "bin/x64/cef_subprocess.exe"
#	endif
#else
#	define SF2_BROWSER_SUBPROCESS_PATH "bin/x64/cef_subprocess"
#endif

#pragma comment(lib, "user32.lib")
typedef SHORT(WINAPI* GetAsyncKeyStateFn)(WPARAM);
static GetAsyncKeyStateFn pGetAsyncKeyState =
	(GetAsyncKeyStateFn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetAsyncKeyState");

#ifdef WIN32
static WNDPROC s_pChainedWndProc;

LRESULT CALLBACK CefWndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK CefWndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (CEFSystem().IsRunning())
	{
		switch (message)
		{
		case WM_SYSCHAR:
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYUP:
		case WM_CHAR:
		{
			CEFSystem().ProcessKeyInput(message, wParam, lParam);
			break;
		}
		case WM_DEADCHAR:
		{
			CEFSystem().ProcessDeadChar(message, wParam, lParam);
			break;
		}
		case WM_IME_STARTCOMPOSITION:
		{
			break;
		}
		case WM_IME_ENDCOMPOSITION:
		{
			break;
		}
		case WM_IME_COMPOSITION:
		{
			HIMC hIMC = ImmGetContext((HWND)vgui::input()->GetIMEWindow());
			if (hIMC)
			{
				wchar_t tempstr[32];

				int len = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, (LPVOID)tempstr, sizeof(tempstr));
				if (len > 0)
				{
					if ((len % 2) != 0)
						len++;
					int numchars = len / sizeof(wchar_t);

					for (int i = 0; i < numchars; ++i)
					{
						CEFSystem().ProcessCompositionResult(tempstr[i]);
					}

				}

				ImmReleaseContext((HWND)vgui::input()->GetIMEWindow(), hIMC);
			}

			break;
		}
		case WM_MOUSEWHEEL:
		{
			CEFSystem().SetLastMouseWheelDist((short)HIWORD(wParam));
			break;
		}
		default:
			break;
		}
	}

	return CallWindowProc(s_pChainedWndProc, hWnd, message, wParam, lParam);
}
#endif // WIN32

//-----------------------------------------------------------------------------
// Purpose: Client App
//-----------------------------------------------------------------------------
class ClientApp : public CefApp,
	public CefBrowserProcessHandler
{
public:
	ClientApp(bool bCefEnableGPU, bool bDisableBeginFrameScheduling);

protected:
	// CefBrowserProcessHandler
	virtual void OnContextInitialized();

private:
	// CefApp methods.
	virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() { return this; }

	virtual void OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line);

	virtual void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar);

private:
	IMPLEMENT_REFCOUNTING(ClientApp);

	bool m_bEnableGPU;
	bool m_bDisableBeginFrameScheduling;
};

CefRefPtr<ClientApp> g_pClientApp;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
ClientApp::ClientApp(bool bCefEnableGPU, bool bDisableBeginFrameScheduling) :
	m_bEnableGPU(bCefEnableGPU), m_bDisableBeginFrameScheduling(bDisableBeginFrameScheduling)
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ClientApp::OnContextInitialized()
{
	CefRegisterSchemeHandlerFactory("avatar", "small", new AvatarSchemeHandlerFactory(AvatarSchemeHandlerFactory::k_AvatarTypeSmall));
	CefRegisterSchemeHandlerFactory("avatar", "medium", new AvatarSchemeHandlerFactory(AvatarSchemeHandlerFactory::k_AvatarTypeMedium));
	CefRegisterSchemeHandlerFactory("avatar", "large", new AvatarSchemeHandlerFactory(AvatarSchemeHandlerFactory::k_AvatarTypeLarge));
	CefRegisterSchemeHandlerFactory("vtf", "", new VTFSchemeHandlerFactory());
	CefRegisterSchemeHandlerFactory("local", "", new LocalSchemeHandlerFactory());
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ClientApp::OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line)
{
	command_line->AppendSwitch(CefString("no-proxy-server"));
	command_line->AppendSwitch(CefString("disable-sync"));

	// Can be disabled through command line (-cef_disable_begin_frame_scheduling) for launching dev tools ingame
	// This option does not work because we use window rendering for the dev tools, or we need to write a off screen rendered for it.
	// Alternatively you can specify -cef_remote_dbg_port on the command line, and visit localhost:port for inspectable pages.
	if (!m_bDisableBeginFrameScheduling)
	{
		command_line->AppendSwitch(CefString("enable-begin-frame-scheduling"));
	}

	if (!m_bEnableGPU)
	{
		command_line->AppendSwitch(CefString("disable-gpu"));
		command_line->AppendSwitch(CefString("disable-gpu-compositing"));
		command_line->AppendSwitch(CefString("disable-d3d11"));
	}

	DevMsg("Cef Command line arguments: %s\n", command_line->GetCommandLineString().ToString().c_str());
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ClientApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar)
{
	registrar->AddCustomScheme("avatar", CEF_SCHEME_OPTION_STANDARD);
	registrar->AddCustomScheme("vtf", CEF_SCHEME_OPTION_LOCAL);
	registrar->AddCustomScheme("local", CEF_SCHEME_OPTION_LOCAL);
}

CCefSystem::CCefSystem()
	: CAutoGameSystemPerFrame("chromium_system"),
	m_bIsRunning(false), m_bHasKeyFocus(false)
{
}

CCefSystem::~CCefSystem()
{
}

static ConVarRef fps_max("fps_max");

bool CCefSystem::Init()
{
	const bool bEnabled = !CommandLine() || CommandLine()->FindParm("-disablecef") == 0;
	if (!bEnabled)
	{
		Warning("CCefSystem: Not initializing, disabled in command line\n");
		return true;
	}

	const bool bCefEnableGPU = CommandLine() && CommandLine()->FindParm("-cef_enable_gpu") != 0;
	const bool bDisableBeginFrameScheduling = CommandLine() && CommandLine()->FindParm("-cef_disable_begin_frame_scheduling") != 0;

	const int iRemoteDebuggingPort = (CommandLine() && CommandLine()->FindParm("-cef_remote_dbg_port") != 0) ? CommandLine()->ParmValue("-cef_remote_dbg_port", 0) : 0;

	if (iRemoteDebuggingPort != 0)
	{
		Msg("Chromium Embedded remote debugging is enabled. Visit http://localhost:%d for inspectable pages.\n", iRemoteDebuggingPort);
	}

	// Get path to subprocess browser
	// Note: use relative path, because otherwise the path may be invalid.
	// Working directory is changed to the game folder by CSrcPython.
	char browser_subprocess_path[MAX_PATH];
	Q_strncpy(browser_subprocess_path, SF2_BROWSER_SUBPROCESS_PATH, sizeof(browser_subprocess_path));
	filesystem->RelativePathToFullPath( SF2_BROWSER_SUBPROCESS_PATH, "MOD", browser_subprocess_path, sizeof( browser_subprocess_path ) );

	// The process sub process file should exist. Error out, because otherwise we can't display the main menu
	if( filesystem->FileExists( browser_subprocess_path ) == false )
	{
		Error( "Could not locate \"%s\". Please check your installation, otherwise disable WebUI with -oldmainmenu.\n", browser_subprocess_path );
		return false;
	}

	// Arguments
	HINSTANCE hinst = (HINSTANCE)GetModuleHandle(NULL);
	CefMainArgs main_args(hinst);
	g_pClientApp = new ClientApp(bCefEnableGPU, bDisableBeginFrameScheduling);

	// Settings
	CefSettings settings;
#ifdef USE_MULTITHREADED_MESSAGELOOP
	settings.multi_threaded_message_loop = true;
#else 
	settings.multi_threaded_message_loop = false;
#endif // USE_MULTITHREADED_MESSAGELOOP
	settings.log_severity = developer.GetBool() ? LOGSEVERITY_VERBOSE : LOGSEVERITY_DEFAULT;
	settings.command_line_args_disabled = true; // Specify args through OnBeforeCommandLineProcessing
	settings.remote_debugging_port = iRemoteDebuggingPort;
	settings.windowless_rendering_enabled = true;
#if !CEF_ENABLE_SANDBOX
	settings.no_sandbox = true;
#endif
	CefString(&settings.cache_path) = CefString("cache");
	CefString(&settings.user_agent_product) = CefString("SF2 CEF/0.4.0");
	CefString(&settings.browser_subprocess_path) = CefString(browser_subprocess_path);

	// Initialize CEF.
	void* sandbox_info = nullptr;
#if CEF_ENABLE_SANDBOX
	CefScopedSandboxInfo scoped_sandbox;
	sandbox_info = scoped_sandbox.sandbox_info();
#endif

	if (!CefInitialize(main_args, settings, g_pClientApp.get(), sandbox_info))
	{
		DevWarning("Failed to initialize Chromium Embedded Framework!\n");
		return false;
	}

	DevMsg("Initialized CEF\n");

	m_bIsRunning = true;
	return true;
}

void CCefSystem::Shutdown()
{
	if (!m_bIsRunning)
		return;

	DevMsg("Shutting down CEF (%d active browsers)\n", m_CefBrowsers.Count());

	CefClearSchemeHandlerFactories();

	// Make sure all browsers are closed
	for (int i = m_CefBrowsers.Count() - 1; i >= 0; i--)
		m_CefBrowsers[i]->Destroy();

#ifndef USE_MULTITHREADED_MESSAGELOOP
	CefDoMessageLoopWork();
#endif // USE_MULTITHREADED_MESSAGELOOP

	// Shut down CEF.
	CefShutdown();

	g_pClientApp = nullptr;

	m_bIsRunning = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefSystem::Update(float frametime)
{
	if (!m_bIsRunning)
		return;

	VPROF_BUDGET("CCefSystem::Update", "CCefSystem");

	// Detect if the user alt-tabbed and tell cef we changed
	// TODO: Probably just need to redraw the texture
	static bool sActiveApp = false;
	if (sActiveApp != engine->IsActiveApp())
	{
		sActiveApp = engine->IsActiveApp();
		const MaterialSystem_Config_t& config = materials->GetCurrentConfigForVideoCard();
		if (sActiveApp && !config.Windowed())
		{
			OnScreenSizeChanged(ScreenWidth(), ScreenHeight());
		}
	}

#ifndef USE_MULTITHREADED_MESSAGELOOP
	// Perform a single iteration of the CEF message loop
	CefDoMessageLoopWork();
#endif // USE_MULTITHREADED_MESSAGELOOP

	// Let browser think
	for (int i = m_CefBrowsers.Count() - 1; i >= 0; i--)
	{
		if (m_CefBrowsers[i]->IsValid())
			m_CefBrowsers[i]->Think();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefSystem::LevelInitPreEntity()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefSystem::LevelInitPostEntity()
{
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefSystem::AddBrowser(CCefBrowser* pBrowser)
{
	m_CefBrowsers.AddToTail(pBrowser);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefSystem::RemoveBrowser(CCefBrowser* pBrowser)
{
	m_CefBrowsers.FindAndRemove(pBrowser);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCefSystem::CountBrowsers(void)
{
	return m_CefBrowsers.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCefBrowser* CCefSystem::GetBrowser(int idx)
{
	if (m_CefBrowsers[idx]->IsValid() && m_CefBrowsers.IsValidIndex(idx))
		return m_CefBrowsers[idx];
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCefBrowser* CCefSystem::FindBrowser(CefBrowser* pBrowser)
{
	for (int i = 0; i < m_CefBrowsers.Count(); i++)
	{
		if (m_CefBrowsers[i]->IsValid() && m_CefBrowsers[i]->GetBrowser() == pBrowser)
			return m_CefBrowsers[i];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCefBrowser* CCefSystem::FindBrowserByName(const char* pName)
{
	for (int i = 0; i < m_CefBrowsers.Count(); i++)
	{
		if (m_CefBrowsers[i]->IsValid() && V_strcmp(m_CefBrowsers[i]->GetName(), pName) == 0)
			return m_CefBrowsers[i];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCefSystem::KeyInput(int down, ButtonCode_t keynum, const char* pszCurrentBinding)
{
	// Give browser implementations directly a chance to override the input
	for (int i = 0; i < m_CefBrowsers.Count(); i++)
	{
		if (!m_CefBrowsers[i]->IsValid() || !m_CefBrowsers[i]->IsFullyVisible())
			continue;

		int ret = m_CefBrowsers[i]->KeyInput(down, keynum, pszCurrentBinding);
		if (ret == 0)
			return 0;
	}

	// CEF has key focus, so don't process any keys (apart from the escape key)
	if (m_bHasKeyFocus && (keynum != KEY_ESCAPE && (!pszCurrentBinding || V_strcmp("toggleconsole", pszCurrentBinding) != 0)))
		return 0;

	return 1;
}

#ifdef _WIN32
static bool isKeyDown(WPARAM wparam)
{
	return pGetAsyncKeyState && (pGetAsyncKeyState(wparam) & 0x8000);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static int getKeyModifiers(WPARAM wparam, LPARAM lparam)
{
	int modifiers = 0;
	if (isKeyDown(VK_SHIFT))
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	if (isKeyDown(VK_CONTROL))
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	if (isKeyDown(VK_MENU))
		modifiers |= EVENTFLAG_ALT_DOWN;

	// Low bit set from GetKeyState indicates "toggled".
	if (pGetAsyncKeyState && (pGetAsyncKeyState(VK_NUMLOCK) & 1))
		modifiers |= EVENTFLAG_NUM_LOCK_ON;
	if (pGetAsyncKeyState && (pGetAsyncKeyState(VK_NUMLOCK) & 1))
		modifiers |= EVENTFLAG_CAPS_LOCK_ON;

	switch (wparam) {
	case VK_RETURN:
		if ((lparam >> 16) & KF_EXTENDED)
			modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_INSERT:
	case VK_DELETE:
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_UP:
	case VK_DOWN:
	case VK_LEFT:
	case VK_RIGHT:
		if (!((lparam >> 16) & KF_EXTENDED))
			modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_NUMLOCK:
	case VK_NUMPAD0:
	case VK_NUMPAD1:
	case VK_NUMPAD2:
	case VK_NUMPAD3:
	case VK_NUMPAD4:
	case VK_NUMPAD5:
	case VK_NUMPAD6:
	case VK_NUMPAD7:
	case VK_NUMPAD8:
	case VK_NUMPAD9:
	case VK_DIVIDE:
	case VK_MULTIPLY:
	case VK_SUBTRACT:
	case VK_ADD:
	case VK_DECIMAL:
	case VK_CLEAR:
		modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_SHIFT:
		if (isKeyDown(VK_LSHIFT))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (isKeyDown(VK_RSHIFT))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_CONTROL:
		if (isKeyDown(VK_LCONTROL))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (isKeyDown(VK_RCONTROL))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_MENU:
		if (isKeyDown(VK_LMENU))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (isKeyDown(VK_RMENU))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_LWIN:
		modifiers |= EVENTFLAG_IS_LEFT;
		break;
	case VK_RWIN:
		modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	}
	return modifiers;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefSystem::ProcessKeyInput(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (wParam == VK_ESCAPE)
		return;

	CefKeyEvent keyevent;

	if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
	{
		keyevent.type = KEYEVENT_RAWKEYDOWN;
	}
	else if (message == WM_KEYUP || message == WM_SYSKEYUP)
	{
		keyevent.type = KEYEVENT_KEYUP;
	}
	else
	{
		keyevent.type = KEYEVENT_CHAR;

		HKL currentKb = ::GetKeyboardLayout(0);

		// Source's input system seems to be doing the same call to ToUnicodeEx, due which 
		// it breaks (since the dead char is buffered and processed in the next call).
		// Delay processing it, so everything gets called in the right order.
		if (m_bHasDeadChar)
		{
			m_bHasDeadChar = false;

			CefKeyEvent deadCharKeyevent;
			deadCharKeyevent.type = KEYEVENT_CHAR;

			wchar_t unicode[2];
#ifdef _DEBUG
			int deadCharRet =
#endif // _DEBUG
				ToUnicodeEx(m_lastDeadChar_virtualKey, m_lastDeadChar_scancode, (BYTE*)m_lastDeadChar_kbrdState, unicode, 2, 0, currentKb);
			Assert(deadCharRet == -1); // -1 means dead char
		}

		// CEF key event expects the unicode version, but our multi byte application does not
		// receive the right code from the message loop. This is a problem for languages such as
		// Cyrillic. Convert the virtual key to the right unicode char.
		UINT scancode = (lParam >> 16) & 0xFF;
		UINT virtualKey = MapVirtualKeyEx(scancode, MAPVK_VSC_TO_VK, currentKb);

		BYTE kbrdState[256];
		if (GetKeyboardState(kbrdState))
		{
			wchar_t unicode[2];
			int ret = ToUnicodeEx(virtualKey, scancode, (BYTE*)kbrdState, unicode, 2, 0, currentKb);

			// Only change wParam if there is a translation for our active keyboard
			if (ret == 1)
			{
				wParam = unicode[0];
			}
		}
	}

	keyevent.character = (wchar_t)wParam;
	keyevent.unmodified_character = (wchar_t)wParam;

	keyevent.windows_key_code = wParam;
	keyevent.native_key_code = lParam;
	keyevent.is_system_key = message == WM_SYSCHAR ||
		message == WM_SYSKEYDOWN ||
		message == WM_SYSKEYUP;

	m_iKeyModifiers = getKeyModifiers(wParam, lParam);
	keyevent.modifiers = m_iKeyModifiers;

	SendKeyEventToBrowsers(keyevent);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefSystem::ProcessCompositionResult(wchar_t result)
{
	CefKeyEvent keyevent;
	keyevent.type = KEYEVENT_CHAR;

	keyevent.character = (wchar_t)result;
	keyevent.unmodified_character = (wchar_t)result;

	keyevent.windows_key_code = result;
	//keyevent.native_key_code = lParam;
	keyevent.is_system_key = false;

	m_iKeyModifiers = getKeyModifiers(result, 0);
	keyevent.modifiers = m_iKeyModifiers;

	SendKeyEventToBrowsers(keyevent);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefSystem::ProcessDeadChar(UINT message, WPARAM wParam, LPARAM lParam)
{
	m_bHasDeadChar = true;
	HKL currentKb = ::GetKeyboardLayout(0);
	m_lastDeadChar_scancode = (lParam >> 16) & 0xFF;
	m_lastDeadChar_virtualKey = MapVirtualKeyEx(m_lastDeadChar_scancode, MAPVK_VSC_TO_VK, currentKb);
	GetKeyboardState(m_lastDeadChar_kbrdState);
}
#endif // _WIN32

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefSystem::SendKeyEventToBrowsers(const CefKeyEvent& keyevent)
{
	for (int i = 0; i < m_CefBrowsers.Count(); i++)
	{
		CCefBrowser* pBrowser = m_CefBrowsers[i];
		if (!pBrowser->IsValid() || !pBrowser->IsFullyVisible() || !pBrowser->IsGameInputEnabled())
			continue;

		if (pBrowser->GetIgnoreTabKey() && keyevent.windows_key_code == VK_TAB)
			continue;

		// Only send key input if no vgui panel has key focus
		// TODO: Deal with game bindings
		vgui::VPANEL focus = vgui::input()->GetFocus();
		vgui::Panel* pPanel = m_CefBrowsers[i]->GetPanel();
		if (!pPanel || !pPanel->IsVisible() || (focus != 0 && focus != pPanel->GetVPanel()))
			continue;

		CefRefPtr<CefBrowser> browser = m_CefBrowsers[i]->GetBrowser();

		browser->GetHost()->SendKeyEvent(keyevent);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefSystem::OnScreenSizeChanged(int nOldWidth, int nOldHeight)
{
	// Invalidate Layout of all browsers. This will make it call PerformLayout next think.
	for (int i = m_CefBrowsers.Count() - 1; i >= 0; i--)
	{
		if (!m_CefBrowsers[i]->IsValid())
			continue;

		m_CefBrowsers[i]->InvalidateLayout();
		m_CefBrowsers[i]->NotifyScreenInfoChanged();

#ifdef RENDER_DIRTY_AREAS
		m_CefBrowsers[i]->GetPanel()->MarkTextureDirty(0, 0, m_CefBrowsers[i]->GetPanel()->GetWide(),
			m_CefBrowsers[i]->GetPanel()->GetTall());
#else
		m_CefBrowsers[i]->GetPanel()->MarkTextureDirty();
#endif

		CefRefPtr<CCefOSRRenderer> renderer = m_CefBrowsers[i]->GetOSRHandler();
		if (renderer)
		{
			renderer->UpdateRootScreenRect(0, 0, ScreenWidth(), ScreenHeight());
		}
	}
}

static CCefSystem s_CEFSystem;

CCefSystem& CEFSystem()
{
	return s_CEFSystem;
}
