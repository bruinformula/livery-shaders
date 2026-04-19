testshade -g 1024 1024 --param splatterScale 16.0 ./build/autolib/oso/AutolibSplatter.oso -o height splatter-height.exr
testshade -g 1024 1024 --param splatterScale 16.0 ./build/autolib/oso/AutolibSplatter.oso -o mask splatter-mask.exr
testshade -g 1024 1024 --param splatterScale 16.0 ./build/autolib/oso/AutolibSplatter.oso -o bumpNormal splatter-bump-normal.exr