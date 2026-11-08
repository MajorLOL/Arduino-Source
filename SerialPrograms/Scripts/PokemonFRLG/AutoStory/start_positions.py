"""Guide of start positions: for every segment and every step the program can
resume at (after a save), where the player stands, with a picture.

python3 start_positions.py <out>/story.json <pokefirered> <guide_dir> [CHARMANDER] [--images-url URL]
(story.json from generate.py; it writes START-POSITIONS.md and images/. With
--images-url the page links the pictures there, e.g. the assets of a release,
so the page is one file that works on a wiki.)
"""
import json
import os
import re
import struct
import sys

GEN = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, GEN)
from frlg_data import Decomp          # noqa: E402
from render import Renderer           # noqa: E402
from PIL import Image, ImageDraw      # noqa: E402

args = list(sys.argv[1:])
images_url = "images/"
if "--images-url" in args:
    k = args.index("--images-url")
    images_url = args[k + 1].rstrip("/") + "/"
    del args[k:k + 2]
story_path, decomp_root, out_dir = args[0:3]
variant = args[3] if len(args) > 3 else "CHARMANDER"

d = Decomp(decomp_root)
r = Renderer(d)
data = json.load(open(story_path))[variant]
segs = data["segments"]
legs = {l["id"]: l for l in data["legs"]}
os.makedirs(os.path.join(out_dir, "images"), exist_ok=True)

FACING_EN = {"up": "up", "down": "down", "left": "left", "right": "right", None: "-"}


def readable(name):
    words = []
    for part in name.split("_"):
        part = re.sub(r"(?<=[a-z])(?=[A-Z0-9])|(?<=[A-Z])(?=[A-Z][a-z])", " ", part)
        words.append(part)
    return " ".join(words)


# ---- Object sprites (first frame of each graphics id) -------------------
def load_sprites():
    root = decomp_root
    od = os.path.join(root, "src/data/object_events")
    ptrs = dict(re.findall(r"\[(OBJ_EVENT_GFX_\w+)\]\s*=\s*&(gObjectEventGraphicsInfo_\w+)",
                           open(os.path.join(od, "object_event_graphics_info_pointers.h")).read()))
    info = {}
    text = open(os.path.join(od, "object_event_graphics_info.h")).read()
    for sym, body in re.findall(r"const struct ObjectEventGraphicsInfo (gObjectEventGraphicsInfo_\w+) = \{(.*?)\};", text, re.S):
        m = re.search(r"\.images = (\w+)", body)
        if m:
            info[sym] = m.group(1)
    pics = {}
    text = open(os.path.join(od, "object_event_pic_tables.h")).read()
    for sym, body in re.findall(r"static const struct SpriteFrameImage (\w+)\[\] = \{(.*?)\};", text, re.S):
        m = re.search(r"overworld_frame\((\w+), (\d+), (\d+), (\d+)\)", body)
        if m:
            pics[sym] = (m.group(1), int(m.group(2)) * 8, int(m.group(3)) * 8)
    files = dict(re.findall(r"const u16 (gObjectEventPic_\w+)\[\] = INCBIN_U16\(\"([^\"]+)\.4bpp\"\)",
                            open(os.path.join(od, "object_event_graphics.h")).read()))
    out = {}
    for gfx, sym in ptrs.items():
        table = info.get(sym)
        if table not in pics:
            continue
        pic, w, h = pics[table]
        if pic not in files:
            continue
        path = os.path.join(root, files[pic] + ".png")
        if os.path.exists(path):
            out[gfx] = (path, w, h)
    return out


SPRITES = load_sprites()
_sprite_cache = {}


