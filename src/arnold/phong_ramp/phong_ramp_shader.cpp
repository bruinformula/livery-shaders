#include <ai_closure.h>
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

#include "phong_ramp_bsdf.h"

class AtRenderSession;
struct AtList;
struct AtNode;
struct AtNodeEntry;

// Based on the matching BSDFs from Cycles 

AI_SHADER_NODE_EXPORT_METHODS(PhongRampMtd)

enum PhongRampParams {
    p_A, p_B, p_C, p_D, p_E, p_F, p_G, p_H,
    p_exponent,
    p_normal
};

node_parameters
{
    AiParameterRGB("colorA", 0.8f, 0.8f, 0.8f);
    AiParameterRGB("colorB", 0.8f, 0.8f, 0.8f);
    AiParameterRGB("colorC", 0.8f, 0.8f, 0.8f);
    AiParameterRGB("colorD", 0.8f, 0.8f, 0.8f);
    AiParameterRGB("colorE", 0.8f, 0.8f, 0.8f);
    AiParameterRGB("colorF", 0.8f, 0.8f, 0.8f);
    AiParameterRGB("colorG", 0.8f, 0.8f, 0.8f);
    AiParameterRGB("colorH", 0.8f, 0.8f, 0.8f);
    AiParameterFlt("exponent", 10.0f);
    AiParameterVec("N",   0.0f, 0.0f, 0.0f);
}

node_initialize {}
node_update {}
node_finish {}

shader_evaluate
{
    if (sg->Rt & AI_RAY_SHADOW)
        return;

    AtVector normal = AiShaderEvalParamVec(p_normal);
    
   if (AiV3IsSmall(normal, 1e-6f)) {
        normal = sg->Nf;
   } else {
        normal = AiV3Normalize(normal);
   }

    sg->out.CLOSURE() = PhongRampBSDFCreate(
        sg, normal,
        AiShaderEvalParamFlt(p_exponent),
        AiShaderEvalParamRGB(p_A),
        AiShaderEvalParamRGB(p_B),
        AiShaderEvalParamRGB(p_C),
        AiShaderEvalParamRGB(p_D),
        AiShaderEvalParamRGB(p_E),
        AiShaderEvalParamRGB(p_F),
        AiShaderEvalParamRGB(p_G),
        AiShaderEvalParamRGB(p_H)
    );
}

node_loader
{
    if (i > 0) return false;
    node->methods = PhongRampMtd;
    node->output_type = AI_TYPE_CLOSURE;
    node->name = "phong_ramp_bsdf";
    node->node_type = AI_NODE_SHADER;
    strcpy(node->version, AI_VERSION);
    return true;
}