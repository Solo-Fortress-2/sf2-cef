//====== Copyright � Sandern Corporation, All rights reserved. ===========//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "cef_tex_gen.h"
#include "cef_vgui_panel.h"
#include "cef_browser.h"
#include "cef_system.h"
#include "cef_os_renderer.h"
#include "clientmode_shared.h"

// @PracticeMedicine: Win32 fixes
#ifdef GetCursorPos
#undef GetCursorPos
#endif

#include <vgui_controls/Controls.h>
#include <vgui/IInput.h>

// CEF
#include "cef_cxx20_stubs.h"
#include "include/cef_browser.h"

#include "materialsystem/ishaderapi.h"
#include "materialsystem/itexture.h"
#include "shaderapi/IShaderAPI.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

ConVar g_cef_draw("g_cef_draw", "1");
ConVar g_cef_debug_texture("g_cef_debug_texture", "0");
ConVar g_cef_loading_text_delay("g_cef_loading_text_delay", "20.0");

//-----------------------------------------------------------------------------
// Purpose: Find appropiate texture width/height helper
//-----------------------------------------------------------------------------
static int nexthigher(int k)
{
	k--;
	for (int i = 1; i < sizeof(int) * CHAR_BIT; i <<= 1)
		k = k | k >> i;
	return k + 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCefVGUIPanel::CCefVGUIPanel(const char* pName, CCefBrowser* pController, vgui::Panel* pParent)
	: Panel(NULL, "SrcCefPanel"), m_pBrowser(pController), m_iTextureID(-1),
	m_bTextureDirty(true), m_bPopupTextureDirty(false), m_bTextureGeneratedOnce(false)
{
	SetPaintBackgroundEnabled(false);
	SetScheme("SourceScheme");

	SetParent(pParent ? pParent : GetClientModeNormal()->GetViewport());

	m_Color = Color(255, 255, 255, 255);
	m_iTexWide = m_iTexTall = 0;

	m_iEventFlags = EVENTFLAG_NONE;
	m_iMouseX = 0;
	m_iMouseY = 0;

	m_Regenerator = NULL;

	m_bCalledLeftPressedParent = m_bCalledRightPressedParent = m_bCalledMiddlePressedParent = false;

	static int staticMatWebViewID = 0;
	Q_snprintf(m_MatWebViewName, _MAX_PATH, "vgui/webview/webview_test%d", staticMatWebViewID++);

	// Hack for working nice with VGUI input
	m_iTopZPos = 10;
	m_iBottomZPos = -15;
	m_bDontDraw = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCefVGUIPanel::~CCefVGUIPanel()
{
	m_pBrowser = NULL;

	if (m_iTextureID != -1)
	{
		vgui::surface()->DestroyTextureID(m_iTextureID);
	}

	if (m_RenderBuffer.IsValid())
	{
		m_RenderBuffer->SetTextureRegenerator(NULL);
		m_RenderBuffer.Shutdown();
	}
	if (m_MatRef.IsValid())
	{
		m_MatRef.Shutdown();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCefVGUIPanel::ResizeTexture(int width, int height)
{
	if (!m_pBrowser)
		return false;

#ifdef USE_MULTITHREADED_MESSAGELOOP
	CefRefPtr<CCefOSRRenderer> renderer = m_pBrowser->GetOSRHandler();
	if (!renderer)
		return false;
#endif // USE_MULTITHREADED_MESSAGELOOP

	m_iWVWide = width;
	m_iWVTall = height;

	int po2wide = nexthigher(m_iWVWide);
	int po2tall = nexthigher(m_iWVTall);

	DevMsg(1, "Cef#%d: Resizing texture from %d %d to %d %d (wish size: %d %d)\n", GetBrowserID(), m_iTexWide, m_iTexTall, po2wide, po2tall, m_iWVWide, m_iWVTall);

	// Only rebuild when the actual size change and don't bother if the size
	// is larger than needed.
	// Otherwise just need to update m_fTexS1 and m_fTexT1 :)
	if (m_iTexWide >= po2wide && m_iTexTall >= po2tall)
	{
		m_fTexS1 = 1.0 - (m_iTexWide - m_iWVWide) / (float)m_iTexWide;
		m_fTexT1 = 1.0 - (m_iTexTall - m_iWVTall) / (float)m_iTexTall;
		return true;
	}

	m_iTexWide = po2wide;
	m_iTexTall = po2tall;
	m_fTexS1 = 1.0 - (m_iTexWide - m_iWVWide) / (float)m_iTexWide;
	m_fTexT1 = 1.0 - (m_iTexTall - m_iWVTall) / (float)m_iTexTall;

	// Ensure we have a procedural texture ID; the buffer upload happens during Paint.
	if (m_iTextureID == -1)
	{
		m_iTextureID = vgui::surface()->CreateNewTextureID(true);
	}

	// Mark full dirty so the next Paint uploads the image data
#ifdef RENDER_DIRTY_AREAS
	MarkTextureDirty(0, 0, m_pBrowser->GetPanel()->GetWide(), m_pBrowser->GetPanel()->GetTall());
#else
	MarkTextureDirty();
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefVGUIPanel::ApplySchemeSettings(vgui::IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hLoadingFont = pScheme->GetFont("HUDNumber");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefVGUIPanel::OnThink()
{
	BaseClass::OnThink();

	if (!m_pBrowser)
		return;

	if (m_pBrowser->GetPassMouseTruIfAlphaZero())
	{
		// Hack for working nice with VGUI input
		// Move to back in case the mouse is on nothing
		// Move to front in case on something
		int x, y;
		vgui::input()->GetCursorPos(x, y);
		ScreenToLocal(x, y);

		if (m_pBrowser->IsAlphaZeroAt(x, y))
		{
			SetZPos(m_iBottomZPos);
		}
		else
		{
			SetZPos(m_iTopZPos);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefVGUIPanel::Paint()
{
	if (!m_pBrowser || !m_pBrowser->GetOSRHandler())
		return;

	CefRefPtr<CCefOSRRenderer> renderer = m_pBrowser->GetOSRHandler();
	if (!renderer)
		return;

	// Update panel size if needed
	if (renderer->GetWidth() != m_iWVWide || renderer->GetHeight() != m_iWVTall)
	{
		if (!ResizeTexture(renderer->GetWidth(), renderer->GetHeight()))
			return;
	}

	SetCursor(renderer->GetCursor());

	if (renderer->GetTextureBuffer() && m_bTextureDirty)
	{
		const int texW = renderer->GetWidth();
		const int texH = renderer->GetHeight();
		if (texW <= 0 || texH <= 0)
			return;

		if (m_iTextureID <= 0)
			m_iTextureID = vgui::surface()->CreateNewTextureID(true);

		const int numPixels = texW * texH;
		m_SwizzleBuffer.EnsureCount(numPixels * 4);
		const unsigned char* src = renderer->GetTextureBuffer();
		unsigned char* dst = m_SwizzleBuffer.Base();
		for (int i = 0; i < numPixels; ++i)
		{
			const int si = i * 4;
			dst[si + 0] = src[si + 2];
			dst[si + 1] = src[si + 1];
			dst[si + 2] = src[si + 0];
			dst[si + 3] = src[si + 3];
		}
		vgui::surface()->DrawSetTextureRGBA(m_iTextureID, dst, texW, texH, false, false);

		m_bTextureDirty = false;
		m_bTextureGeneratedOnce = true;
	}

	if (!m_bDontDraw)
	{
		DrawWebview();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefVGUIPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	CefRefPtr<CCefOSRRenderer> renderer = m_pBrowser->GetOSRHandler();
	if (!renderer)
		return;

	int x, y, wide, tall;
	GetPos(x, y);
	GetSize(wide, tall);

	renderer->UpdateViewRect(x, y, wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefVGUIPanel::DrawWebview()
{
	int iWide, iTall;
	GetSize(iWide, iTall);

	if (surface()->IsTextureIDValid(m_iTextureID) && m_bTextureGeneratedOnce && g_cef_draw.GetBool())
	{
		vgui::surface()->DrawSetColor(m_Color);
		vgui::surface()->DrawSetTexture(m_iTextureID);
		vgui::surface()->DrawTexturedSubRect(0, 0, iWide, iTall, 0, 0, m_fTexS1, m_fTexT1);
	}
	else
	{
		// Show loading text if it takes longer than a certain time
		if (Plat_FloatTime() - m_pBrowser->GetLastLoadStartTime() > g_cef_loading_text_delay.GetFloat())
		{
			wchar_t buf[256];
			mbstowcs(buf, m_pBrowser->GetName(), sizeof(buf));

			wchar_t text[256];
			V_snwprintf(text, sizeof(text) / sizeof(wchar_t), L"Loading %ls...", buf);

			int iTextWide, iTextTall;
			surface()->GetTextSize(m_hLoadingFont, text, iTextWide, iTextTall);

			vgui::surface()->DrawSetTextFont(m_hLoadingFont);
			vgui::surface()->DrawSetTextColor(m_Color);
			vgui::surface()->DrawSetTextPos((iWide / 2) - (iTextWide / 2), (iTall / 2) - (iTextTall / 2));

			vgui::surface()->DrawUnicodeString(text);
		}

		// Debug reason why nothing is showing yet
		if (g_cef_debug_texture.GetBool())
		{
			DevMsg("CEF: not drawing");
			if (m_iTextureID == -1)
				DevMsg(", texture does not exists");
			if (m_bTextureDirty)
				DevMsg(", texture is dirty");
			if (!g_cef_draw.GetBool())
				DevMsg(", g_cef_draw is 0");
			DevMsg("\n");
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CCefVGUIPanel::GetEventFlags()
{
	return m_iEventFlags | CEFSystem().GetKeyModifiers();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefVGUIPanel::OnCursorEntered()
{
	if (!IsValid())
		return;

	// Called before OnCursorMoved, so mouse coordinates might be outdated
	int x, y;
	vgui::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);

	m_iMouseX = x;
	m_iMouseY = y;

	CefMouseEvent me;
	me.x = m_iMouseX;
	me.y = m_iMouseY;
	me.modifiers = GetEventFlags();
	m_pBrowser->GetBrowser()->GetHost()->SendMouseMoveEvent(me, false);

	DevMsg(2, "Cef#%d: injected cursor entered %d %d\n", GetBrowserID(), m_iMouseX, m_iMouseY);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefVGUIPanel::OnCursorExited()
{
	if (!IsValid())
		return;

	CefMouseEvent me;
	me.x = m_iMouseX;
	me.y = m_iMouseY;
	me.modifiers = GetEventFlags();
	m_pBrowser->GetBrowser()->GetHost()->SendMouseMoveEvent(me, true);

	DevMsg(2, "Cef#%d: injected cursor exited %d %d\n", GetBrowserID(), m_iMouseX, m_iMouseY);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefVGUIPanel::OnCursorMoved(int x, int y)
{
	if (!IsValid())
		return;

	m_iMouseX = x;
	m_iMouseY = y;

	if (m_pBrowser->GetPassMouseTruIfAlphaZero() && m_pBrowser->IsAlphaZeroAt(x, y))
	{
		DevMsg(3, "Cef#%d: passed cursor move %d %d to parent (alpha zero)\n", GetBrowserID(), x, y);

		CallParentFunction(new KeyValues("OnCursorMoved", "x", x, "y", y));
	}

	CefMouseEvent me;
	me.x = x;
	me.y = y;
	me.modifiers = GetEventFlags();
	m_pBrowser->GetBrowser()->GetHost()->SendMouseMoveEvent(me, false);

	DevMsg(3, "Cef#%d: injected cursor move %d %d\n", GetBrowserID(), x, y);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefVGUIPanel::UpdatePressedParent(vgui::MouseCode code, bool state)
{
	switch (code)
	{
	case MOUSE_LEFT:
		m_bCalledLeftPressedParent = state;
		break;
	case MOUSE_RIGHT:
		m_bCalledRightPressedParent = state;
		break;
	case MOUSE_MIDDLE:
		m_bCalledMiddlePressedParent = state;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCefVGUIPanel::IsPressedParent(vgui::MouseCode code)
{
	switch (code)
	{
	case MOUSE_LEFT:
		return m_bCalledLeftPressedParent;
	case MOUSE_RIGHT:
		return m_bCalledRightPressedParent;
	case MOUSE_MIDDLE:
		return m_bCalledMiddlePressedParent;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefVGUIPanel::OnMousePressed(vgui::MouseCode code)
{
	if (!IsValid())
		return;

	// Do click on parent if needed
	// Store if we pressed on the parent
	if (m_pBrowser->GetPassMouseTruIfAlphaZero() && m_pBrowser->IsAlphaZeroAt(m_iMouseX, m_iMouseY))
	{
		DevMsg(1, "Cef#%d: passed mouse pressed %d %d to parent\n", GetBrowserID(), m_iMouseX, m_iMouseY);

		CallParentFunction(new KeyValues("MousePressed", "code", code));

		UpdatePressedParent(code, true);
	}
	else
	{
		UpdatePressedParent(code, false);
	}

	if (m_pBrowser->GetUseMouseCapture())
	{
		// Make sure released is called on this panel
		// Make sure to do this after calling the parent, so we override
		// the mouse capture to this panel
		vgui::input()->SetMouseCaptureEx(GetVPanel(), code);
	}

	CefBrowserHost::MouseButtonType iMouseType = MBT_LEFT;

	CefMouseEvent me;
	me.x = m_iMouseX;
	me.y = m_iMouseY;

	switch (code)
	{
	case MOUSE_LEFT:
		iMouseType = MBT_LEFT;
		m_iEventFlags |= EVENTFLAG_LEFT_MOUSE_BUTTON;
		break;
	case MOUSE_RIGHT:
		iMouseType = MBT_RIGHT;
		m_iEventFlags |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
		break;
	case MOUSE_MIDDLE:
		iMouseType = MBT_MIDDLE;
		m_iEventFlags |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
		break;
	}

	me.modifiers = GetEventFlags();

	m_pBrowser->GetBrowser()->GetHost()->SendMouseClickEvent(me, iMouseType, false, 1);

	DevMsg(1, "Cef#%d: injected mouse pressed %d %d (mouse capture: %d)\n", GetBrowserID(), m_iMouseX, m_iMouseY, m_pBrowser->GetUseMouseCapture());
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefVGUIPanel::OnMouseDoublePressed(vgui::MouseCode code)
{
	if (!IsValid())
		return;

	// Do click on parent if needed
	if (m_pBrowser->GetPassMouseTruIfAlphaZero() && m_pBrowser->IsAlphaZeroAt(m_iMouseX, m_iMouseY))
	{
		DevMsg(1, "Cef#%d: passed mouse double pressed %d %d to parent\n", GetBrowserID(), m_iMouseX, m_iMouseY);

		CallParentFunction(new KeyValues("MouseDoublePressed", "code", code));
	}

	CefBrowserHost::MouseButtonType iMouseType = MBT_LEFT;

	// Do click
	CefMouseEvent me;
	me.x = m_iMouseX;
	me.y = m_iMouseY;

	switch (code)
	{
	case MOUSE_LEFT:
		iMouseType = MBT_LEFT;
		m_iEventFlags |= EVENTFLAG_LEFT_MOUSE_BUTTON;
		break;
	case MOUSE_RIGHT:
		iMouseType = MBT_RIGHT;
		m_iEventFlags |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
		break;
	case MOUSE_MIDDLE:
		iMouseType = MBT_MIDDLE;
		m_iEventFlags |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
		break;
	}

	me.modifiers = GetEventFlags();

	m_pBrowser->GetBrowser()->GetHost()->SendMouseClickEvent(me, iMouseType, false, 2);

	DevMsg(1, "Cef#%d: injected mouse double pressed %d %d\n", GetBrowserID(), m_iMouseX, m_iMouseY);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefVGUIPanel::OnMouseReleased(vgui::MouseCode code)
{
	if (!IsValid())
		return;

	// Check mouse capture and make sure it is cleared
	bool bHasMouseCapture = vgui::input()->GetMouseCapture() == GetVPanel();
	if (bHasMouseCapture)
	{
		vgui::input()->SetMouseCaptureEx(0, code);
	}

	// Check if we should pass input to parent (only if we don't have mouse capture active)
	if ((m_pBrowser->GetUseMouseCapture() && IsPressedParent(code)) || (m_pBrowser->GetPassMouseTruIfAlphaZero() && m_pBrowser->IsAlphaZeroAt(m_iMouseX, m_iMouseY)))
	{
		DevMsg(1, "Cef#%d: passed mouse released %d %d to parent\n", GetBrowserID(), m_iMouseX, m_iMouseY);

		CallParentFunction(new KeyValues("MouseReleased", "code", code));
	}

	// Clear parent pressed
	UpdatePressedParent(code, false);

	// Do click
	CefBrowserHost::MouseButtonType iMouseType = MBT_LEFT;

	CefMouseEvent me;
	me.x = m_iMouseX;
	me.y = m_iMouseY;

	switch (code)
	{
	case MOUSE_LEFT:
		iMouseType = MBT_LEFT;
		m_iEventFlags &= ~EVENTFLAG_LEFT_MOUSE_BUTTON;
		break;
	case MOUSE_RIGHT:
		iMouseType = MBT_RIGHT;
		m_iEventFlags &= ~EVENTFLAG_RIGHT_MOUSE_BUTTON;
		break;
	case MOUSE_MIDDLE:
		iMouseType = MBT_MIDDLE;
		m_iEventFlags &= ~EVENTFLAG_MIDDLE_MOUSE_BUTTON;
		break;
	}

	me.modifiers = GetEventFlags();

	m_pBrowser->GetBrowser()->GetHost()->SendMouseClickEvent(me, iMouseType, true, 1);

	DevMsg(1, "Cef#%d: injected mouse released %d %d\n", GetBrowserID(), m_iMouseX, m_iMouseY);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCefVGUIPanel::OnMouseWheeled(int delta)
{
	if (!IsValid())
		return;

	if (m_pBrowser->GetPassMouseTruIfAlphaZero() && m_pBrowser->IsAlphaZeroAt(m_iMouseX, m_iMouseY))
	{
		CallParentFunction(new KeyValues("MouseWheeled", "delta", delta));
		return;
	}

	DevMsg(1, "Cef#%d: injected mouse wheeled %d\n", GetBrowserID(), delta);

	CefMouseEvent me;
	me.x = m_iMouseX;
	me.y = m_iMouseY;
	me.modifiers = GetEventFlags();

	// VGUI just gives -1 or +1. SendMouseWheelEvent expects the number of pixels to shift.
	// Use the last mouse wheel value from the window proc instead
	m_pBrowser->GetBrowser()->GetHost()->SendMouseWheelEvent(me, 0, CEFSystem().GetLastMouseWheelDist());
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
vgui::HCursor CCefVGUIPanel::GetCursor()
{
	if (GetParent() && m_pBrowser->GetPassMouseTruIfAlphaZero() && m_pBrowser->IsAlphaZeroAt(m_iMouseX, m_iMouseY))
	{
		return GetParent()->GetCursor();
	}

	return BaseClass::GetCursor();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CCefVGUIPanel::GetBrowserID()
{
	return m_pBrowser && m_pBrowser->GetBrowser() ? m_pBrowser->GetBrowser()->GetIdentifier() : -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCefVGUIPanel::IsValid()
{
	return m_pBrowser && m_pBrowser->GetBrowser();
}