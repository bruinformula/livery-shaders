#include "diffuse_bsdf.h"

#include <ai.h>

AI_SHADER_NODE_EXPORT_METHODS(DiffuseRampMtd)

enum DiffuseRampParams {
   p_A,
   p_B,
   p_C,
   p_D,
   p_E,
   p_F,
   p_G,
   p_H,
   p_normal
};

node_parameters
{
   AiParameterRGB("colorA", 0.8f, 0.8f, 0.8f);  // red
   AiParameterRGB("colorB", 0.8f, 0.8f, 0.8f);  // orange
   AiParameterRGB("colorC", 0.8f, 0.8f, 0.8f);  // yellow
   AiParameterRGB("colorD", 0.8f, 0.8f, 0.8f);  // green
   AiParameterRGB("colorE", 0.8f, 0.8f, 0.8f);  // cyan
   AiParameterRGB("colorF", 0.8f, 0.8f, 0.8f);  // blue
   AiParameterRGB("colorG", 0.8f, 0.8f, 0.8f);  // violet
   AiParameterRGB("colorH", 0.8f, 0.8f, 0.8f);  // magenta
   AiParameterVec("N", 0.0f, 0.0f, 0.0f);

}

node_initialize
{
}

node_update
{
}

node_finish
{
}

shader_evaluate
{
   // early out for shadow rays and black color
   if (sg->Rt & AI_RAY_SHADOW)
      return;

   AtRGB colorA = AiShaderEvalParamRGB(p_A);
   AtRGB colorB = AiShaderEvalParamRGB(p_B);
   AtRGB colorC = AiShaderEvalParamRGB(p_C);
   AtRGB colorD = AiShaderEvalParamRGB(p_D);
   AtRGB colorE = AiShaderEvalParamRGB(p_E);
   AtRGB colorF = AiShaderEvalParamRGB(p_F);
   AtRGB colorG = AiShaderEvalParamRGB(p_G);
   AtRGB colorH = AiShaderEvalParamRGB(p_H);

   AtVector normal = AiShaderEvalParamVec(p_normal);

   if (AiV3IsSmall(normal, 1e-6f)) {
        normal = sg->Nf;
   } else {
        normal = AiV3Normalize(normal);
   }

   sg->out.CLOSURE() = DiffuseRampBSDFCreate(sg, normal,
    colorA, colorB, colorC, colorD,
    colorE, colorF, colorG, colorH);
}

node_loader
{
   if (i>0)
      return false;

   node->methods      = DiffuseRampMtd;
   node->output_type  = AI_TYPE_CLOSURE;
   node->name         = "diffuse_ramp_bsdf";
   node->node_type    = AI_NODE_SHADER;
   strcpy(node->version, AI_VERSION);
   return true;
}