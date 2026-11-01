"""
Loads Pokemon FireRed map data from the pret/pokefirered decompilation.

Everything here is read straight from the decomp files:
  data/maps/<Map>/map.json          objects, warps, triggers, connections
  data/layouts/layouts.json         size + tilesets of every layout
  data/layouts/<Layout>/map.bin     16-bit block per tile (metatile, collision, elevation)
  data/tilesets/.../metatile_attributes.bin   32-bit attributes per metatile (behavior)

Block format (include/global.fieldmap.h):
  bits 0-9   metatile id
  bits 10-11 collision
  bits 12-15 elevation
Metatile attributes (src/fieldmap.c, sMetatileAttrMasks):
  bits 0-8   behavior (include/constants/metatile_behaviors.h)
  bits 24-26 encounter type
"""

import json
import os
import re
import struct
from functools import lru_cache

NUM_METATILES_IN_PRIMARY = 640
NUM_TILES_IN_PRIMARY = 640
NUM_PALS_IN_PRIMARY = 7

DIRS = {
    "up": (0, -1),
    "down": (0, 1),
    "left": (-1, 0),
    "right": (1, 0),
}
OPPOSITE = {"up": "down", "down": "up", "left": "right", "right": "left"}


class Behaviors:
    """Metatile behavior ids, parsed from include/constants/metatile_behaviors.h."""

    def __init__(self, root):
        self.by_name = {}
        self.by_id = {}
        path = os.path.join(root, "include/constants/metatile_behaviors.h")
        with open(path) as f:
            for line in f:
                m = re.match(r"#define (MB_\w+) (0x[0-9A-Fa-f]+|\d+)", line)
                if m:
                    value = int(m.group(2), 0)
                    self.by_name[m.group(1)] = value
                    self.by_id.setdefault(value, m.group(1))

    def name(self, value):
        return self.by_id.get(value, "MB_0x%02X" % value)

    def __getattr__(self, item):
        if item.startswith("MB_"):
            return self.by_name[item]
        raise AttributeError(item)


