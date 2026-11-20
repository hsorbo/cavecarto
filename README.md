# cavecarto

Computes station positions for cave survey stick maps. You give it stations,
either fixed (lat/lon) or relative to a parent (azimuth, length, depth), and it
returns a position for every station, as local XY in meters or as lat/lon.

It can apply two corrections along the way:

- **Loop closure**: where survey lines meet again, the misclosure is spread
  over the loops with a least squares fit.
- **Magnetic north**: compass azimuths are corrected for magnetic declination
  on the date each station was surveyed, using the IGRF-14 model.

It is a small C99 library with zero dependencies, and has Python and Swift
bindings.

To drop it into another project, `sh amalgamate.sh` writes the whole library
to a single source file, `cavecarto.c`, to compile alongside
`include/cavecarto.h`.

# Performance

Time for one call to `cc_compute_station_xy` on a real survey of 75,720
stations with 1,691 loop closures, on an Apple M4 Pro (`-O3`):

| Transform                               | Time   |
|-----------------------------------------|--------|
| `CC_TRANSFORM_NONE`                     | 0.8 ms |
| `CC_TRANSFORM_CLOSE_LOOPS` (CSparse)    | 2.8 ms |
| `CC_TRANSFORM_CLOSE_LOOPS` (Accelerate) | 3.1 ms |
| `CC_TRANSFORM_MAG_NORTH_FAST`           | 1.0 ms |
| `CC_TRANSFORM_MAG_NORTH`                | 43 ms  |

Loop closure uses the bundled CSparse solver, or Apple's Accelerate framework
when built with `USE_ACCELERATE` (the default on Apple platforms). The other
transforms do not depend on the solver. Run `./bench_suite` to measure on your
own data.

# Building

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
```python
from datetime import datetime
from cavecarto import Station, Fixed, Relative, Transform, compute_station_latlon

day = datetime(2024, 3, 1)
stations = [
    Station(depth=0, pos=Fixed(lat=60.0, lon=10.0), date=day),
    Station(depth=5, pos=Relative(azimuth=0, length=30, parent_index=0), date=day),
    Station(depth=8, pos=Relative(azimuth=90, length=30, parent_index=1), date=day),
    Station(depth=8, pos=Relative(azimuth=180, length=30, parent_index=2), date=day),
    # this shot ties back into station 0, closing the loop
    Station(depth=5, pos=Relative(azimuth=265, length=30, parent_index=3, closed_index=0), date=day),
]

# datum None: use the first fixed station
coords = compute_station_latlon(
    stations, None, Transform.CLOSE_LOOPS | Transform.ADJUST_MAG_NORTH
)
for lon, lat in coords:
    print(lat, lon)
```

`compute_station_xy` takes the same arguments and returns `(x, y)` in meters
from the datum, with x growing east and y growing south.

# License

cavecarto is released under the MIT license, see [LICENSE](LICENSE).

The exception is the bundled sparse matrix library in `src/csparse.c` and
`src/csparse.h`: CSparse, Copyright (c) 2006 Timothy A. Davis, licensed under
the GNU Lesser General Public License, version 2.1 or (at your option) any
later version, see [LICENSE.LGPL-2.1](LICENSE.LGPL-2.1).

`test/utest.h` and `test/ubench.h` are public domain (Unlicense).
