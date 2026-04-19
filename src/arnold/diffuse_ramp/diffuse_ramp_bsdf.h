#pragma once

struct AtBSDF;
struct AtRGB;
struct AtShaderGlobals;
struct AtVector;

// Based on the matching BSDFs from Cycles 

AtBSDF* DiffuseRampBSDFCreate(const AtShaderGlobals* sg, const AtVector& N,
                              AtRGB colorA, AtRGB colorB, AtRGB colorC, AtRGB colorD,
                              AtRGB colorE, AtRGB colorF, AtRGB colorG, AtRGB colorH);