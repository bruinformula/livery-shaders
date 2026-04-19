#include <ai_closure.h>
#include <ai_color.h>
#include <ai_node_entry.h>
#include <ai_nodes.h>
#include <ai_params.h>
#include <ai_plugins.h>
#include <ai_ray.h>
#include <ai_shader_parameval.h>
#include <ai_shaderglobals.h>
#include <ai_shaders.h>
#include <ai_vector.h>
#include <ai_version.h>

#include "glossy_toon_bsdf.h"

class AtRenderSession;
struct AtList;
struct AtNode;
struct AtNodeEntry;

// Based on the Toon BSDFs from Blender/Cycles

AI_SHADER_NODE_EXPORT_METHODS(GlossyToonMtd)

enum GlossyToonParams {
    p_color,
    p_size,
    p_smooth,
    p_normal
};

node_parameters
{
    AiParameterRGB("color",  1.0f, 1.0f, 1.0f);
    AiParameterFlt("size",   0.5f);
    AiParameterFlt("smooth", 0.0f);
    AiParameterVec("N",      0.0f, 0.0f, 0.0f);
}

node_initialize {}
node_update {}
node_finish {}

shader_evaluate
{
    if (sg->Rt & AI_RAY_SHADOW)
        return;

    const AtRGB color  = AiShaderEvalParamRGB(p_color);
    const float size   = AiShaderEvalParamFlt(p_size);
    const float smooth = AiShaderEvalParamFlt(p_smooth);

    AtVector normal = AiShaderEvalParamVec(p_normal);
    if (AiV3IsSmall(normal, 1e-6f))
        normal = sg->Nf;
    else
        normal = AiV3Normalize(normal);

    sg->out.CLOSURE() = GlossyToonBSDFCreate(sg, normal, color, size, smooth);
}

node_loader
{
    if (i > 0)
        return false;
    node->methods     = GlossyToonMtd;
    node->output_type = AI_TYPE_CLOSURE;
    node->name        = "glossy_toon_bsdf";
    node->node_type   = AI_NODE_SHADER;
    strcpy(node->version, AI_VERSION);
    return true;
}