#pragma once 
#define ENTRY_KUWAHARA_FILTER void
#ifdef NOT_MTX_IMPL
	#include "../Pattern/KuwaharaFilter.osl"
#else
	#include "../KuwaharaFilter.osl"
#endif