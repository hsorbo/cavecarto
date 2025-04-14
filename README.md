# building

```
mkdir build && cd build
cmake ..
make

# test
./test_suite

# bench 
python3 scripts/tmlu2h.py ~/Downloads/MEGAFILE.tmlu > test/bench.h
```