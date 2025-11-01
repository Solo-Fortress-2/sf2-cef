#ifndef CEF_TEXGEN_H
#define CEF_TEXGEN_H
#ifdef _WIN32
#pragma once
#endif // _WIN32

#include "materialsystem/itexture.h"

class CCefBrowser;

//-----------------------------------------------------------------------------
// Purpose: Texture generation
//-----------------------------------------------------------------------------
class CCefTextureGenerator : public ITextureRegenerator
{
public:
	CCefTextureGenerator(CCefBrowser* pBrowser);
	virtual void RegenerateTextureBits(ITexture* pTexture, IVTFTexture* pVTFTexture, Rect_t* pRect);
	virtual void Release() {}

	bool IsDirty() const { return m_bIsDirty; }
	void MakeDirty() { m_bIsDirty = true; }
	void ClearDirty() { m_bIsDirty = false; }

private:
	bool m_bIsDirty;
	CCefBrowser* m_pBrowser;
};

#endif // !CEF_TEXGEN_H
