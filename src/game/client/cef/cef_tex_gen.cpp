//====== Copyright � Sandern Corporation, All rights reserved. ===========//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "cef_tex_gen.h"
#include "cef_browser.h"
#include "cef_os_renderer.h"
#include "cef_vgui_panel.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static ConVar g_debug_cef_test("g_debug_cef_test", "0");

CCefTextureGenerator::CCefTextureGenerator(CCefBrowser* pBrowser) : m_pBrowser(pBrowser), m_bIsDirty(true)
{

}

//-----------------------------------------------------------------------------
// Purpose: Texture generator
//-----------------------------------------------------------------------------
void CCefTextureGenerator::RegenerateTextureBits(ITexture* pTexture, IVTFTexture* pVTFTexture, Rect_t* pRect)
{
	if (g_debug_cef_test.GetBool())
		return;

#ifdef USE_MULTITHREADED_MESSAGELOOP
	CefRefPtr<CCefOSRRenderer> renderer = m_pBrowser->GetOSRHandler();

	if (!renderer)
		return;

	AUTO_LOCK(renderer->GetTextureBufferMutex());

	if (!renderer->GetTextureBuffer())
		return;
#endif

	// Don't regenerate while loading
	if (engine->IsDrawingLoadingImage())
	{
#ifdef RENDER_DIRTY_AREAS
		int width, height;
		width = renderer->GetWidth();
		height = renderer->GetHeight();

		m_pBrowser->GetPanel()->MarkTextureDirty(0, 0, width, height);
#else
		m_pBrowser->GetPanel()->MarkTextureDirty();
#endif
		return;
	}

	if (!pRect)
	{
		Warning("CCefTextureGenerator: Regenerating image, but no area specified\n");
		return;
	}

#ifndef USE_MULTITHREADED_MESSAGELOOP
	CefRefPtr<SrcCefOSRRenderer> renderer = m_pBrowser->GetOSRHandler();
	if (!renderer || !renderer->GetTextureBuffer())
		return;
#endif // USE_MULTITHREADED_MESSAGELOOP

	int width, height, channels;
	int srcwidth, srcheight;

	width = pVTFTexture->Width();
	height = pVTFTexture->Height();
	Assert(pVTFTexture->Format() == IMAGE_FORMAT_BGRA8888);
	channels = 4;

	unsigned char* imageData = pVTFTexture->ImageData(0, 0, 0);

	m_bIsDirty = false;

	srcwidth = renderer->GetWidth();
	srcheight = renderer->GetHeight();

	// Shouldn't happen, but can happen
	if (srcwidth > width || srcheight > height)
		return;

	const unsigned char* srcbuffer = renderer->GetTextureBuffer();

	// Copy per row
	int clampedwidth = Min(srcwidth, pRect->width);
	int yend = Min(srcheight, pRect->y + pRect->height);
	int xoffset = (pRect->x * channels);
	for (int y = pRect->y; y < yend; y++)
	{
		Q_memcpy(imageData + (y * width * channels) + xoffset, // Destination
			srcbuffer + (y * srcwidth * channels) + xoffset, // Source
			clampedwidth * channels // Row width to copy
		);
	}
}