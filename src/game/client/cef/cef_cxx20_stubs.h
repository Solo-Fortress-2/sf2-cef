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

// @PracticeMedicine:
// this makes existing CEF function signatures error out because
// some of them have "null" as their parameter name.
#ifdef null
#undef null
#endif

// @PracticeMedicine:
// needed for some CEF base functions
namespace std
{
	struct in_place_t {};
	constexpr in_place_t in_place{};
}

#pragma warning ( disable : 4005 )

#endif