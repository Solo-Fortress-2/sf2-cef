/*
 * Copyright (c) 2025 The Solo Fortress 2 Team, all rights reserved.
 * cef_browser.cpp, Main CEF browser. Use this to render the browser in your VGUI panels.
 *
 * Licensed under CC BY-NC 3.0 (Lambda Wars' original license)
 */

#include "cbase.h"
#include "cef_browser.h"
#include "cef_system.h"
#include "cef_js.h"

#ifdef ShellExecute
#undef ShellExecute
#endif

#include "inputsystem/iinputsystem.h"
#include <vgui/VGUI.h>
#include <vgui/IInput.h>
#include "materialsystem/materialsystem_config.h"
#include "steam/steam_api.h"
#include "vgui/ISystem.h"

// CEF includes
#include "cef_cxx20_stubs.h"
#include "include/wrapper/cef_helpers.h"
#include "include/cef_task.h"
#include "include/base/cef_bind.h"
#include "include/base/cef_callback.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

typedef void(*CefTaskCallback)(void* pUserData);

class CCefBoundTask : public CefTask
{
public:
	CCefBoundTask(CefTaskCallback pFn, void* pData)
		: m_pFn(pFn)
		, m_pData(pData)
	{
	}

	virtual void Execute() OVERRIDE
	{
		if (m_pFn)
			m_pFn(m_pData);
	}

private:
	CefTaskCallback m_pFn;
	void* m_pData;

	IMPLEMENT_REFCOUNTING(CCefBoundTask);
};

