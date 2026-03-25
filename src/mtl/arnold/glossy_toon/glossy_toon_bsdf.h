#pragma once

struct AtBSDF;
struct AtRGB;
struct AtShaderGlobals;
struct AtVector;

// Based on the Toon BSDFs from Blender/Cycles

AtBSDF* GlossyToonBSDFCreate(const AtShaderGlobals* sg,
                              const AtVector& N,
                              const AtRGB& color,
                              float size,
                              float smooth);