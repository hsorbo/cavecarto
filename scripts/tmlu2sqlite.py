#!/usr/bin/env python3
import re
from typing import Optional, Union
import xml.etree.ElementTree as ET
from dataclasses import dataclass
import sqlite3
import argparse


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


create = """
CREATE TABLE IF NOT EXISTS stations (
    id INTEGER PRIMARY KEY,
    parent INTEGER NULL,
    depth REAL,
    date TEXT,
    latitude REAL,
    longitude REAL,
    distance REAL,
    azimuth REAL,
    linked INTEGER NULL,
    fixed BOOLEAN,
    FOREIGN KEY(linked) REFERENCES stations(id),
    FOREIGN KEY(parent) REFERENCES stations(id)
);

CREATE TABLE IF NOT EXISTS tags (
    id INTEGER PRIMARY KEY,
    station_id INTEGER,
    name TEXT,
    FOREIGN KEY(station_id) REFERENCES stations(id)
);

CREATE TABLE IF NOT EXISTS survey_data (
    id INTEGER PRIMARY KEY,
    station_id INTEGER,
    key TEXT,
    value TEXT,
    FOREIGN KEY(station_id) REFERENCES stations(id)
);

CREATE TABLE IF NOT EXISTS divers (
    id INTEGER PRIMARY KEY,
    name TEXT
);

CREATE TABLE IF NOT EXISTS survey (
    id INTEGER PRIMARY KEY,
    station INTEGER,
    diver INTEGER,
    FOREIGN KEY (station) REFERENCES stations(id),
    FOREIGN KEY (diver) REFERENCES divers(id)
);

CREATE TABLE IF NOT EXISTS exploration (
    id INTEGER PRIMARY KEY,
    station INTEGER,
    diver INTEGER,
    FOREIGN KEY (station) REFERENCES stations(id),
    FOREIGN KEY (diver) REFERENCES divers(id)
);
"""


insert_station = """
    INSERT INTO  stations(id, parent, depth, date, latitude, longitude, distance, azimuth, linked, fixed) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
"""

if __name__ == "__main__":
    argparse.ArgumentParser(description="Convert TMLU-files to SQLite")
    parser = argparse.ArgumentParser()
    parser.add_argument("tmlu", help="TMLU-file to convert")
    parser.add_argument("db", help="SQLite database to write to")
    args = parser.parse_args()
    exp_rx = re.compile(".*<Explorer>(?P<explorers>.*?)<.*")
    srv_rx = re.compile(".*<Surveyor>(?P<surveyors>.*?)<.*")
    divers = {}
    stations = read_tmlu(args.tmlu)
    con = sqlite3.connect(args.db)
    cur = con.cursor()
    cur.executescript(create)

    data = [
        (
            srvd.id,
            srvd.parent,
            srvd.depth,
            srvd.date,
            srvd.station.latitude if srvd.is_fixed() else None,
            srvd.station.longitude if srvd.is_fixed() else None,
            srvd.station.distance if not srvd.is_fixed() else None,
            srvd.station.azimuth if not srvd.is_fixed() else None,
            srvd.station.linked if not srvd.is_fixed() else None,
            srvd.is_fixed(),
        )
        for srvd in stations
    ]
    cur.executemany(insert_station, data)
    for x in stations:
        if x.excluded == "true":
            cur.execute(
                "INSERT INTO tags(station_id, name) VALUES(?, 'ariane-excluded')",
                (x.id,),
            )
        if x.comment != None and x.comment.strip() != "":
            cur.execute(
                "INSERT INTO survey_data(station_id, key, value) VALUES(?, 'comment', ?)",
                (x.id, x.comment),
            )
        if x.color != None and x.color.strip() != "":
            cur.execute(
                "INSERT INTO survey_data(station_id, key, value) VALUES(?, 'ariane-color', ?)",
                (x.id, x.color),
            )
        if x.name != None and x.name.strip() != "":
            cur.execute(
                "INSERT INTO survey_data(station_id, key, value) VALUES(?, 'name', ?)",
                (x.id, x.name),
            )
        if x.section != None and x.section.strip() != "":
            cur.execute(
                "INSERT INTO survey_data(station_id, key, value) VALUES(?, 'section', ?)",
                (x.id, x.section),
            )
        m_exp = exp_rx.match(x.explorer) if x.explorer != None else None
        if m_exp != None:
            for diver in m_exp.group("explorers").split(","):
                diver = diver.strip()
                if diver == "":
                    continue
                if diver not in divers:
                    cur.execute("INSERT INTO divers (name) VALUES (?)", (diver,))
                    divers[diver] = cur.lastrowid
                cur.execute(
                    "INSERT INTO exploration (station, diver) VALUES (?, ?)",
                    (x.id, divers[diver]),
                )

        m_srv = srv_rx.match(x.explorer) if x.explorer != None else None
        if m_srv != None:
            for diver in m_srv.group("surveyors").split(","):
                diver = diver.strip()
                if diver == "":
                    continue
                if diver not in divers:
                    cur.execute("INSERT INTO divers (name) VALUES (?)", (diver,))
                    divers[diver] = cur.lastrowid
                cur.execute(
                    "INSERT INTO survey (station, diver) VALUES (?, ?)",
                    (x.id, divers[diver]),
                )

    con.commit()
    con.execute("VACUUM")
