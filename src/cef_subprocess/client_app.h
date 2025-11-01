/*
 * Copyright (c) 2025 The Solo Fortress 2 Team, all rights reserved.
 * client_app.h, Main CEF application of the sub-process.
 *
 * Licensed under CC BY-NC 3.0 (Lambda Wars' original license)
 */

#ifndef CLIENT_APP_H
#define CLIENT_APP_H
#ifdef _WIN32
#pragma once
#endif

#include "cef_cxx20_stubs.h"
#include "include/cef_app.h"
#include "render_browser.h"
#include "utlvector.h"

// Forward declarations
class CefBrowser;

// Client app
class ClientApp : public CefApp,
				  public CefRenderProcessHandler
{
public:
	virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() {
		return this;
	}

	// CefApp
	virtual void OnBeforeCommandLineProcessing( const CefString& process_type, CefRefPtr<CefCommandLine> command_line ) override;

	virtual void OnRegisterCustomSchemes( CefRawPtr<CefSchemeRegistrar> registrar ) override;

	// Messages from/to main process
	virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
										CefRefPtr<CefFrame> frame,
										CefProcessId source_process,
										CefRefPtr<CefProcessMessage> message) override;

	// Browser
	virtual void OnBrowserCreated(CefRefPtr<CefBrowser> browser,
								CefRefPtr<CefDictionaryValue> extra_info) override;

	virtual void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) override;

	CefRefPtr<RenderBrowser> FindBrowser( CefRefPtr<CefBrowser> browser );

	// Context
	virtual void OnContextCreated(CefRefPtr<CefBrowser> browser,
								CefRefPtr<CefFrame> frame,
								CefRefPtr<CefV8Context> context) override;

	virtual void OnContextReleased(CefRefPtr<CefBrowser> browser,
									CefRefPtr<CefFrame> frame,
									CefRefPtr<CefV8Context> context) override;

	// Message functions
	virtual void SendMsg( CefRefPtr<CefBrowser> browser, const char *pMsg, ... );

	virtual void SendWarning( CefRefPtr<CefBrowser> browser, const char *pMsg, ... );

private:
	CUtlVector< CefRefPtr<RenderBrowser> > m_Browsers;

	IMPLEMENT_REFCOUNTING( ClientApp );
};

#endif // CLIENT_APP_H