from __future__ import annotations
from dataclasses import dataclass
from datetime import datetime
from typing import List, Tuple, Union, Optional
from enum import Flag, auto
from . import _surveylib


class CoordFlags(Flag):
    NONE = 1
    LAT_LON = auto()
    CLOSE_LOOPS = auto()
    ADJUST_MAG_NORTH = auto()


@dataclass
class Fixed:
    lat: float
    lon: float

    def to_dict(self):
        return {"lat": self.lat, "lon": self.lon}


@dataclass
class Relative:
    azimuth: float
    length: float
    parent_index: int
    closed_index: Optional[int] = None

    def to_dict(self):
        return {
            "azimuth": self.azimuth,
            "length": self.length,
            "parent_index": self.parent_index,
            "closed_index": self.closed_index,
        }


@dataclass
class Station:
    depth: float
    pos: Union[Fixed, Relative]
    date: datetime

    def to_dict(self):
        out = {"depth": self.depth}
        out["date"] = self.date
        if isinstance(self.pos, Fixed):
            out["location"] = self.pos.to_dict()
        elif isinstance(self.pos, Relative):
            out["distance"] = self.pos.to_dict()
        else:
            raise TypeError("pos must be Fixed or Relative")
        return out


def get_coords(
    stations: List[Station], datum: Optional[Fixed], flags: CoordFlags = CoordFlags.NONE
) -> Tuple[List[float], List[float]]:
    if not isinstance(flags, CoordFlags):
        raise TypeError(f"Expected CoordFlags, got {type(flags).__name__}")
    if not isinstance(stations, list):
        raise TypeError("stations must be a list of Station objects")
    station_dicts = []
    for s in stations:
        if not isinstance(s, Station):
            raise TypeError(f"stations is not a Station (got {type(s).__name__})")
        station_dicts.append(s.to_dict())
    datum_dict = datum.to_dict() if datum is not None else None
    return _surveylib.get_coords(station_dicts, datum_dict, flags.value)
