g++ -std=c++17 -O1 -DLINUX scripts/rman_metrics.cpp -I$RMANTREE/include -L$RMANTREE/lib \
    -lprman -lstats -lpxrcore -lpthread -Wl,-rpath,$RMANTREE/lib -o build/bin/rman_metrics