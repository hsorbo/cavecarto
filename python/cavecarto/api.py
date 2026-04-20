from __future__ import annotations
from dataclasses import dataclass
from datetime import datetime
from typing import List, Tuple, Union, Optional
from enum import Flag, auto
from . import _cavecarto


class Transform(Flag):
    NONE = 0
    CLOSE_LOOPS = auto()
    ADJUST_MAG_NORTH = auto()
    ADJUST_MAG_NORTH_FAST = auto()


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


def compute_station_latlon(
    stations: List[Station], datum: Optional[Fixed], flags: Transform = Transform.NONE
) -> Tuple[List[float], List[float]]:
    station_dicts = [s.to_dict() for s in stations]
    datum_dict = datum.to_dict() if datum is not None else None
    return _cavecarto.get_coords(station_dicts, datum_dict, flags.value, True)


def compute_station_xy(
    stations: List[Station], datum: Optional[Fixed], flags: Transform = Transform.NONE
) -> Tuple[List[float], List[float]]:
    station_dicts = [s.to_dict() for s in stations]
    datum_dict = datum.to_dict() if datum is not None else None
    return _cavecarto.get_coords(station_dicts, datum_dict, flags.value, False)
