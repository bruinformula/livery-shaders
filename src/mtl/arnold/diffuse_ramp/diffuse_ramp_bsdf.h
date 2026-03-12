#pragma once

#include <ai.h>

// Based on the matching BSDFs from Cycles 

AtBSDF* DiffuseRampBSDFCreate(const AtShaderGlobals* sg, const AtVector& N,
                              AtRGB colorA, AtRGB colorB, AtRGB colorC, AtRGB colorD,
                              AtRGB colorE, AtRGB colorF, AtRGB colorG, AtRGB colorH);