def sprite(gfx, facing="down"):
    """RGBA frame of a sprite: frame 0 down, 1 up, 2 left (right: mirrored)."""
    key = (gfx, facing)
    if key in _sprite_cache:
        return _sprite_cache[key]
    if gfx not in SPRITES:
        _sprite_cache[key] = None
        return None
    path, w, h = SPRITES[gfx]
    im = Image.open(path)
    frames = im.width // w
    idx = {"down": 0, "up": 1, "left": 2, "right": 2}.get(facing, 0)
    if idx >= frames:
        idx = 0
    pal_img = im.convert("P") if im.mode == "P" else None
    if pal_img is not None:
        crop = pal_img.crop((idx * w, 0, idx * w + w, h))
        rgba = crop.convert("RGBA")
        px = rgba.load()
        cp = crop.load()
        for yy in range(h):
            for xx in range(w):
                if cp[xx, yy] == 0:
                    px[xx, yy] = (0, 0, 0, 0)
    else:
        rgba = im.convert("RGBA").crop((idx * w, 0, idx * w + w, h))
    if facing == "right":
        rgba = rgba.transpose(Image.FLIP_LEFT_RIGHT)
    _sprite_cache[key] = rgba
    return rgba


# ---- Map pictures with the connected maps around them ---------------------
layouts_json = json.load(open(os.path.join(decomp_root, "data/layouts/layouts.json")))["layouts"]
BORDERS = {}
for li in layouts_json:
    if "border_filepath" in li:
        raw = open(os.path.join(decomp_root, li["border_filepath"]), "rb").read()
        BORDERS[li["id"]] = list(struct.unpack("<%dH" % (len(raw) // 2), raw))
LAYOUT_ID_BY_NAME = {li["name"]: li["id"] for li in layouts_json if "name" in li}

COLS, ROWS = 15, 11     # About one Switch/GBA screen around the player.


def tile_image(m, x, y):
    """Picture of tile (x, y) of map m, also outside it (connected maps, else border)."""
    if m.in_bounds(x, y):
        return r._metatile_image(m.layout, m.layout.block(x, y) & 0x3FF)
    for c in m.connections:
        if c.get("direction") not in ("up", "down", "left", "right"):
            continue
        n = d.maps.get(c["map"])
        if n is None:
            continue
        o = int(c.get("offset", 0))
        if c["direction"] == "up":
            ox, oy = o, -n.height
        elif c["direction"] == "down":
            ox, oy = o, m.height
        elif c["direction"] == "left":
            ox, oy = -n.width, o
        else:
            ox, oy = m.width, o
        if n.in_bounds(x - ox, y - oy):
            return r._metatile_image(n.layout, n.layout.block(x - ox, y - oy) & 0x3FF)
    border = BORDERS.get(m.layout.id)
    if border and len(border) >= 4:
        b = border[(y & 1) * 2 + (x & 1)]
        return r._metatile_image(m.layout, b & 0x3FF)
    return Image.new("RGB", (16, 16), (0, 0, 0))


def picture(map_id, x, y, facing, path, title):
    m = d.maps[map_id]
    x0 = x - COLS // 2
    y0 = y - ROWS // 2
    img = Image.new("RGBA", (COLS * 16, ROWS * 16), (0, 0, 0, 255))
    for j in range(ROWS):
        for i in range(COLS):
            img.paste(tile_image(m, x0 + i, y0 + j), (i * 16, j * 16))
    #  People that are always there (no flag): nurse, clerks, ...
    for o in m.objects:
        if str(o.get("flag", "0")) != "0" or o.get("type", "object") != "object":
            continue
        ox, oy = int(o["x"]), int(o["y"])
        if not (x0 - 1 <= ox <= x0 + COLS and y0 - 1 <= oy <= y0 + ROWS):
            continue
        mv = o.get("movement_type", "")
        f = "down"
        for k in ("UP", "LEFT", "RIGHT"):
            if mv.endswith("FACE_" + k):
                f = k.lower()
        s = sprite(o.get("graphics_id"), f)
        if s is None:
            continue
        px = (ox - x0) * 16 + 8 - s.width // 2
        py = (oy - y0) * 16 + 16 - s.height
        img.paste(s, (px, py), s)
    #  The tile to stand on and the player on it.
    tx, ty = (x - x0) * 16, (y - y0) * 16
    over = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(over).rectangle([tx, ty, tx + 15, ty + 15], fill=(255, 0, 0, 90))
    img = Image.alpha_composite(img, over)
    s = sprite("OBJ_EVENT_GFX_RED_NORMAL", facing or "down")
    if s is not None:
        img.paste(s, (tx, ty + 16 - s.height), s)
    dr = ImageDraw.Draw(img)
    dr.rectangle([tx - 1, ty - 1, tx + 16, ty + 16], outline=(255, 0, 0, 255), width=2)
    img = img.resize((img.width * 2, img.height * 2), Image.NEAREST)
    bar = Image.new("RGBA", (img.width, 20), (20, 20, 20, 255))
    ImageDraw.Draw(bar).text((6, 4), title, fill=(255, 255, 255, 255))
    full = Image.new("RGBA", (img.width, img.height + 20))
    full.paste(bar, (0, 0))
    full.paste(img, (0, 20))
    full.convert("RGB").save(path, optimize=True)


# ---- Resume points -------------------------------------------------------
def first_walk_start(steps, k):
    for st in steps[k:]:
        if st["type"] == "WALK":
            return legs[st["leg"]]["start"]
        if st["type"] == "GRIND":
            return None
    return None


def next_label(steps, k):
    if k >= len(steps):
        return ""
    st = steps[k]
    return "%s: %s" % (st["type"], st.get("label", ""))


points = []      # (seg index, step 1-based, pos, why)
problems = []
start_saved = {0: "new"}   # Segment index -> how the program saved at its start.
for si, seg in enumerate(segs):
    steps = seg["steps"]
    points.append((si, 1, seg["start"], "start"))
    for i, st in enumerate(steps):
        t = st["type"]
        at = st.get("at")
        resume = []
        if t == "SAVE":
            resume = [(i + 1, "save")]
        elif t == "HEAL":
            if i + 1 < len(steps) and steps[i + 1]["type"] == "SAVE":
                continue
            resume = [(i + 1, "heal")]
        elif t == "GRIND":
            resume = [(i, "grind"), (i + 1, "grind_done")]
        elif t == "TEAM" and st.get("p0") == 1:
            resume = [(i + 1, "team")]
        else:
            continue
        if at is None:
            problems.append("%s step %d %s: no position" % (seg["id"], i + 1, t))
            continue
        for resume_at, why in resume:
            if resume_at >= len(steps):
                #  The next segment starts here.
                if si + 1 < len(segs):
                    start_saved.setdefault(si + 1, why)
                    nxt = segs[si + 1]["start"]
                    if nxt[:3] != at[:3]:
                        problems.append("%s end save at %s, but %s starts at %s" % (
                            seg["id"], at, segs[si + 1]["id"], nxt))
                continue
            w = first_walk_start(steps, resume_at)
            if w is not None and w[:3] != at[:3]:
                problems.append("%s step %d: at %s, first walk starts at %s" % (seg["id"], resume_at + 1, at, w))
            points.append((si, resume_at + 1, at, why))

#  One entry per (segment, step).
seen = set()
uniq = []
for i, p in enumerate(points):
    if p[3] == "start":
        how = start_saved.get(p[0])
        p = (p[0], p[1], p[2], {"new": "start_new", "save": "start_save", "heal": "start_heal"}.get(how, "start_nosave"))
    key = (p[0], p[1])
    if key in seen:
        continue
    seen.add(key)
    uniq.append(p)
uniq.sort(key=lambda p: (p[0], p[1]))

WHY_EN = {
    "start_new": "Start of the story (the new game saves here)",
    "start_save": "Start of the segment (the segment before saves here)",
    "start_heal": "Start of the segment (the segment before heals and saves here)",
    "start_nosave": "Start of the segment. *No save here*: the segment before goes on into this one",
    "save": "After a save",
    "heal": "After healing (with \"Save after healing\" on)",
    "grind": "Training: the party fainted or healed here (this step again)",
    "grind_done": "Training done: at the nurse",
    "team": "After putting the party back in order",
}

images = {}
for si, step, pos, why in uniq:
    mid, x, y, f = pos[0], pos[1], pos[2], pos[3] if len(pos) > 3 else None
    key = (mid, x, y, f)
    if key not in images:
        name = "%s_%d_%d_%s.png" % (d.maps[mid].name, x, y, f or "none")
        picture(mid, x, y, f, os.path.join(out_dir, "images", name),
                "%s (%d, %d), facing %s" % (readable(d.maps[mid].name), x, y, FACING_EN[f]))
        images[key] = name

L = []
L.append("# Start positions (%s)" % variant.capitalize())
L.append("")
L.append("Where the player must stand (and face) for every **Start at segment** and **Start at step** "
         "the program can begin from: the start of each segment and every step it can go on from "
         "after a save (the log says it: `Saved. To go on from this save later: start at segment X, step N.`).")
L.append("")
L.append("- Save the game **standing on the red tile, facing the way the player faces in the picture**, then "
         "set *Start at segment* and *Start at step* to the values in the table.")
L.append("- The coordinates are the map's tile coordinates of the game data (x to the right, y down, "
         "from the top left corner of the map); the picture is the easier way to find the tile.")
L.append("- Steps after healing are only save points with **Save after healing** on (the default).")
L.append("- Segment starts marked *No save here* are not places the program stops at: the game cannot always be "
         "saved there by hand (a scene goes on), so start from the last save before it instead.")
L.append("- The pictures show the map and the people that are always there; people that come and go "
         "with the story are not drawn.")
L.append("")
L.append("## Segments")
L.append("")
for si, seg in enumerate(segs):
    L.append("- [%s](#%s) — %s" % (seg["id"], seg["id"].lower(), seg.get("title", "")))
L.append("")
cur = None
for si, step, pos, why in uniq:
    seg = segs[si]
    if cur != si:
        cur = si
        L.append("## %s" % seg["id"])
        L.append("")
        if seg.get("title"):
            L.append("*%s*" % seg["title"])
            L.append("")
        L.append("| Start at step | Place | Tile (x, y) | Facing | Why | Next step |")
        L.append("|---|---|---|---|---|---|")
        rows = [p for p in uniq if p[0] == si]
        for _, st_, ps, wy in rows:
            f = ps[3] if len(ps) > 3 else None
            L.append("| **%d** | %s | (%d, %d) | %s | %s | %s |" % (
                st_, readable(d.maps[ps[0]].name), ps[1], ps[2], FACING_EN[f], WHY_EN[wy],
                next_label(seg["steps"], st_ - 1).replace("|", "/")))
        L.append("")
        shown = set()
        for _, st_, ps, wy in rows:
            f = ps[3] if len(ps) > 3 else None
            k = (ps[0], ps[1], ps[2], f)
            steps_here = [str(p[1]) for p in rows if (p[2][0], p[2][1], p[2][2], p[2][3] if len(p[2]) > 3 else None) == k]
            if k in shown:
                continue
            shown.add(k)
            L.append("**Step %s:** %s (%d, %d), facing %s" % (
                ", ".join(steps_here), readable(d.maps[ps[0]].name), ps[1], ps[2], FACING_EN[f]))
            L.append("")
            L.append("![%s step %s](%s%s)" % (seg["id"], ", ".join(steps_here), images_url, images[k]))
            L.append("")
open(os.path.join(out_dir, "START-POSITIONS.md"), "w").write("\n".join(L) + "\n")
print("%d start points, %d pictures" % (len(uniq), len(images)))
for p in problems:
    print("CHECK:", p)
