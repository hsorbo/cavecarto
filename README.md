# building

```
mkdir build && cd build
cmake ..
make

# test
./test_suite

# bench 
python3 scripts/tmlu2h.py ~/Downloads/MEGAFILE.tmlu > test/bench.h
make
./bench_suite


# build release
mkdir release && cd release
cmake -DCMAKE_BUILD_TYPE=Release release ..
make
```