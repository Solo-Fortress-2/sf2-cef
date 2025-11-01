//====== Copyright � Sandern Corporation, All rights reserved. ===========//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "sf2/uuid_util.h"
#include "cef_js.h"

// CEF
#include "cef_cxx20_stubs.h"
#include "include/cef_v8.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static int s_NextJSObjectID = 0;

JSObject::JSObject(const char* pName, const char* pUUID)
{
	m_Name = pName;

	if (pUUID == NULL)
	{
		char uuid[37];
		if (!SF2_GenerateUUID(uuid))
			m_UUID = pUUID;
		else
			m_UUID = uuid;
	}
	else
	{
		m_UUID = pUUID;
	}
}

JSObject::~JSObject()
{
	// TODO: cleanup
}