//-----------------------------------------------------------------------------
// Purpose: Small helper function for opening urls
//-----------------------------------------------------------------------------
static void OpenURL(const char* url)
{
	// Open url in steam browser if fullscreen
	// If windowed, just do a shell execute so it executes in the default browser
	const MaterialSystem_Config_t& config = materials->GetCurrentConfigForVideoCard();
	if (config.Windowed())
	{
		vgui::system()->ShellExecute("open", url);
	}
	else
	{
		if (!steamapicontext || !steamapicontext->SteamFriends())
		{
			Warning("OpenURL: could not get steam api context!\n");
			return;
		}
		steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage(url);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CefClientHandler::CefClientHandler(CCefBrowser* pSrcBrowser, CefNavigationType navigationbehavior, const char* pDebugName) :
	m_BrowserId(0), m_pSrcBrowser(pSrcBrowser), m_NavigationBehavior(navigationbehavior), m_DebugName(pDebugName),
	m_OSRHandler(nullptr), m_bInitialized(false), m_fLastPingTime(-1)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CefClientHandler::Destroy()
{
	Assert(!CefCurrentlyOn(TID_UI));

	if (GetOSRHandler())
	{
		GetOSRHandler()->Destroy();
	}

	if (GetBrowser() && GetBrowser()->GetHost())
	{
		GetBrowser()->GetHost()->CloseBrowser(true);
	}
	m_pSrcBrowser = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CefClientHandler::DoClose(CefRefPtr<CefBrowser> browser)
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CefClientHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
	Assert(CefCurrentlyOn(TID_UI));
	m_Browser = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CefClientHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message)
{
	if (!m_pSrcBrowser)
		return false;

	if (message->GetName() == "pong")
	{
		DevMsg("Received PONG from render process of browser %d!\n", browser->GetIdentifier());
		m_fLastPingTime = Plat_FloatTime();
		return true;
	}
	else if (message->GetName() == "methodcall")
	{
		CefRefPtr<CefListValue> args = message->GetArgumentList();
#ifdef USE_MULTITHREADED_MESSAGELOOP
		AddMessage(MT_METHODCALL, frame, args->Copy());
#else
		CefString identifier = args->GetString(0);
		CefRefPtr<CefListValue> methodargs = args->GetList(1);

		if (args->GetType(2) == VTYPE_NULL)
		{
			m_pSrcBrowser->OnMethodCall(identifier, methodargs);
		}
		else
		{
			int iCallbackID = args->GetInt(2);
			m_pSrcBrowser->OnMethodCall(identifier, methodargs, &iCallbackID);
		}
#endif // USE_MULTITHREADED_MESSAGELOOP
		return true;
	}
	else if (message->GetName() == "oncontextcreated")
	{
#ifdef USE_MULTITHREADED_MESSAGELOOP
		AddMessage(MT_CONTEXTCREATED, frame, nullptr);
#else
		m_pSrcBrowser->OnContextCreated();
#endif // USE_MULTITHREADED_MESSAGELOOP
	}
	else if (message->GetName() == "openurl")
	{
		CefRefPtr<CefListValue> args = message->GetArgumentList();
		OpenURL(args->GetString(0).ToString().c_str());
	}
	else if (message->GetName() == "msg")
	{
		CefRefPtr<CefListValue> args = message->GetArgumentList();
		Msg("Browser %d Render Process: %ls", browser->GetIdentifier(), args->GetString(0).c_str());
		return true;
	}
	else if (message->GetName() == "warning")
	{
		CefRefPtr<CefListValue> args = message->GetArgumentList();
		Warning("Browser %d Render Process: %ls", browser->GetIdentifier(), args->GetString(0).c_str());
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CefClientHandler::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefContextMenuParams> params,
	CefRefPtr<CefMenuModel> model)
{
	// Always clear context menus
	model->Clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CefClientHandler::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
	cef_log_severity_t level,
	const CefString& message,
	const CefString& source,
	int line)
{
	switch (level)
	{
	case cef_log_severity_t::LOGSEVERITY_ERROR:
	case cef_log_severity_t::LOGSEVERITY_FATAL:
		ConColorMsg(Color(255, 20, 20, 255), "%d %ls: %ls\n", line, source.c_str(), message.c_str());
		return true;
	case cef_log_severity_t::LOGSEVERITY_WARNING:
	case cef_log_severity_t::LOGSEVERITY_DEBUG:
		ConColorMsg(Color(255, 210, 10, 255), "%d %ls: %ls\n", line, source.c_str(), message.c_str());
		return true;
	default:
		ConColorMsg(Color(20, 20, 255, 255), "%d %ls: %ls\n", line, source.c_str(), message.c_str());
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CefClientHandler::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
	TerminationStatus status)
{
	Warning("Browser %d render process crashed with status code %d!\n", browser->GetIdentifier(), status);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CefClientHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
	CEF_REQUIRE_UI_THREAD();

	if (!m_pSrcBrowser)
		return;

	DevMsg("#%d %s: CCefBrowser::OnAfterCreated\n", browser->GetIdentifier(), m_DebugName.ToString().c_str());

	if (!m_Browser.get())
	{
		// We need to keep the main child window, but not popup windows
		m_Browser = browser;
		m_BrowserId = browser->GetIdentifier();
	}

	m_bInitialized = true;
	m_pSrcBrowser->Ping();

#ifdef USE_MULTITHREADED_MESSAGELOOP
	AddMessage(MT_AFTERCREATED, browser->GetMainFrame(), nullptr);
#else
	m_pSrcBrowser->OnAfterCreated();
#endif // USE_MULTITHREADED_MESSAGELOOP

	browser->GetHost()->WasResized();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CefClientHandler::OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
	TransitionType transition_type)
{
	if (!m_pSrcBrowser)
		return;

	DevMsg("#%d %s: CCefBrowser::OnLoadStart\n", browser->GetIdentifier(), m_DebugName.ToString().c_str());

#ifdef USE_MULTITHREADED_MESSAGELOOP
	AddMessage(MT_LOADSTART, frame, nullptr);
#else
	m_pSrcBrowser->OnLoadStart(frame);
#endif // USE_MULTITHREADED_MESSAGELOOP
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CefClientHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode)
{
	if (!m_pSrcBrowser)
		return;

	DevMsg("#%d %s: CCefBrowser::OnLoadEnd\n", browser->GetIdentifier(), m_DebugName.ToString().c_str());

#ifdef USE_MULTITHREADED_MESSAGELOOP
	CefRefPtr<CefListValue> data = CefListValue::Create();
	data->SetInt(0, httpStatusCode);
	AddMessage(MT_LOADEND, frame, data);
#else
	m_pSrcBrowser->OnLoadEnd(frame, httpStatusCode);
#endif // USE_MULTITHREADED_MESSAGELOOP
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CefClientHandler::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode,
	const CefString& errorText, const CefString& failedUrl)
{
#ifdef USE_MULTITHREADED_MESSAGELOOP
	CefRefPtr<CefListValue> data = CefListValue::Create();
	data->SetInt(0, errorCode);
	data->SetString(1, errorText);
	data->SetString(2, failedUrl);
	AddMessage(MT_LOADERROR, frame, data);
#else
	if (!m_pSrcBrowser)
		return;
	m_pSrcBrowser->OnLoadError(frame, errorCode, errorText.ToWString().c_str(), failedUrl.ToWString().c_str());
#endif // 0
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CefClientHandler::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
	bool isLoading,
	bool canGoBack,
	bool canGoForward)
{
#ifdef USE_MULTITHREADED_MESSAGELOOP
	CefRefPtr<CefListValue> data = CefListValue::Create();
	data->SetBool(0, isLoading);
	data->SetBool(1, canGoBack);
	data->SetBool(2, canGoForward);
	AddMessage(MT_LOADINGSTATECHANGE, nullptr, data);
#else
	if (!m_pSrcBrowser)
		return;
	m_pSrcBrowser->OnLoadingStateChange(isLoading, canGoBack, canGoForward);
#endif // USE_MULTITHREADED_MESSAGELOOP

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CefClientHandler::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request,
	bool is_redirect)
{
	Assert(CefCurrentlyOn(TID_UI));

	// Default behavior: allow navigating away
	bool bDenyNavigation = false;

	// Always allow sub frames to navigate to anything
	if (frame->IsMain())
	{
		// Always allow dev tools urls
		std::string url = request->GetURL().ToString();
		static std::string chromedevtoolspro("chrome-devtools://");
		if (url.compare(0, chromedevtoolspro.size(), chromedevtoolspro) != 0)
		{
			if (m_NavigationBehavior == CefNavigationType::NT_PREVENTALL)
			{
				// This mode prevents from navigating away from the current page
				if (browser->GetMainFrame()->GetURL() != request->GetURL())
					bDenyNavigation = true;
			}
			else if (m_NavigationBehavior == CefNavigationType::NT_ONLYFILEPROT)
			{
				// This mode only allows navigating to urls starting with the file or custom local protocol
				static std::string filepro("file://");
				static std::string localpro("local:");

				if (url.compare(0, filepro.size(), filepro) && url.compare(0, localpro.size(), localpro))
					bDenyNavigation = true;
			}
		}

		// If we don't allow navigation, open the url in a new window
		if (bDenyNavigation)
		{
#ifdef USE_MULTITHREADED_MESSAGELOOP
			CefRefPtr<CefListValue> data = CefListValue::Create();
			data->SetString(0, request->GetURL());
			AddMessage(MT_OPENURL, nullptr, data);
#else
			OpenURL(request->GetURL().ToString().c_str());
#endif // USE_MULTITHREADED_MESSAGELOOP
		}
	}

	DevMsg("#%d %s: CCefBrowser::OnBeforeBrowse %s. Deny: %d\n",
		browser->GetIdentifier(), m_DebugName.ToString().c_str(),
		request->GetURL().ToString().c_str(), bDenyNavigation);
	return bDenyNavigation;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CefClientHandler::OnResourceRedirect(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	const CefString& old_url,
	CefString& new_url)
{
}

#ifdef USE_MULTITHREADED_MESSAGELOOP
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CefClientHandler::AddMessage(messageType_e type, CefRefPtr<CefFrame> frame, CefRefPtr<CefListValue> data)
{
	AUTO_LOCK(m_MessageQueueMutex);
	m_messageQueue.AddToTail(messageData_t(type, frame, data));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CefClientHandler::ProcessMessages()
{
	if (m_messageQueue.Count() == 0)
	{
		return;
	}

	m_MessageQueueMutex.Lock();
	CUtlVector< messageData_t > messageQueue(0, m_messageQueue.Count());
	messageQueue.CopyArray(m_messageQueue.Base(), m_messageQueue.Count());
	m_messageQueue.Purge();
	m_MessageQueueMutex.Unlock();

	for (int i = 0; i < messageQueue.Count(); i++)
	{
		messageData_t& data = messageQueue[i];
		CefString identifier;
		CefRefPtr<CefListValue> methodargs;

		switch (data.type)
		{
		case MT_LOADSTART:
			m_pSrcBrowser->OnLoadStart(data.frame);
			break;
		case MT_LOADEND:
			m_pSrcBrowser->OnLoadEnd(data.frame, data.data->GetInt(0));
			break;
		case MT_LOADERROR:
			m_pSrcBrowser->OnLoadError(data.frame, data.data->GetInt(0), data.data->GetString(1).ToWString().c_str(), data.data->GetString(2).ToWString().c_str());
			break;
		case MT_LOADINGSTATECHANGE:
			m_pSrcBrowser->OnLoadingStateChange(data.data->GetBool(0), data.data->GetBool(1), data.data->GetBool(2));
			break;
		case MT_AFTERCREATED:
			m_pSrcBrowser->OnAfterCreated();
			break;
		case MT_CONTEXTCREATED:
			m_pSrcBrowser->OnContextCreated();
			break;
		case MT_METHODCALL:
			identifier = data.data->GetString(0);
			methodargs = data.data->GetList(1);

			if (data.data->GetType(2) == VTYPE_NULL)
			{
				m_pSrcBrowser->OnMethodCall(identifier, methodargs);
			}
			else
			{
				int iCallbackID = data.data->GetInt(2);
				m_pSrcBrowser->OnMethodCall(identifier, methodargs, &iCallbackID);
			}
			break;
		case MT_OPENURL:
			OpenURL(data.data->GetString(0).ToString().c_str());
			break;
		default:
			break;
		}
	}
}
#endif // USE_MULTITHREADED_MESSAGELOOP

//-----------------------------------------------------------------------------
// Purpose: DevTools don't use off screen rendering, so separate client handler.
//-----------------------------------------------------------------------------
class CefClientDevToolsHandler : public CefClient, CefDisplayHandler
{
public:
	virtual bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
		cef_log_severity_t level,
		const CefString& message,
		const CefString& source,
		int line)
	{
		switch (level)
		{
		case cef_log_severity_t::LOGSEVERITY_ERROR:
		case cef_log_severity_t::LOGSEVERITY_FATAL:
			ConColorMsg(Color(255, 20, 20, 255), "%d %ls: %ls\n", line, source.c_str(), message.c_str());
			return true;
		case cef_log_severity_t::LOGSEVERITY_WARNING:
		case cef_log_severity_t::LOGSEVERITY_DEBUG:
			ConColorMsg(Color(255, 210, 10, 255), "%d %ls: %ls\n", line, source.c_str(), message.c_str());
			return true;
		default:
			ConColorMsg(Color(20, 20, 255, 255), "%d %ls: %ls\n", line, source.c_str(), message.c_str());
			return true;
		}
		return false;
	}

private:
	IMPLEMENT_REFCOUNTING(CefClientDevToolsHandler);

};

//-----------------------------------------------------------------------------
// Purpose: Cef browser
//-----------------------------------------------------------------------------
CCefBrowser::CCefBrowser(const char* name, const char* pURL, int renderFrameRate, int wide, int tall, CefNavigationType navigationbehavior) :
	m_bPerformLayout(true), m_bVisible(false), m_pPanel(NULL),
	m_bGameInputEnabled(false), m_bUseMouseCapture(false), m_bPassMouseTruIfAlphaZero(false), m_bHasFocus(false), m_CefClientHandler(nullptr),
	m_fLastTriedPingTime(-1), m_bInitializePingSuccessful(false), m_bWasHidden(false), m_bIgnoreTabKey(false), m_fLastLoadStartTime(0)
{
	m_Name = name ? name : "UnknownCefBrowser";

	// Create panel and texture generator
	m_pPanel = new CCefVGUIPanel(name, this, NULL);

	// Have the option to set the initial width and tall here, so CreateBrowser gets the correct view rectangle
	if (wide > 0 && tall > 0)
	{
		m_pPanel->SetSize(wide, tall);
	}

	// Initialize browser
	CEFSystem().AddBrowser(this);

	if (!CEFSystem().IsRunning())
	{
		Warning("CEFSystem not running, not creating browser\n");
		return;
	}

	m_URL = pURL ? pURL : "";
	m_CefClientHandler = new CefClientHandler(this, navigationbehavior, name);

	CefWindowInfo info;
	info.SetAsWindowless( /*CEFSystem().GetMainWindow()*/ NULL);

	m_CefClientHandler->SetOSRHandler(new CCefOSRRenderer(this, true));

	// Browser settings
	CefBrowserSettings settings;
	settings.windowless_frame_rate = renderFrameRate;
	CefString(&settings.default_encoding) = CefString("UTF-8");

	// Creat the new child browser window
	DevMsg("%s: CefBrowserHost::CreateBrowser\n", m_Name.c_str());

	m_fBrowserCreateTime = Plat_FloatTime();
	if (!CefBrowserHost::CreateBrowser(info, m_CefClientHandler, m_URL, settings, nullptr, nullptr))
	{
		Warning(" Failed to create CEF browser %s\n", name);
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCefBrowser::~CCefBrowser()
{
	// Remove ourself from the list
	CEFSystem().RemoveBrowser(this);

	Destroy();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::Destroy(void)
{
	CloseDevTools();

	// OSR thread could be receiving a new paint buffer, during which it marks
	// the vgui panel as dirty.
	AUTO_LOCK(CCefOSRRenderer::GetTextureBufferMutex());

	// Delete panel
	if (m_pPanel)
	{
		m_pPanel->DeletePanel();
		m_pPanel = NULL;
	}

	// Close browser
	if (m_CefClientHandler)
	{
		OnDestroy();

		DevMsg("#%d %s: CCefBrowser::Destroy\n", GetBrowser() ? GetBrowser()->GetIdentifier() : -1, m_Name.c_str());

		m_CefClientHandler->Destroy();
		m_CefClientHandler = nullptr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCefBrowser::IsValid(void)
{
	return m_CefClientHandler && m_CefClientHandler->GetBrowser();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
vgui::VPANEL CCefBrowser::GetVPanel()
{
	return m_pPanel ? m_pPanel->GetVPanel() : 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CefRefPtr<CCefOSRRenderer> CCefBrowser::GetOSRHandler()
{
	return m_CefClientHandler ? m_CefClientHandler->GetOSRHandler() : nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CefRefPtr<CefBrowser> CCefBrowser::GetBrowser()
{
	return m_CefClientHandler ? m_CefClientHandler->GetBrowser() : nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::Think(void)
{
#ifdef USE_MULTITHREADED_MESSAGELOOP
	m_CefClientHandler->ProcessMessages();
#endif // USE_MULTITHREADED_MESSAGELOOP

	if (m_bPerformLayout)
	{
		PerformLayout();
		m_CefClientHandler->GetBrowser()->GetHost()->WasResized();

		m_bPerformLayout = false;
	}

	// Tell CEF not to paint if panel is hidden
	bool bFullyVisible = GetPanel()->IsVisible();
	if (bFullyVisible && m_bWasHidden)
	{
		WasHidden(false);
		m_bWasHidden = false;
	}
	else if (!bFullyVisible && !m_bWasHidden)
	{
		WasHidden(true);
		m_bWasHidden = true;
	}

	if (!m_bInitializePingSuccessful)
	{
		if (!m_CefClientHandler->IsInitialized() || m_CefClientHandler->GetLastPingTime() == -1)
		{
			float fLifeTime = Plat_FloatTime() - m_fBrowserCreateTime;
			if (fLifeTime > 60.0f)
			{
				if (m_CefClientHandler->GetLastPingTime() == -1)
				{
					Error("Could not launch browser helper process. \n\n"
						"This is usually caused by security software blocking the process from launching. "
						"Please check your security software and unblock \"sf2_cef_subprocess.exe\". \n\n"
						"Your security software might also temporary block the process for scanning. "
						"In this case just relaunch the game. "
					);
				}
				else
				{
					Error("Creating web view failed\n\n"
						"Contact a developer in case this happens"
					);
				}
			}
			else if (Plat_FloatTime() - m_fLastTriedPingTime > 1.0f)
			{
				Ping();
			}
		}
		else
		{
			m_bInitializePingSuccessful = true;
			DevMsg("#%d %s: Browser creation time: %f\n", GetBrowser() ? GetBrowser()->GetIdentifier() : -1, m_Name.c_str(), Plat_FloatTime() - m_fBrowserCreateTime);
		}
	}

	vgui::VPANEL focus = vgui::input()->GetFocus();
	vgui::Panel* pPanel = GetPanel();
	if (pPanel->IsVisible() && focus == 0)
	{
		if (!m_bHasFocus)
		{
			m_bHasFocus = true;
			GetBrowser()->GetHost()->SetFocus(m_bHasFocus);
		}
	}
	else
	{
		if (m_bHasFocus)
		{
			m_bHasFocus = false;
			GetBrowser()->GetHost()->SetFocus(m_bHasFocus);
		}
	}

	OnThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::OnAfterCreated(void)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::OnContextCreated()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::OnLoadStart(CefRefPtr<CefFrame> frame)
{
	m_fLastLoadStartTime = Plat_FloatTime();

#ifdef ENABLE_PYTHON
	if (!SrcPySystem()->IsPythonRunning())
		return;

	try
	{
		PyOnLoadStart(boost::python::object(PyCefFrame(frame)));
	}
	catch (boost::python::error_already_set&)
	{
		PyErr_Print();
	}
#endif // ENABLE_PYTHON
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::OnLoadEnd(CefRefPtr<CefFrame> frame, int httpStatusCode)
{
#ifdef ENABLE_PYTHON
	if (!SrcPySystem()->IsPythonRunning())
		return;

	try
	{
		PyOnLoadEnd(boost::python::object(PyCefFrame(frame)), httpStatusCode);
	}
	catch (boost::python::error_already_set&)
	{
		PyErr_Print();
	}
#endif // ENABLE_PYTHON
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::OnLoadError(CefRefPtr<CefFrame> frame, int errorCode, const wchar_t* errorText, const wchar_t* failedUrl)
{
#ifdef ENABLE_PYTHON
	if (!SrcPySystem()->IsPythonRunning())
		return;

	try
	{
		// Convert error texts to Python unicode objects
		boost::python::object pyErrorText;
		if (errorText)
		{
			PyObject* pPyErrorText = PyUnicode_FromWideChar(errorText, wcslen(errorText));
			if (pPyErrorText)
				pyErrorText = boost::python::object(boost::python::handle<>(pPyErrorText));
		}
		boost::python::object pyFailedURL;
		if (failedUrl)
		{
			PyObject* pPyFailedURL = PyUnicode_FromWideChar(failedUrl, wcslen(failedUrl));
			if (pPyFailedURL)
				pyFailedURL = boost::python::object(boost::python::handle<>(pPyFailedURL));
		}

		PyOnLoadError(boost::python::object(PyCefFrame(frame)), errorCode, pyErrorText, pyFailedURL);
	}
	catch (boost::python::error_already_set&)
	{
		PyErr_Print();
	}
#endif // ENABLE_PYTHON
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::PerformLayout(void)
{

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::InvalidateLayout(void)
{
	m_bPerformLayout = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::NotifyScreenInfoChanged()
{
	if (!IsValid())
		return;
	m_CefClientHandler->GetBrowser()->GetHost()->NotifyScreenInfoChanged();
}

// Usage functions
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCefBrowser::SetSize(int wide, int tall)
{
	if (!IsValid())
		return;

	m_pPanel->SetSize(wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCefBrowser::SetPos(int x, int y)
{
	if (!IsValid())
		return;

	m_pPanel->SetPos(x, y);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCefBrowser::SetZPos(int z)
{
	if (!IsValid())
		return;

	m_pPanel->SetZPos(z);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCefBrowser::SetVisible(bool state)
{
	if (!IsValid())
		return;

	if (m_bVisible == state)
		return;

	m_bVisible = state;
	m_pPanel->SetVisible(state);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCefBrowser::IsVisible()
{
	if (!IsValid())
		return false;

	return m_pPanel->IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCefBrowser::IsFullyVisible()
{
	if (!IsValid())
		return false;

	return m_pPanel->IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCefBrowser::SetMouseInputEnabled(bool state)
{
	if (!IsValid())
	{
		Warning("#%d %s: CCefBrowser::SetMouseInputEnabled: browser not valid yet!\n", GetBrowser() ? GetBrowser()->GetIdentifier() : -1, m_Name.c_str());
		return;
	}

	m_pPanel->SetMouseInputEnabled(state);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCefBrowser::SetKeyBoardInputEnabled(bool state)
{
	if (!IsValid())
	{
		Warning("#%d %s: CCefBrowser::SetKeyBoardInputEnabled: browser not valid yet!\n", GetBrowser() ? GetBrowser()->GetIdentifier() : -1, m_Name.c_str());
		return;
	}

	m_pPanel->SetKeyBoardInputEnabled(state);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCefBrowser::IsMouseInputEnabled()
{
	if (!IsValid())
		return false;

	return m_pPanel->IsMouseInputEnabled();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCefBrowser::IsKeyBoardInputEnabled()
{
	if (!IsValid())
		return false;

	return m_pPanel->IsKeyBoardInputEnabled();
}

//-----------------------------------------------------------------------------
// Purpose: For overriding by implementations
//-----------------------------------------------------------------------------
int CCefBrowser::KeyInput(int down, ButtonCode_t keynum, const char* pszCurrentBinding)
{
	if (!IsGameInputEnabled())
		return 1;

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCefBrowser::SetCursor(vgui::HCursor cursor)
{
	if (!IsValid())
		return;

	return m_pPanel->SetCursor(cursor);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
vgui::HCursor CCefBrowser::GetCursor()
{
	if (!IsValid())
		return 0;

	return m_pPanel->GetCursor();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CCefBrowser::GetAlphaAt(int x, int y)
{
	if (!IsValid())
		return 0;

	if (GetOSRHandler())
		return GetOSRHandler()->GetAlphaAt(x, y);
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCefBrowser::IsLoading(void)
{
	if (!IsValid())
		return false;

	return m_CefClientHandler->GetBrowser()->IsLoading();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCefBrowser::Reload(void)
{
	if (!IsValid())
		return;

	m_CefClientHandler->GetBrowser()->Reload();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCefBrowser::ReloadIgnoreCache(void)
{
	if (!IsValid())
		return;

	m_CefClientHandler->GetBrowser()->ReloadIgnoreCache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCefBrowser::StopLoad(void)
{
	if (!IsValid())
		return;

	m_CefClientHandler->GetBrowser()->StopLoad();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::Focus()
{
	if (!IsValid())
		return;
	m_CefClientHandler->GetBrowser()->GetHost()->SetFocus(true);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::Unfocus()
{
	if (!IsValid())
		return;
	m_CefClientHandler->GetBrowser()->GetHost()->SetFocus(false);
}

//-----------------------------------------------------------------------------
// Purpose: Load an URL
//-----------------------------------------------------------------------------
void CCefBrowser::LoadURL(const char* url)
{
	if (!IsValid())
		return;

	CefRefPtr<CefFrame> mainFrame = m_CefClientHandler->GetBrowser()->GetMainFrame();
	if (!mainFrame)
		return;

	mainFrame->LoadURL(CefString(url));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char* CCefBrowser::GetURL()
{
	if (!IsValid())
		return "";

	CefRefPtr<CefFrame> mainFrame = m_CefClientHandler->GetBrowser()->GetMainFrame();
	if (!mainFrame)
		return "";

	static std::string s_url;
	s_url = mainFrame->GetURL().ToString(); // ugly
	return s_url.c_str();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::WasHidden(bool hidden)
{
	if (!IsValid())
		return;

	m_CefClientHandler->GetBrowser()->GetHost()->WasHidden(hidden);
}

//-----------------------------------------------------------------------------
// Purpose: Execute javascript code
//-----------------------------------------------------------------------------
void CCefBrowser::ExecuteJavaScript(const char* code, const char* script_url, int start_line)
{
	if (!IsValid())
		return;

	CefRefPtr<CefFrame> mainFrame = m_CefClientHandler->GetBrowser()->GetMainFrame();
	if (!mainFrame)
		return;

	mainFrame->ExecuteJavaScript(code, script_url, start_line);
}

//-----------------------------------------------------------------------------
// Purpose: Execute javascript code with reference to result
//-----------------------------------------------------------------------------
CefRefPtr<JSObject> CCefBrowser::ExecuteJavaScriptWithResult(const char* code, const char* script_url, int start_line)
{
	if (!IsValid())
		return nullptr;

	CefRefPtr<CefFrame> mainFrame = m_CefClientHandler->GetBrowser()->GetMainFrame();
	if (!mainFrame) return nullptr;

	CefRefPtr<JSObject> jsObject = new JSObject();

	CefRefPtr<CefProcessMessage> message =
		CefProcessMessage::Create("calljswithresult");
	CefRefPtr<CefListValue> args = message->GetArgumentList();
	args->SetString(0, jsObject->GetIdentifier());
	args->SetString(1, code);

	mainFrame->SendProcessMessage(PID_RENDERER, message);

	return jsObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CefRefPtr<JSObject> CCefBrowser::CreateGlobalObject(const char* name)
{
	if (!IsValid())
		return nullptr;

	CefRefPtr<CefFrame> mainFrame = m_CefClientHandler->GetBrowser()->GetMainFrame();
	if (!mainFrame) return nullptr;

	CefRefPtr<JSObject> jsObject = new JSObject(name);

	CefRefPtr<CefProcessMessage> message =
		CefProcessMessage::Create("createglobalobject");
	CefRefPtr<CefListValue> args = message->GetArgumentList();
	args->SetString(0, jsObject->GetIdentifier());
	args->SetString(1, name);

	mainFrame->SendProcessMessage(PID_RENDERER, message);

	return jsObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CefRefPtr<JSObject> CCefBrowser::CreateFunction(const char* name, CefRefPtr<JSObject> object, bool bHasCallback)
{
	if (!IsValid())
		return nullptr;

	CefRefPtr<CefFrame> mainFrame = m_CefClientHandler->GetBrowser()->GetMainFrame();
	if (!mainFrame) return nullptr;

	CefRefPtr<JSObject> jsObject = new JSObject(name);

	CefRefPtr<CefProcessMessage> message =
		CefProcessMessage::Create(!bHasCallback ? "createfunction" : "createfunctionwithcallback");
	CefRefPtr<CefListValue> args = message->GetArgumentList();
	args->SetString(0, jsObject->GetIdentifier());
	args->SetString(1, name);
	if (object)
		args->SetString(2, object->GetIdentifier());
	else
		args->SetNull(2);

	mainFrame->SendProcessMessage(PID_RENDERER, message);

	return jsObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::SendCallback(int* pCallbackID, CefRefPtr<CefListValue> methodargs)
{
	if (!IsValid())
		return;

	CefRefPtr<CefFrame> mainFrame = m_CefClientHandler->GetBrowser()->GetMainFrame();
	if (!mainFrame) return;

	if (!pCallbackID)
	{
		Warning("SendCallback: no callback specified\n");
		return;
	}

	CefRefPtr<CefProcessMessage> message =
		CefProcessMessage::Create("callbackmethod");
	CefRefPtr<CefListValue> args = message->GetArgumentList();
	args->SetInt(0, *pCallbackID);
	args->SetList(1, methodargs);

	mainFrame->SendProcessMessage(PID_RENDERER, message);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::Invoke(CefRefPtr<JSObject> object, const char* methodname, CefRefPtr<CefListValue> methodargs)
{
	if (!IsValid())
		return;

	CefRefPtr<CefFrame> mainFrame = GetBrowser()->GetMainFrame();
	if (!mainFrame) return;

	CefRefPtr<CefProcessMessage> message =
		CefProcessMessage::Create("invoke");
	CefRefPtr<CefListValue> args = message->GetArgumentList();
	args->SetString(0, object ? object->GetIdentifier() : "");
	args->SetString(1, methodname);
	args->SetList(2, methodargs);

	mainFrame->SendProcessMessage(PID_RENDERER, message);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CefRefPtr<JSObject> CCefBrowser::InvokeWithResult(CefRefPtr<JSObject> object, const char* methodname, CefRefPtr<CefListValue> methodargs)
{
	if (!IsValid())
		return nullptr;

	CefRefPtr<CefFrame> mainFrame = GetBrowser()->GetMainFrame();
	if (!mainFrame) return nullptr;

	CefRefPtr<JSObject> jsResultObject = new JSObject();

	CefRefPtr<CefProcessMessage> message =
		CefProcessMessage::Create("invokewithresult");
	CefRefPtr<CefListValue> args = message->GetArgumentList();
	args->SetString(0, jsResultObject->GetIdentifier());
	args->SetString(1, object ? object->GetIdentifier() : "");
	args->SetString(2, methodname);
	args->SetList(3, methodargs);

	mainFrame->SendProcessMessage(PID_RENDERER, message);

	return jsResultObject;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::OnMethodCall(CefString identifier, CefRefPtr<CefListValue> methodargs, int* pCallbackID)
{
	DevMsg("%d %s: CCefBrowser::OnMethodCall\n", GetBrowser()->GetIdentifier(), GetName());
#ifdef ENABLE_PYTHON
	if (!SrcPySystem()->IsPythonRunning())
		return;

	try
	{
		PyOnMethodCall(boost::python::object(identifier.ToString().c_str()), CefValueListToPy(methodargs), pCallbackID ? boost::python::object(*pCallbackID) : boost::python::object());
	}
	catch (boost::python::error_already_set&)
	{
		PyErr_Print();
	}
#endif // ENABLE_PYTHON
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::Ping()
{
	if (!IsValid())
		return;

	CefRefPtr<CefBrowser> browser = GetBrowser();
	if (!browser || browser->IsLoading() || browser->HasDocument() == false) 
	{
		DevWarning("Failed to send ping message to renderer process for #%d %s (invalid instance or document)\n",
			browser->GetIdentifier(), GetName());
		return;
	}

	CefRefPtr<CefFrame> mainFrame = browser->GetMainFrame();
	if (!mainFrame)
	{
		DevWarning("Failed to send ping message to renderer process for #%d %s (renderer not alive?)\n",
			browser->GetIdentifier(), GetName());
		return;
	}
	
	CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create("ping");
	mainFrame->SendProcessMessage(PID_RENDERER, message);

	m_fLastTriedPingTime = Plat_FloatTime();

	DevMsg("#%d %s: CCefBrowser::Ping\n", browser->GetIdentifier(), GetName());
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::ShowDevTools()
{
	if (!IsValid())
		return;

	CefWindowInfo windowInfo;
	CefBrowserSettings settings;

#if defined(WIN32)
	windowInfo.SetAsPopup(GetBrowser()->GetHost()->GetWindowHandle(), "Chromium DevTools - Solo Fortress 2");
#endif

	CefRefPtr<CefClientDevToolsHandler> cefClientDevToolsHandler = new CefClientDevToolsHandler();

	GetBrowser()->GetHost()->ShowDevTools(windowInfo, cefClientDevToolsHandler, settings, CefPoint());
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefBrowser::CloseDevTools()
{
	if (!IsValid())
		return;

	GetBrowser()->GetHost()->CloseDevTools();
}