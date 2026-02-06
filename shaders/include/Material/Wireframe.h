#pragma once 
#define ENTRY_WIREFRAME void
#ifdef NOT_MTX_IMPL
	#include "../Material/Wireframe.osl"
#else
	#include "../Wireframe.osl"
#endif