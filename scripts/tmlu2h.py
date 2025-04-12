#!/usr/bin/env python3
import re
import sys
from typing import Optional, Union
import xml.etree.ElementTree as ET
from dataclasses import dataclass


@dataclass
class Closure:
    a: int
    b: int


@dataclass
class Fixed:
    latitude: float
    longitude: float


@dataclass
class Relative:
    distance: float
    azimuth: float
    linked: Optional[int]


@dataclass
class SurveyData:
    id: int
    parent: Optional[int]
    station: Union[Fixed, Relative]
    depth: float
    date: str
    color: str
    comment: str
    name: str
    section: str
    explorer: str
    excluded: str

    def is_fixed(self) -> bool:
        return isinstance(self.station, Fixed)

    def is_linked(self) -> bool:
        return isinstance(self.station, Relative) and self.station.linked != None


# TMLU parsing
def read_srvd(filename: str):
    """Generates dictionaries for each SRVD-entry in tmlu-file"""
    for data in ET.parse(filename).find(".//Data"):
        yield dict([(x.tag, x.text) for x in data])


def parse_srvd(d: dict):
    id = int(d["ID"])
    parent = int(d["FRID"])
    if d["TY"] == "CLOSURE":
        return Closure(a=int(d["FRID"]), b=int(d["CID"]))
    fixed = Fixed(latitude=float(d["LT"]), longitude=float(d["LGT"]))
    relative = Relative(distance=float(d["LG"]), azimuth=float(d["AZ"]), linked=None)
    return SurveyData(
        id=id,
        station=fixed if d["TY"] == "START" else relative,
        depth=float(d["DP"]),
        parent=parent if parent >= 0 else None,
        color=d["CL"],
        comment=d["CM"],
        name=d["NM"],
        section=d["SC"],
        explorer=d["EX"],
        excluded=d["EXC"],
        date=d["DT"],
    )


def connect_components(m: list[tuple[int, int]]) -> list[set[int]]:
    """
    Given a list of tuples, where each tuple is a pair of integers,
    this function returns a list of sets, where each set contains
    all integers that are connected to each other in the input list.
    Example: [(a, b), (c, b), (e, f)] -> [{a, b, c}, {e, f}]

    This is used to group closed stations.
    """
    components = []

    for a, b in m:
        merged_sets = []
        for c in components:
            if a in c or b in c:
                c.add(a)
                c.add(b)
                merged_sets.append(c)

        if merged_sets:
            new_set = set.union(*merged_sets)
            components = [c for c in components if c not in merged_sets]
            components.append(new_set)
        else:
            components.append({a, b})

    return components


def sort_closure(stations: dict[int, SurveyData], closure: set[int]):
    start = [id for id in closure if isinstance(stations[id], Fixed)]
    if len(start) > 1:
        raise ValueError(f"Multiple start stations in closure {closure}")
    master = start[0] if len(start) == 1 else min(closure)
    return (master, [x for x in list(closure) if x != master])


def read_tmlu(file):
    all = [parse_srvd(x) for x in read_srvd(file)]
    stations = [x for x in all if isinstance(x, SurveyData)]
    closures = [(x.a, x.b) for x in all if isinstance(x, Closure)]
    stations_dict = dict([(x.id, x.station) for x in stations])
    for closure in connect_components(closures):
        (master, slaves) = sort_closure(stations_dict, closure)
        for x in slaves:
            stations_dict[x].linked = master
    return stations


if __name__ == "__main__":
    stations = read_tmlu(sys.argv[1])
    print("#include \"cavecarto.h\"")
    print("#include <time.h>")
    print("#include <stdbool.h>")
    print(f"int station_count = {len(stations)};")
    print(f"station stations[{len(stations)}] = {{")
    ids = {}
    for (i, s) in enumerate(stations):
        ids[s.id] = i
         
    for station in stations:
        (year, month, day) = map(int, station.date.split("-"))
        data = map(
            lambda x: f"{x[0]}={x[1]}",
            [
                (".depth", station.depth),
                (".fixed", "true" if station.is_fixed() else "false"),
                (".parent", "NULL" if station.is_fixed() else f"&stations[{ids[station.parent]}]"),
                (".closure", "NULL" if not station.is_linked() else f"&stations[{ids[station.station.linked]}]"),
                (
                    (".location", f"{{.lat={station.station.latitude}, .lon={station.station.longitude} }}")
                    if station.is_fixed()
                    else (".distance", f"{{.azimuth={station.station.azimuth}, .length={station.station.distance} }}")
                ),
                (".date", f"{{ .tm_year = {year} - 1900, .tm_mon = {month}-1, .tm_mday = {day}, .tm_hour = 0, .tm_min  = 0, .tm_sec  = 0, .tm_isdst = -1}}"),
                (".userData", f"(void*) {station.id}"),
            ],
        )
        print(f"{{{str.join(", ", data)}}},")
    print("};")
