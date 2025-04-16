# building

```
mkdir build && cd build
cmake ..
cmake -G Xcode ..
cmake -DCMAKE_BUILD_TYPE=Release build ..
make

# test
./test_suite

# bench 
python3 scripts/tmlu2h.py ~/Downloads/MEGAFILE.tmlu > test/bench.h
```

# Python

Install
```
pip3 install .
```

Uninstall
```
pip3 uninstall surveylib -y
```