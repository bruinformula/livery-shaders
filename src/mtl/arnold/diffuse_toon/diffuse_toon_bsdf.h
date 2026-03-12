#pragma once

#include <ai.h>

// Based on the Toon BSDFs from Blender/Cycles

AtBSDF* DiffuseToonBSDFCreate(const AtShaderGlobals* sg,
                               const AtVector& N,
                               const AtRGB& color,
                               float size,
                               float smooth);