"""
Render FireRed maps from the decomp graphics, with an optional route overlay.
Used to check generated routes by eye.
"""

import os
import re
from PIL import Image, ImageDraw

from frlg_data import NUM_TILES_IN_PRIMARY, NUM_METATILES_IN_PRIMARY, NUM_PALS_IN_PRIMARY


def _load_pal(path):
    with open(path) as f:
        lines = [l.strip() for l in f if l.strip()]
    colors = []
    for l in lines[3:]:
        r, g, b = (int(v) for v in l.split()[:3])
        colors.append((r, g, b))
    while len(colors) < 16:
        colors.append((0, 0, 0))
    return colors


class TilesetGfx:
    def __init__(self, root, folder):
        base = os.path.join(root, folder)
        img = Image.open(os.path.join(base, "tiles.png"))
        if img.mode != "P":
            img = img.convert("P")
        self.width_tiles = img.width // 8
        self.pixels = img.load()
        self.img = img
        self.palettes = []
        pal_dir = os.path.join(base, "palettes")
        for i in range(16):
            p = os.path.join(pal_dir, "%02d.pal" % i)
            self.palettes.append(_load_pal(p) if os.path.exists(p) else [(255, 0, 255)] * 16)

    def tile_count(self):
        return self.width_tiles * (self.img.height // 8)

    def index(self, tile, px, py):
        tx = tile % self.width_tiles
        ty = tile // self.width_tiles
        return self.pixels[tx * 8 + px, ty * 8 + py] & 0xF


class Renderer:
    def __init__(self, decomp):
        self.d = decomp
        self._gfx = {}
        self._metatile_cache = {}

    def _gfx_folder(self, symbol):
        """Folder with tiles.png and palettes/ for a tileset. Usually the same
        folder as the metatiles, but some tilesets share the graphics of
        another one (Silph Co. uses the Condominiums graphics)."""
        folder = self.d._tileset_folders[symbol]
        if os.path.exists(os.path.join(self.d.root, folder, "tiles.png")):
            return folder
        if not hasattr(self, "_gfx_paths"):
            root = self.d.root
            headers = open(os.path.join(root, "src/data/tilesets/headers.h")).read()
            graphics = open(os.path.join(root, "src/data/tilesets/graphics.h")).read()
            tiles = dict(re.findall(r"const struct Tileset (gTileset_\w+) =\s*\{.*?\.tiles = (gTilesetTiles_\w+)", headers, re.S))
            paths = dict(re.findall(r'const u32 (gTilesetTiles_\w+)\[\] = INCBIN_U32\("([^"]+)/tiles\.4bpp', graphics))
            self._gfx_paths = {k: paths[v] for k, v in tiles.items() if v in paths}
        return self._gfx_paths.get(symbol, folder)

    def gfx(self, symbol):
        if symbol not in self._gfx:
            self._gfx[symbol] = TilesetGfx(self.d.root, self._gfx_folder(symbol))
        return self._gfx[symbol]

    def _metatile_image(self, layout, metatile):
        key = (layout.primary, layout.secondary, metatile)
        if key in self._metatile_cache:
            return self._metatile_cache[key]
        prim = self.d.tileset(layout.primary)
        sec = self.d.tileset(layout.secondary)
        gprim = self.gfx(layout.primary)
        gsec = self.gfx(layout.secondary)
        if metatile < NUM_METATILES_IN_PRIMARY:
            entries = prim.metatiles[metatile * 8: metatile * 8 + 8]
        else:
            i = metatile - NUM_METATILES_IN_PRIMARY
            entries = sec.metatiles[i * 8: i * 8 + 8]
        img = Image.new("RGB", (16, 16), (0, 0, 0))
        px = img.load()
        if len(entries) < 8:
            self._metatile_cache[key] = img
            return img
        for layer in range(2):
            for k in range(4):
                e = entries[layer * 4 + k]
                tile = e & 0x3FF
                hflip = (e >> 10) & 1
                vflip = (e >> 11) & 1
                pal = (e >> 12) & 0xF
                if tile < NUM_TILES_IN_PRIMARY:
                    g, t = gprim, tile
                else:
                    g, t = gsec, tile - NUM_TILES_IN_PRIMARY
                if t >= g.tile_count():
                    continue
                palette = (gprim if pal < NUM_PALS_IN_PRIMARY else gsec).palettes[pal]
                ox = (k % 2) * 8
                oy = (k // 2) * 8
                for yy in range(8):
                    for xx in range(8):
                        sx = 7 - xx if hflip else xx
                        sy = 7 - yy if vflip else yy
                        ci = g.index(t, sx, sy)
                        if ci == 0 and layer == 1:
                            continue
                        if ci == 0 and layer == 0:
                            ci = 0
                        px[ox + xx, oy + yy] = palette[ci]
        self._metatile_cache[key] = img
        return img

    def render_map(self, m, scale=2):
        img = Image.new("RGB", (m.width * 16, m.height * 16))
        for y in range(m.height):
            for x in range(m.width):
                block = m.layout.block(x, y)
                img.paste(self._metatile_image(m.layout, block & 0x3FF), (x * 16, y * 16))
        if scale != 1:
            img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
        return img


def draw_route(renderer, world, m, legs_on_map, path_png, scale=2, title=None):
    """
    legs_on_map: list of dicts {points: [(x,y),...], color, start, end, label}
    """
    base = renderer.render_map(m, scale)
    over = Image.new("RGBA", base.size, (0, 0, 0, 0))
    dr = ImageDraw.Draw(over)
    s = 16 * scale
    blocked, soft, sight = world.objects(m)
    #  Tiles with collision: light dark tint. Encounter tiles: green tint.
    for y in range(m.height):
        for x in range(m.width):
            _, col, elev, beh, enc = m.tile(x, y)
            box = [x * s, y * s, (x + 1) * s - 1, (y + 1) * s - 1]
            if col:
                dr.rectangle(box, fill=(0, 0, 0, 60))
            elif enc == 1:
                dr.rectangle(box, fill=(0, 200, 0, 50))
            if (x, y) in soft and soft[(x, y)] >= 10:
                dr.rectangle(box, fill=(255, 160, 0, 70))
            if (x, y) in sight:
                dr.rectangle(box, fill=(255, 0, 0, 70))
    for (x, y) in blocked:
        dr.rectangle([x * s + 3, y * s + 3, (x + 1) * s - 4, (y + 1) * s - 4], outline=(255, 120, 0, 255), width=2)
    for (x, y) in world.active_triggers(m):
        dr.rectangle([x * s + 2, y * s + 2, (x + 1) * s - 3, (y + 1) * s - 3], outline=(200, 0, 255, 255), width=2)
    for w in m.warps:
        x, y = w["x"], w["y"]
        dr.rectangle([x * s + 1, y * s + 1, (x + 1) * s - 2, (y + 1) * s - 2], outline=(0, 160, 255, 255), width=2)
    for leg in legs_on_map:
        pts = [(x * s + s // 2, y * s + s // 2) for x, y in leg["points"]]
        color = leg.get("color", (255, 0, 0, 255))
        if len(pts) >= 2:
            dr.line(pts, fill=color, width=max(2, scale * 2))
        if pts:
            x0, y0 = pts[0]
            dr.ellipse([x0 - 5, y0 - 5, x0 + 5, y0 + 5], fill=(255, 255, 255, 255), outline=color, width=2)
            x1, y1 = pts[-1]
            dr.rectangle([x1 - 5, y1 - 5, x1 + 5, y1 + 5], fill=color)
            if leg.get("label"):
                dr.text((x0 + 7, y0 - 7), leg["label"], fill=(255, 255, 255, 255))
    out = Image.alpha_composite(base.convert("RGBA"), over)
    if title:
        bar = Image.new("RGBA", (out.width, 18), (20, 20, 20, 255))
        ImageDraw.Draw(bar).text((4, 3), title, fill=(255, 255, 255, 255))
        full = Image.new("RGBA", (out.width, out.height + 18))
        full.paste(bar, (0, 0))
        full.paste(out, (0, 18))
        out = full
    out.convert("RGB").save(path_png)
