#pragma once

#include <ai_shader_bsdf.h>
#include <ai_shaderglobals.h>

AtBSDF* PhongRampBSDFCreate(const AtShaderGlobals* sg,
                             const AtVector& N,
                             float exponent,
                             AtRGB colorA, AtRGB colorB, AtRGB colorC, AtRGB colorD,
                             AtRGB colorE, AtRGB colorF, AtRGB colorG, AtRGB colorH);