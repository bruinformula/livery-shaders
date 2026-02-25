#include "diffuse_bsdf.h"

#include <ai.h>

AI_SHADER_NODE_EXPORT_METHODS(DiffuseMtd)

enum DiffuseParams {
   p_color,
};

node_parameters
{
   AiParameterRGB("color", 0.8f, 0.8f, 0.8f);
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

   AtRGB color = AiShaderEvalParamRGB(p_color);
   if (AiColorIsSmall(color))
      return;

   sg->out.CLOSURE() = DiffuseBSDFCreate(sg, color, sg->Nf);
}

node_loader
{
   if (i>0)
      return false;

   node->methods      = DiffuseMtd;
   node->output_type  = AI_TYPE_CLOSURE;
   node->name         = "wonderful_diffuse";
   node->node_type    = AI_NODE_SHADER;
   strcpy(node->version, AI_VERSION);
   return true;
}