#pragma once 
#define ENTRY_BUMP_TO_ROUGHNESS void
#ifdef NOT_MTX_IMPL
	#include "../Utility/BumpToRoughness.osl"
#else
	#include "../BumpToRoughness.osl"
#endif