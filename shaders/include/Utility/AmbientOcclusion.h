#pragma once 
#define ENTRY_AMBIENT_OCCLUSION void
#ifdef NOT_MTX_IMPL
	#include "../Utility/AmbientOcclusion.osl"
#else
	#include "../AmbientOcclusion.osl"
#endif