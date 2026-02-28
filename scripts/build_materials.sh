#./build/bin/MaterialXLibraryBuilder --oslLibraryPath shaders/src -I shaders/include --libraryOutputPath ./build/autolib --autolib-include-rewrite 

./build/bin/MaterialXOSOExporter --mtlxMaterialsPath ./test/materials --osoOutputPath ./build/oso --library ./src/mtl/arnold/mtx --library ./build/autolib --writeOSLSource -I "$BLENDER_ROOT/scripts/addons_core/cycles/shader"
#./build/bin/MaterialXOSOExporter --mtlxMaterialsPath ./test/materials --osoOutputPath ./build/oso --library ./build/autolib --writeOSLSource