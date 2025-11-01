//====== Copyright � Sandern Corporation, All rights reserved. ===========//
//
// Purpose: 
//
//=============================================================================

#ifndef SRC_CEF_VGUI_PANEL_H
#define SRC_CEF_VGUI_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include "cef_tex_gen.h"
#include "cef_os_renderer.h"
#include "materialsystem/MaterialSystemUtil.h"

class CCefBrowser;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCefVGUIPanel : public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE(CCefVGUIPanel, vgui::Panel);

	CCefVGUIPanel(const char* pName, CCefBrowser* pController, vgui::Panel* pParent);
	~CCefVGUIPanel();

	virtual bool ResizeTexture(int width, int height);
#ifdef RENDER_DIRTY_AREAS
	void MarkTextureDirty(int iDirtyX, int iDirtyY, int iDirtyXEnd, int iDirtyYEnd);
	void MarkTextureFullDirty();
#else
	void MarkTextureDirty();
#endif // RENDER_DIRTY_AREAS
	void MarkPopupDirty();

	virtual void ApplySchemeSettings(vgui::IScheme* pScheme);
	virtual void OnThink();
	virtual void Paint();
	virtual void PerformLayout();

	virtual void DrawWebview();

	int	GetEventFlags();

	virtual void OnCursorEntered();
	virtual void OnCursorExited();
	virtual void OnCursorMoved(int x, int y);
	virtual void OnMousePressed(vgui::MouseCode code);
	virtual void OnMouseDoublePressed(vgui::MouseCode code);
	virtual void OnMouseReleased(vgui::MouseCode code);
	virtual void OnMouseWheeled(int delta);

	virtual vgui::HCursor GetCursor();

	// Hack for working nice with VGUI input
	// Should be removed once we mix with VGUI anymore
	int GetTextureID();
	float GetTexS1();
	float GetTexT1();
	void SetDoNotDraw(bool state);
	bool GetDoNotDraw(void);
	float GetTexWide();
	float GetTexTall();

protected:
	int	GetBrowserID();
	bool IsValid();

private:
	virtual void UpdatePressedParent(vgui::MouseCode code, bool state);
	virtual bool IsPressedParent(vgui::MouseCode code);

private:
	int m_iMouseX, m_iMouseY;
	int m_iEventFlags;
	CCefBrowser* m_pBrowser;

	CUtlVector<unsigned char> m_SwizzleBuffer;

	bool m_bCalledLeftPressedParent;
	bool m_bCalledRightPressedParent;
	bool m_bCalledMiddlePressedParent;

	// Texture variables
	CCefTextureGenerator* m_Regenerator;
	CTextureReference	m_RenderBuffer;
	CMaterialReference m_MatRef;
	char m_MatWebViewName[MAX_PATH];
	char m_TextureWebViewName[MAX_PATH];

	vgui::HFont m_hLoadingFont;

	int m_iTextureID;
	int m_iWVWide, m_iWVTall;
	int m_iTexWide, m_iTexTall;
	int m_iTexFlags;
	ImageFormat m_iTexImageFormat;
	Color m_Color;
	float m_fTexS1, m_fTexT1;

#ifdef RENDER_DIRTY_AREAS
	int m_iDirtyX, m_iDirtyY, m_iDirtyXEnd, m_iDirtyYEnd;
#endif // RENDER_DIRTY_AREAS
	bool m_bTextureGeneratedOnce;
	bool m_bTextureDirty;
	bool m_bPopupTextureDirty;

	// Hack for working nice with VGUI input
	int m_iTopZPos, m_iBottomZPos;
	bool m_bDontDraw;
};

// Hack for working nice with VGUI input
inline int CCefVGUIPanel::GetTextureID()
{
	return m_iTextureID;
}

inline float CCefVGUIPanel::GetTexS1()
{
	return m_fTexS1;
}

inline float CCefVGUIPanel::GetTexT1()
{
	return m_fTexT1;
}

inline float CCefVGUIPanel::GetTexWide()
{
	return m_iTexWide;
}

inline float CCefVGUIPanel::GetTexTall()
{
	return m_iTexTall;
}

inline void CCefVGUIPanel::SetDoNotDraw(bool state)
{
	m_bDontDraw = state;
}

inline bool CCefVGUIPanel::GetDoNotDraw(void)
{
	return m_bDontDraw;
}

#ifdef RENDER_DIRTY_AREAS
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline void CCefVGUIPanel::MarkTextureDirty(int iDirtyX, int iDirtyY, int iDirtyXEnd, int iDirtyYEnd)
{
	m_bTextureDirty = true;

	m_iDirtyX = Max(0, Min(m_iDirtyX, iDirtyX));
	m_iDirtyY = Max(0, Min(m_iDirtyY, iDirtyY));
	m_iDirtyXEnd = Min(m_iWVWide, Max(m_iDirtyXEnd, iDirtyXEnd));
	m_iDirtyYEnd = Min(m_iWVTall, Max(m_iDirtyYEnd, iDirtyYEnd));

	// Popup always becomes dirty because it is drawn on top of the base texture
	MarkPopupDirty();
}

inline void CCefVGUIPanel::MarkTextureFullDirty()
{
	m_iDirtyX = m_iDirtyY = 0;
	m_iDirtyXEnd = m_iWVWide;
	m_iDirtyYEnd = m_iWVTall;

	// Popup always becomes dirty because it is drawn on top of the base texture
	MarkPopupDirty();
}
#else
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline void CCefVGUIPanel::MarkTextureDirty()
{
	m_bTextureDirty = true;

	if (m_Regenerator != NULL)
		m_Regenerator->MakeDirty();

	// Popup always becomes dirty because it is drawn on top of the base texture
	MarkPopupDirty();
}
#endif // RENDER_DIRTY_AREAS

inline void CCefVGUIPanel::MarkPopupDirty()
{
	m_bPopupTextureDirty = true;
}

#endif // SRC_CEF_VGUI_PANEL_H