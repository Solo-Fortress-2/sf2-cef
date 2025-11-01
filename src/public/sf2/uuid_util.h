#ifndef UUID_UTIL_H
#define UUID_UTIL_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/strtools.h"

#ifdef WIN32
#include <Rpc.h>
#else
#include <uuid/uuid.h>
#endif

inline bool SF2_GenerateUUID( char *destuuid )
{
#ifdef WIN32
    UUID uuid;
    UuidCreate ( &uuid );

    unsigned char * str;
    if( UuidToStringA ( &uuid, &str ) != RPC_S_OK )
	{
		return false;
	}

	Q_strncpy( destuuid, (char *)str, 37 );

	RpcStringFreeA ( &str );
#else
    uuid_t uuid;
    uuid_generate_random ( uuid );
    char s[37];
    uuid_unparse ( uuid, s );
#endif
	return true;
}

#endif // WARS_CEF_SHARED_H