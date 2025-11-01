/*
 * Copyright (c) 2025 The Solo Fortress 2 Team, all rights reserved.
 * cef_cxx20_stubs.h, Some required definitions and macros to allow the project to compile with CEF includes.
 *
 * Licensed under CC BY-NC 3.0 (Lambda Wars' original license)
 */

#ifndef CEF_CXX20_STUBS_H
#define CEF_CXX20_STUBS_H
#ifdef _WIN32
#pragma once
#endif // _WIN32

#ifdef null
#undef null
#endif

namespace std
{
	struct in_place_t {};
	constexpr in_place_t in_place{};
}

#pragma warning ( disable : 4100 )
#pragma warning ( disable : 4005 )

#endif