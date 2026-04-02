
StepConvertUsd -i ./test/step/wonderful_model.usda -v
StepConvertUsd -i ./test/step/wonderful_model_variant.usda -p /Wonderful{LOD=high}
StepConvertUsd -i ./test/step/wonderful_model_variant_variant.usda -v
StepConvertUsd -i ./test/step/wonderful_model_variant_variant.usda -p /Wonderful{LOD=high}Prototypes/rod0{quality=final} -v
StepConvertUsd -i ./test/step/wonderful_model_variant_variant.usda -p /Wonderful{LOD=low}Prototypes/rod0{quality=final} -v
StepConvertUsd -i ./test/step/wonderful_model_variant_variant.usda -p /Wonderful{LOD=high} -p /Wonderful{LOD=high}Prototypes/rod0{quality=low} -v

#StepConvertUsd --inputUsdFile $MK11_ROOT/mk11/geometry/model/model-v1/model-v1.usda
#StepConvertUsd --inputUsdFile $MK11_ROOT/mk11/geometry/model/model-old/model-old.usda