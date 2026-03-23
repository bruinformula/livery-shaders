#./build/bin/StepConvertUsd --inputStepFile ./test/step/model1.STEP --outputFile ./build/model1.usdc
#./build/bin/StepConvertUsd --inputStepFile ./test/step/model1.STEP --outputFile ./build/model1-regular.usdc
#./build/bin/StepConvertUsdVariant --inputStepFile ./test/step/model1.STEP --outputFile ./build/model1-variant.usda --config scripts/low.config --config scripts/high.config 

./build/bin/StepConvertUsd --inputStepFile ./test/step/model5.STEP --outputFile ./build/model5.usdc

./build/bin/StepConvertUsd --inputStepFile ./test/step/model2.STEP --outputFile ./build/model2.usdc --renderPurposeThreshold 100.0
#./build/bin/StepConvertUsd --inputStepFile ./test/step/model3.STEP --outputFile ./build/model3.usdc
#./build/bin/StepConvertUsd --inputStepFile ./test/step/model4.STEP --outputFile ./build/model4.usdc

#./build/bin/StepConvertUsd --inputStepFile $MK10_ROOT/MK10.STEP --outputFile ./build/Mk10.usdc
./build/bin/StepConvertUsd --inputStepFile $MK11_ROOT/mk11/geometry/model/step/MK11-TOP-001-A-TOP_ASSEMBLY.STP --outputFile $MK11_ROOT/mk11/geometry/model/model-old.usdc --renderPurposeThreshold 1.0
./build/bin/StepConvertUsd --inputStepFile $MK11_ROOT/mk11/geometry/model/step/MK11-TOP-002-A-TOP_ASSEMBLY.STP --outputFile $MK11_ROOT/mk11/geometry/model/model-v1.usdc
