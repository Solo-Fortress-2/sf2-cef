/*
 * Copyright (c) 2025 The Solo Fortress 2 Team, all rights reserved.
 * main.cpp, Sub-process entry point.
 *
 * Licensed under CC BY-NC 3.0 (Lambda Wars' original license)
 */

#include "client_app.h"
#include "cef_cxx20_stubs.h"
#include "include/cef_app.h"
#include "include/cef_client.h"

#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	HINSTANCE hinst = (HINSTANCE)GetModuleHandle(NULL);
	CefMainArgs main_args(hinst);

	CefRefPtr<CefApp> app(new ClientApp);

	// Execute the secondary process.
	void* sandbox_info = nullptr;

#if CEF_ENABLE_SANDBOX
	// Manage the life span of the sandbox information object. This is necessary
	// for sandbox support on Windows. See cef_sandbox_win.h for complete details.
	CefScopedSandboxInfo scoped_sandbox;
	sandbox_info = scoped_sandbox.sandbox_info();
#endif

	return CefExecuteProcess(main_args, app.get(), sandbox_info);
}
#else
int main(int argc, char* argv[]) 
{
    // Structure for passing command-line arguments.
    // The definition of this structure is platform-specific.
    CefMainArgs main_args(argc, argv);

    // Implementation of the CefApp interface.
    CefRefPtr<ClientApp> app(new ClientApp);

	// Execute the secondary process.
	void* sandbox_info = nullptr;

#if CEF_ENABLE_SANDBOX
	// Manage the life span of the sandbox information object. This is necessary
	// for sandbox support on Windows. See cef_sandbox_win.h for complete details.
	CefScopedSandboxInfo scoped_sandbox;
	sandbox_info = scoped_sandbox.sandbox_info();
#endif

    // Execute the sub-process logic. This will block until the sub-process should exit.
    return CefExecuteProcess(main_args, app.get(), sandbox_info);
}
#endif