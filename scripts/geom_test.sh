# FILE 1: no variants
echo "FILE1: baseline (no variants)"
StepConvertUsd -i ./test/usd/model1.usda -v

echo "FILE1: explicit root path"
StepConvertUsd -i ./test/usd/model1.usda -p /Wonderful -v

echo "FILE1: prototypes path"
StepConvertUsd -i ./test/usd/model1.usda -p /Wonderful/Prototypes -v


# FILE 2: quality only
echo "FILE2: baseline (quality default)"
StepConvertUsd -i ./test/usd/model1_per_prototype_variant.usda -v

echo "FILE2: rod0 no variant (should use default=final)"
StepConvertUsd -i ./test/usd/model1_per_prototype_variant.usda \
  -p /Wonderful/Prototypes/rod0 -v

echo "FILE2: rod0 quality=draft"
StepConvertUsd -i ./test/usd/model1_per_prototype_variant.usda \
  -p /Wonderful/Prototypes/rod0{quality=draft} -v

echo "FILE2: rod0 quality=final"
StepConvertUsd -i ./test/usd/model1_per_prototype_variant.usda \
  -p /Wonderful/Prototypes/rod0{quality=final} -v

echo "FILE2: conflicting quality (draft vs final)"
StepConvertUsd -i ./test/usd/model1_per_prototype_variant.usda \
  -p /Wonderful/Prototypes/rod0{quality=draft} \
  -p /Wonderful/Prototypes/rod0{quality=final} -v

echo "FILE2: invalid quality (ultra)"
StepConvertUsd -i ./test/usd/model1_per_prototype_variant.usda \
  -p /Wonderful/Prototypes/rod0{quality=ultra} -v


# FILE 3: LOD + quality
echo "FILE3: baseline (LOD=high, quality=default)"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda -v

echo "FILE3: LOD=high only"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=high} -v

echo "FILE3: LOD=low only"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=low} -v

echo "FILE3: LOD=high, rod0 default quality"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=high}rod0 -v

echo "FILE3: LOD=high, quality=default"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=high}rod0{quality=default} -v

echo "FILE3: LOD=high, quality=draft"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=high}rod0{quality=draft} -v

echo "FILE3: LOD=high, quality=final"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=high}rod0{quality=final} -v

echo "FILE3: LOD=low, quality=default"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=low}rod0{quality=default} -v

echo "FILE3: LOD=low, quality=draft"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=low}rod0{quality=draft} -v

echo "FILE3: LOD=low, quality=final"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=low}rod0{quality=final} -v

echo "FILE3: split -p (LOD high + quality final)"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=high} \
  -p /Wonderful/Prototypes/rod0{quality=final} -v

echo "FILE3: conflicting LOD (high vs low)"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=high} \
  -p /Wonderful/Prototypes{LOD=low} -v

echo "FILE3: cross conflict (LOD high + rod0 under low)"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=high} \
  -p /Wonderful/Prototypes{LOD=low}rod0{quality=final} -v

echo "FILE3: invalid LOD"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=medium} -v

echo "FILE3: invalid quality"
StepConvertUsd -i ./test/usd/model1_prototype_and_per_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=high}rod0{quality=ultra} -v


# FILE 4: LOD only
echo "FILE4: baseline (LOD=high default)"
StepConvertUsd -i ./test/usd/model1_prototype_variant.usda -v

echo "FILE4: LOD=high"
StepConvertUsd -i ./test/usd/model1_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=high} -v

echo "FILE4: LOD=low"
StepConvertUsd -i ./test/usd/model1_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=low} -v

echo "FILE4: duplicate LOD selection (high, high)"
StepConvertUsd -i ./test/usd/model1_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=high} \
  -p /Wonderful/Prototypes{LOD=high} -v

echo "FILE4: conflicting LOD (high vs low)"
StepConvertUsd -i ./test/usd/model1_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=high} \
  -p /Wonderful/Prototypes{LOD=low} -v

echo "FILE4: invalid LOD"
StepConvertUsd -i ./test/usd/model1_prototype_variant.usda \
  -p /Wonderful/Prototypes{LOD=medium} -v