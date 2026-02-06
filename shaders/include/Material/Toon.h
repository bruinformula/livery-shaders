#pragma once 
#define ENTRY_TOON void
#ifdef NOT_MTX_IMPL
	#include "../Material/Toon.osl"
#else
	#include "../Toon.osl"
#endif