class Tileset:
    def __init__(self, root, symbol, folder):
        self.symbol = symbol
        self.folder = folder
        base = os.path.join(root, folder)
        with open(os.path.join(base, "metatile_attributes.bin"), "rb") as f:
            raw = f.read()
        self.attributes = list(struct.unpack("<%dI" % (len(raw) // 4), raw))
        with open(os.path.join(base, "metatiles.bin"), "rb") as f:
            raw = f.read()
        self.metatiles = list(struct.unpack("<%dH" % (len(raw) // 2), raw))


class Layout:
    def __init__(self, root, info):
        self.id = info["id"]
        self.name = info["name"]
        self.width = info["width"]
        self.height = info["height"]
        self.primary = info["primary_tileset"]
        self.secondary = info["secondary_tileset"]
        with open(os.path.join(root, info["blockdata_filepath"]), "rb") as f:
            raw = f.read()
        self.blocks = list(struct.unpack("<%dH" % (len(raw) // 2), raw))

    def block(self, x, y):
        return self.blocks[y * self.width + x]


class MapData:
    def __init__(self, decomp, folder, info):
        self.decomp = decomp
        self.folder = folder
        self.id = info["id"]
        self.name = info["name"]
        self.layout = decomp.layouts[info["layout"]]
        self.width = self.layout.width
        self.height = self.layout.height
        self.connections = info.get("connections") or []
        self.objects = info.get("object_events") or []
        self.warps = info.get("warp_events") or []
        self.coord_events = info.get("coord_events") or []
        self.bg_events = info.get("bg_events") or []
        self.map_type = info.get("map_type")
        self.mapsec = info.get("region_map_section")
        self.show_map_name = info.get("show_map_name")
        self.allow_running = info.get("allow_running")

    def in_bounds(self, x, y):
        return 0 <= x < self.width and 0 <= y < self.height

    def tile(self, x, y):
        """Return (metatile, collision, elevation, behavior, encounter_type)."""
        block = self.layout.block(x, y)
        metatile = block & 0x3FF
        collision = (block >> 10) & 0x3
        elevation = (block >> 12) & 0xF
        attrs = self.decomp.metatile_attributes(self.layout, metatile)
        behavior = attrs & 0x1FF
        encounter = (attrs >> 24) & 0x7
        return metatile, collision, elevation, behavior, encounter

    def warp_at(self, x, y):
        for i, w in enumerate(self.warps):
            if w["x"] == x and w["y"] == y:
                return i, w
        return None, None

    def object_by_local_id(self, local_id):
        for i, o in enumerate(self.objects):
            if o.get("local_id") == local_id:
                return i, o
        return None, None


class Decomp:
    def __init__(self, root):
        self.root = root
        self.behaviors = Behaviors(root)
        with open(os.path.join(root, "data/layouts/layouts.json")) as f:
            layouts = json.load(f)["layouts"]
        self.layouts = {}
        for info in layouts:
            if "id" in info and "blockdata_filepath" in info:
                self.layouts[info["id"]] = Layout(root, info)
        self._tileset_folders = self._parse_tileset_folders()
        self.previews = self._parse_map_previews()
        self._tilesets = {}
        self.maps = {}
        maps_dir = os.path.join(root, "data/maps")
        for folder in sorted(os.listdir(maps_dir)):
            path = os.path.join(maps_dir, folder, "map.json")
            if not os.path.exists(path):
                continue
            with open(path) as f:
                info = json.load(f)
            if info.get("layout") not in self.layouts:
                continue
            m = MapData(self, folder, info)
            self.maps[m.id] = m

    # ---------------------------------------------------------------- map previews
    def _parse_map_previews(self):
        """
        mapsec -> "FOREST" or "CAVE", from sMapPreviewScreenData in
        src/map_preview_screen.c. Entering one of these areas from another
        area shows its preview picture, and the player cannot move until it
        is gone: a forest-type preview is drawn over the new map for
        40-120 frames plus a cross-fade (src/map_preview_screen.c,
        Task_RunMapPreviewScreenForest), a cave-type preview is a full
        screen picture before the map fades in (src/fldeff_flash.c,
        Task_MapPreviewScreen_0), which holding B skips.
        """
        text = open(os.path.join(self.root, "src/map_preview_screen.c")).read()
        result = {}
        for mapsec, kind in re.findall(
            r"\.mapsec = (MAPSEC_\w+),\s*\.type = MPS_TYPE_(FOREST|CAVE)", text
        ):
            result[mapsec] = kind
        return result

    def warp_preview(self, src_map_id, dst_map_id):
        """The preview shown when warping from src to dst: None, "FOREST" or "CAVE"."""
        src = self.map(src_map_id)
        dst = self.map(dst_map_id)
        if src.mapsec == dst.mapsec:
            return None
        return self.previews.get(dst.mapsec)

    # ---------------------------------------------------------------- tilesets
    def _parse_tileset_folders(self):
        """Map gTileset_X -> folder via headers.h and metatiles.h."""
        headers = open(os.path.join(self.root, "src/data/tilesets/headers.h")).read()
        metatiles = open(os.path.join(self.root, "src/data/tilesets/metatiles.h")).read()
        attr_paths = dict(re.findall(
            r'const u32 (gMetatileAttributes_\w+)\[\] = INCBIN_U32\("([^"]+)/metatile_attributes\.bin"\)',
            metatiles,
        ))
        result = {}
        for symbol, attr_symbol in re.findall(
            r"const struct Tileset (gTileset_\w+) =\s*\{.*?\.metatileAttributes = (gMetatileAttributes_\w+)",
            headers, re.S,
        ):
            if attr_symbol in attr_paths:
                result[symbol] = attr_paths[attr_symbol]
        return result

    def tileset(self, symbol):
        if symbol not in self._tilesets:
            self._tilesets[symbol] = Tileset(self.root, symbol, self._tileset_folders[symbol])
        return self._tilesets[symbol]

    def metatile_attributes(self, layout, metatile):
        if metatile < NUM_METATILES_IN_PRIMARY:
            ts = self.tileset(layout.primary)
            index = metatile
        else:
            ts = self.tileset(layout.secondary)
            index = metatile - NUM_METATILES_IN_PRIMARY
        if index < len(ts.attributes):
            return ts.attributes[index]
        return 0

    # ---------------------------------------------------------------- helpers
    def map(self, map_id):
        if not map_id.startswith("MAP_"):
            map_id = "MAP_" + map_id
        return self.maps[map_id]

    def connection_target(self, m, x, y):
        """
        For a coordinate just outside map m, return (map, x, y) in the
        connected map, or None if there is no connection there.
        Mirrors the connection math in src/fieldmap.c.
        """
        for c in m.connections:
            d = c["direction"]
            if d not in DIRS:
                continue
            other = self.maps.get(c["map"])
            if other is None:
                continue
            off = c["offset"]
            if d == "up" and y < 0:
                nx, ny = x - off, other.height + y
            elif d == "down" and y >= m.height:
                nx, ny = x - off, y - m.height
            elif d == "left" and x < 0:
                nx, ny = other.width + x, y - off
            elif d == "right" and x >= m.width:
                nx, ny = x - m.width, y - off
            else:
                continue
            if other.in_bounds(nx, ny):
                return other, nx, ny
        return None
