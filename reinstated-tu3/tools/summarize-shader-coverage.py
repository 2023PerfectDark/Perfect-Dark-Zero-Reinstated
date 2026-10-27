"""Summarize recorded map loads; counts show observed coverage, not completeness."""
import argparse
import collections
import json
from pathlib import Path
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("folder", type=Path, help="user/cache/pdz-mission-shaders directory")
args = parser.parse_args()
maps = collections.defaultdict(lambda: {"loads": 0, "assets": set(), "pipelines": {}})
bad_lines = 0
journal = args.folder / "map-loads.jsonl"
if journal.exists():
    for line in journal.read_text(encoding="utf-8").splitlines():
        try:
            entry = json.loads(line)
            if entry["event"] != "map_load":
                continue
            key = (entry["mode"], entry["map_id"])
            maps[key]["loads"] += 1
            if entry.get("asset"):
                maps[key]["assets"].add(entry["asset"])
        except (ValueError, KeyError, TypeError):
            bad_lines += 1
for path in args.folder.glob("*.pipelines"):
    match = re.fullmatch(r"mode-(\d+)-map-(\d+)-(.+)\.pipelines", path.name)
    if not match:
        continue
    mode, map_id = map(int, match.group(1, 2))
    hashes = set()
    for line in path.read_text().splitlines():
        try:
            hashes.add(int(line, 16))
        except ValueError:
            bad_lines += 1
    maps[mode, map_id]["pipelines"][match[3]] = len(hashes)
print(json.dumps({
    "coverage": "observed only; unvisited areas and effects may need more pipelines",
    "ignored_invalid_lines": bad_lines,
    "maps": [
        {"mode": mode, "map_id": map_id, "loads": value["loads"],
         "assets": sorted(value["assets"]), "pipelines": value["pipelines"]}
        for (mode, map_id), value in sorted(maps.items())
    ],
}, indent=2))
