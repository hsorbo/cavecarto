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
pip3 install pycavecarto
```

Uninstall
```
pip3 uninstall pycavecarto -y
```

Usage
```
import cavecarto
```

# License

cavecarto is released under the MIT license, see [LICENSE](LICENSE).

The exception is the bundled sparse matrix library in `src/csparse.c` and
`src/csparse.h`: CSparse, Copyright (c) 2006 Timothy A. Davis, licensed under
the GNU Lesser General Public License, version 2.1 or (at your option) any
later version, see [LICENSE.LGPL-2.1](LICENSE.LGPL-2.1).

`test/utest.h` and `test/ubench.h` are public domain (Unlicense).
