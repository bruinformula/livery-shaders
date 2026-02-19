./build/bin/STEPConvertUSD --inputSTEPFile ./test/step/model1.STEP --outputDir ./build/
#./build/bin/STEPConvertUSD --inputSTEPFile ./test/step/model2.STEP
#!/bin/bash

start=$(date +%s)

#./build/bin/STEPConvertUSD --inputSTEPFile ../../Mk10/MK10.STEP --outputDir ./build/

end=$(date +%s)
elapsed=$((end - start))

echo "Time taken: ${elapsed} seconds"

# For MK10 the cached usd write takes 68 seconds
# this is a 700MB heavy Step file 