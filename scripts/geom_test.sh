./build/bin/StepConvertUsd --inputStepFile ./test/step/model1.Step --outputFile ./build/model1.usdc
./build/bin/StepConvertUsd --inputStepFile ./test/step/model1.Step --outputFile ./build/model1-regular.usdc
./build/bin/StepConvertUsdVariant --inputStepFile ./test/step/model1.Step --outputFile ./build/model1-variant.usda --config scripts/low.config --config scripts/high.config 

#./build/bin/StepConvertUsd --inputStepFile ./test/step/model2.Step --outputFile ./build/model2.usdc
#./build/bin/StepConvertUsd --inputStepFile ./test/step/model3.Step --outputFile ./build/model3.usdc
#./build/bin/StepConvertUsd --inputStepFile ./test/step/model4.Step --outputFile ./build/model4.usdc

#./build/bin/StepConvertUsd --inputStepFile $MK10_ROOT/MK10.Step --outputFile ./build/Mk10.usdc
#./build/bin/StepConvertUsd --inputStepFile $MK11_ROOT/car/step_files/MK11-TOP-001-A-TOP_ASSEMBLY.STP --outputFile ./build/Mk11.usdc