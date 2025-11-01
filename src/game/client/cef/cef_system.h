/*
 * Copyright (c) 2025 The Solo Fortress 2 Team, all rights reserved.
 * cef_system.h, Initializes and configures Chromium Embedded Framework.
 *
 * Licensed under CC BY-NC 3.0 (Lambda Wars' original license)
 */

#ifndef CEF_SYSTEM_H
#define CEF_SYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"

#include "cef_cxx20_stubs.h"
#include "cef_browser.h"
#include "cef_js.h"
#include "cef_vgui_panel.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"

class CCefSystem : public CAutoGameSystemPerFrame
{
public:
	CCefSystem();
	virtual ~CCefSystem();

	virtual bool Init();
	virtual void Shutdown();

	virtual void Update(float frametime);

	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();

	virtual int KeyInput(int down, ButtonCode_t keynum, const char* pszCurrentBinding);

#ifdef WIN32
	void ProcessKeyInput(UINT message, WPARAM wParam, LPARAM lParam);
	void ProcessCompositionResult(wchar_t result);
	void ProcessDeadChar(UINT message, WPARAM wParam, LPARAM lParam);
#endif WIN32

	void SendKeyEventToBrowsers(const CefKeyEvent& keyevent);

	int GetKeyModifiers();

	void SetFocus(bool focus);

	short GetLastMouseWheelDist() { return m_iLastMouseWheelDist; }
	void SetLastMouseWheelDist(short dist) { m_iLastMouseWheelDist = dist; }

	void AddBrowser(CCefBrowser* pBrowser);
	void RemoveBrowser(CCefBrowser* pBrowser);
	int CountBrowsers(void);
	CCefBrowser* GetBrowser(int idx);
	CCefBrowser* FindBrowser(CefBrowser* pBrowser);
	CCefBrowser* FindBrowserByName(const char* pName);

	void OnScreenSizeChanged(int nOldWidth, int nOldHeight);

	CUtlVector< CCefBrowser* >& GetBrowsers();

	bool IsRunning();

private:
	bool m_bIsRunning;
	int m_iKeyModifiers;

	short m_iLastMouseWheelDist;
	bool m_bHasKeyFocus;

#ifdef WIN32
	// Stores last dead char vk and scancode
	bool m_bHasDeadChar;
	UINT m_lastDeadChar_scancode;
	UINT m_lastDeadChar_virtualKey;
	BYTE m_lastDeadChar_kbrdState[256];
#endif WIN32

	// Browser
	CUtlVector< CCefBrowser* > m_CefBrowsers;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CCefSystem::GetKeyModifiers()
{
	return m_iKeyModifiers;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline void CCefSystem::SetFocus(bool focus)
{
	m_bHasKeyFocus = focus;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline CUtlVector< CCefBrowser* >& CCefSystem::GetBrowsers()
{
	return m_CefBrowsers;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline bool CCefSystem::IsRunning()
{
	return m_bIsRunning;
}

CCefSystem& CEFSystem();

#endif // !CEF_SYSTEM_H
