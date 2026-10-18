#!/usr/bin/env python3
"""Build quakelive.pk3 for baseql from your own copy of Quake Live.

Packs the Heavy Machinegun's and Green Armor's models, sounds and icons, Quake
Live's HUDs and fonts, and Quake Live's maps with the textures, shaders, models and
sounds they use, so baseql can load them. Files that Quake III Arena or Team
Arena already provide are left out, so Quake 3's own maps keep their look.

You need your own copies of Quake III Arena, Team Arena (which baseql's menus
use) and Quake Live. The pk3 this writes contains Quake Live's copyrighted
assets: it's for your own use, don't redistribute it.

Put the result in your baseql directory. Everyone on a pure server needs the
same pk3; the same Quake Live, Quake 3 and Team Arena data gives the same files.

usage: make-ql-pak.py --quakelive QL/baseq3 --quake3 Q3/baseq3
                      --missionpack Q3/missionpack [--maps all|a,b,c]
                      [--output quakelive.pk3] [--list-maps]
"""

import argparse
import os
import re
import struct
import sys
import zipfile

# when the pk3's files are dated; fixed so that builds are reproducible
PK3_DATE = (2016, 6, 3, 0, 0, 0)

IMAGE_EXTENSIONS = (".tga", ".jpg", ".png")

# files of the Quake Live items baseql has (Heavy Machinegun, Green Armor), by
# directory or name
ITEM_PREFIXES = (
    "models/weapons3/hmg/",
    "sound/weapons/hmg/",
    "models/powerups/ammo/hmgam",
    "icons/weap_hmg.",
    "icons/ammo_hmg.",
    "models/powerups/armor/armor_grn",
    "icons/iconr_green.",
)

# Quake Live's HUDs (ui/hud.txt selects one, like cg_hudFiles). These replace
# Team Arena's ui/hud.txt. ui/menudef.h stays baseql's own, which has
# Quake Live's names with baseql's numbers.
HUD_FILES = ("ui/hud.txt", "ui/hud2.txt", "ui/hud3.txt", "ui/hud.menu", "ui/min_hud.menu", "ui/comp_hud.menu")

# art the HUD code loads by name, such as the gametype icons
HUD_ASSET_PREFIXES = ("ui/assets/hud/",)

# the fonts HUD text draws in (FONT_* in baseql's ui/menudef.h). Quake Live's
# fallback font for other scripts, droidsansfallbackfull.ttf, is left out: the
# engine draws only Latin-1.
FONT_FILES = ("fonts/handelgothic.ttf", "fonts/notosans-regular.ttf", "fonts/droidsansmono.ttf")

# Quake Live gametype names in arenas.txt, as the Team Arena menus spell them
ARENA_TYPES = {"duel": "tourney", "tdm": "team", "har": "harvester"}

# The game reads each .arena file into an 8 KB buffer
MAX_ARENA_FILE = 8000

# ioquake3's renderers refuse MD3 surfaces with more vertices or triangles than
# these (SHADER_MAX_VERTEXES and SHADER_MAX_INDEXES); Quake Live's allow more
MD3_MAX_VERTS = 999
MD3_MAX_TRIANGLES = 1999


def normalize(name):
    """Game paths use / and no leading one; the engine treats \\ the same."""
    return name.replace("\\", "/").lstrip("/")


def fail(message):
    sys.exit("make-ql-pak: " + message)


def find_pk3s(directory):
    if not os.path.isdir(directory):
        fail(directory + " isn't a directory")
    return sorted(os.path.join(directory, name) for name in os.listdir(directory)
                  if name.lower().endswith(".pk3"))


def require_pk3(directory, name, game):
    for path in find_pk3s(directory):
        if os.path.basename(path).lower() == name:
            return path
    fail("no %s in %s; this needs your own copy of %s" % (name, directory, game))


class Pak:
    """Read access to a set of pk3s, by lowercase path."""

    def __init__(self, paths):
        self.files = {}
        for path in paths:
            archive = zipfile.ZipFile(path)
            for info in archive.infolist():
                if not info.is_dir():
                    self.files.setdefault(info.filename.lower(), (archive, info))

    def __contains__(self, name):
        return name.lower() in self.files

    def read(self, name):
        archive, info = self.files[name.lower()]
        return archive.read(info)

    def original_name(self, name):
        return self.files[name.lower()][1].filename

    def names(self):
        return self.files.keys()


# --- shader scripts

def strip_comments(text):
    """Removes // and /* */ comments, keeping the line breaks, which matter
    to the engine's shader parser."""
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


TOKEN = re.compile(r'"[^"\n]*"|[{}]|[^\s{}"]+')


def parse_shaders(text):
    """Maps each shader name in a script to its name and definition, verbatim."""
    shaders = {}
    text = strip_comments(text)
    tokens = list(TOKEN.finditer(text))
    i = 0
    while i < len(tokens):
        if i + 1 < len(tokens) and tokens[i + 1].group(0) == "{":
            depth, j = 0, i + 1
            while j < len(tokens):
                depth += {"{": 1, "}": -1}.get(tokens[j].group(0), 0)
                if depth == 0:
                    break
                j += 1
            name = tokens[i].group(0).strip('"')
            end = tokens[min(j, len(tokens) - 1)].end()
            shaders.setdefault(name.lower(), (name, text[tokens[i + 1].start():end]))
            i = j + 1
        else:
            i += 1
    return shaders


def shader_images(name, body):
    """Image names a shader definition uses."""
    images = []
    for line in body.splitlines():
        tokens = [t.strip('"') for t in TOKEN.findall(line)]
        lowered = [t.lower() for t in tokens]
        for i, token in enumerate(lowered):
            args = tokens[i + 1:]
            if token in ("map", "clampmap") and args:
                images.append(args[0])
            elif token == "animmap":
                images += [a for a in args[1:] if a not in "{}"]     # after the frequency
            elif token == "skyparms":
                for box in args[0:1] + args[2:3]:
                    if box != "-":
                        images += ["%s_%s" % (box, side) for side in ("rt", "bk", "lf", "ft", "up", "dn")]
            elif token in ("implicitmap", "implicitmask", "implicitblend") and args:
                images.append(name if args[0] == "-" else args[0])
    return [image for image in images if not image.startswith("$")]


# --- maps and models

def bsp_references(data):
    """Shader, model and sound names a BSP uses."""
    if data[:4] != b"IBSP" or struct.unpack_from("<i", data, 4)[0] not in (46, 47):
        raise ValueError("not a Quake 3 or Quake Live map")
    lumps = [struct.unpack_from("<ii", data, 8 + 8 * i) for i in range(17)]

    def names(lump, size):
        offset, length = lumps[lump]
        return [data[offset + i:offset + i + 64].split(b"\0")[0].decode("latin-1")
                for i in range(0, length - length % size, size)]

    shaders = names(1, 72) + names(12, 72)          # shaders and fogs
    offset, length = lumps[0]
    entities = data[offset:offset + length].decode("latin-1")
    models, sounds = [], []
    for key, value in re.findall(r'"([^"]*)"\s+"([^"]*)"', entities):
        key = key.lower()
        if key in ("model", "model2") and value.lower().endswith(".md3"):
            models.append(value)
        elif key == "noise" and not value.startswith("*"):
            sounds.append(value)
        elif key == "music":
            sounds += value.split()
    return shaders, models, sounds


def md3_shaders(data):
    if data[:4] != b"IDP3":
        return []
    num_surfaces, = struct.unpack_from("<i", data, 84)
    offset, = struct.unpack_from("<i", data, 100)
    shaders = []
    for _ in range(num_surfaces):
        num_shaders, = struct.unpack_from("<i", data, offset + 76)
        ofs_shaders, = struct.unpack_from("<i", data, offset + 92)
        for i in range(num_shaders):
            start = offset + ofs_shaders + 68 * i
            shaders.append(data[start:start + 64].split(b"\0")[0].decode("latin-1"))
        offset += struct.unpack_from("<i", data, offset + 104)[0]
    return shaders


def split_md3(data):
    """Splits MD3 surfaces with too many vertices or triangles for ioquake3
    into several smaller ones. Returns the model unchanged if none are."""
    if data[:4] != b"IDP3":
        return data
    header = list(struct.unpack_from("<4si64si8i", data, 0))
    num_frames, num_surfaces, ofs_surfaces = header[4], header[6], header[10]

    surfaces, offset, changed = [], ofs_surfaces, False
    for _ in range(num_surfaces):
        (ident, name, flags, frames, num_shaders, num_verts, num_tris,
         ofs_tris, ofs_shaders, ofs_st, ofs_xyz, ofs_end) = struct.unpack_from("<4s64s10i", data, offset)
        base = offset
        shaders = data[base + ofs_shaders:base + ofs_shaders + 68 * num_shaders]
        tris = [struct.unpack_from("<3i", data, base + ofs_tris + 12 * i) for i in range(num_tris)]
        st = [data[base + ofs_st + 8 * v:base + ofs_st + 8 * v + 8] for v in range(num_verts)]
        xyz = [[data[base + ofs_xyz + 8 * (f * num_verts + v):base + ofs_xyz + 8 * (f * num_verts + v) + 8]
                for v in range(num_verts)] for f in range(frames)]
        offset += ofs_end

        # group triangles so each group stays within the limits
        groups, current, remap = [], [], {}
        for tri in tris:
            new = [v for v in tri if v not in remap]
            if current and (len(remap) + len(set(new)) > MD3_MAX_VERTS or len(current) >= MD3_MAX_TRIANGLES):
                groups.append((current, remap))
                current, remap = [], {}
            for v in tri:
                remap.setdefault(v, len(remap))
            current.append(tri)
        groups.append((current, remap))
        changed |= len(groups) > 1

        for group_tris, group_remap in groups:
            order = sorted(group_remap, key=group_remap.get)
            body_tris = b"".join(struct.pack("<3i", *(group_remap[v] for v in tri)) for tri in group_tris)
            body_st = b"".join(st[v] for v in order)
            body_xyz = b"".join(xyz[f][v] for f in range(frames) for v in order)
            o_shaders = 108
            o_tris = o_shaders + len(shaders)
            o_st = o_tris + len(body_tris)
            o_xyz = o_st + len(body_st)
            o_end = o_xyz + len(body_xyz)
            surfaces.append(struct.pack("<4s64s10i", ident, name, flags, frames, num_shaders, len(order),
                                        len(group_tris), o_tris, o_shaders, o_st, o_xyz, o_end)
                            + shaders + body_tris + body_st + body_xyz)

    if not changed:
        return data
    body = b"".join(surfaces)
    header[6] = len(surfaces)
    header[11] = ofs_surfaces + len(body)        # ofsEnd
    return struct.pack("<4si64si8i", *header) + data[108:ofs_surfaces] + body


# --- collecting what to pack

class Collector:
    def __init__(self, ql, base, ql_shaders, base_shaders):
        self.ql, self.base = ql, base
        self.ql_shaders, self.base_shaders = ql_shaders, base_shaders
        self.files = set()          # Quake Live files to copy
        self.shaders = {}           # Quake Live shader definitions to write
        self.missing = set()
        self.seen = set()

    def add_file(self, name):
        """Adds a Quake Live file unless Quake 3 has it. Returns whether found."""
        name = normalize(name)
        if name in self.base:
            return True
        if name in self.ql:
            self.files.add(self.ql.original_name(name))
            return True
        return False

    def add_image(self, name):
        name = normalize(name)
        stem, ext = os.path.splitext(name)
        candidates = [name] if ext.lower() in IMAGE_EXTENSIONS else []
        candidates += [stem + e for e in IMAGE_EXTENSIONS]
        if any(c in self.base for c in candidates):
            return
        for candidate in candidates:
            if self.add_file(candidate):
                return
        self.missing.add(name)

    def add_shader(self, name):
        name = normalize(name)
        # like the engine, look shaders up without the image extension
        stem, ext = os.path.splitext(name)
        key = (stem if ext.lower() in IMAGE_EXTENSIONS else name).lower()
        if not name or key in self.seen or key == "noshader":
            return
        self.seen.add(key)
        if key in self.base_shaders:
            return
        # Quake 3 maps use an image of this name as is; a Quake Live shader
        # definition would change how they look
        if any(key + e in self.base for e in IMAGE_EXTENSIONS):
            return
        if key in self.ql_shaders:
            shader_name, body = self.ql_shaders[key]
            self.shaders[key] = "%s\n%s\n" % (shader_name, body)
            for image in shader_images(shader_name, body):
                self.add_image(image)
        else:
            self.add_image(name)

    def add_model(self, name):
        name = normalize(name)
        if name.lower() in self.seen:
            return
        self.seen.add(name.lower())
        if not self.add_file(name):
            self.missing.add(name)
        elif name not in self.base:
            for shader in md3_shaders(self.ql.read(name)):
                self.add_shader(shader)

    def add_sound(self, name):
        stem = os.path.splitext(normalize(name))[0]
        if not any(self.add_file(stem + e) for e in (".wav", ".ogg")):
            self.missing.add(name)

    def add_map(self, name):
        bsp = "maps/%s.bsp" % name
        self.add_file(bsp)
        self.add_file("maps/%s.aas" % name)
        for ext in IMAGE_EXTENSIONS:
            self.add_file("levelshots/%s%s" % (name, ext))
        shaders, models, sounds = bsp_references(self.ql.read(bsp))
        for shader in shaders:
            self.add_shader(shader)
        for model in models:
            self.add_model(model)
        for sound in sounds:
            self.add_sound(sound)

    def add_hud(self):
        for name in HUD_FILES:
            if name in self.ql:
                self.files.add(self.ql.original_name(name))     # replaces Team Arena's
                if name.endswith(".menu"):
                    for image in menu_assets(self.ql.read(name).decode("latin-1")):
                        self.add_shader(image)
        for name in sorted(self.ql.names()):
            if name.startswith(HUD_ASSET_PREFIXES):
                self.add_file(name)
        for name in FONT_FILES:
            if not self.add_file(name):
                self.missing.add(name)

    def add_items(self):
        for name in sorted(self.ql.names()):
            if name.startswith(ITEM_PREFIXES):
                if name.endswith(".md3"):
                    self.add_model(name)
                else:
                    self.add_file(name)


def menu_assets(text):
    """Image paths a menu script refers to: quoted names with a directory.
    Font names ("fonts/font") aren't files; the HUD draws in FONT_FILES."""
    names = re.findall(r'"([^"\s]+/[^"\s]+)"', strip_comments(text))
    return [n for n in names if not n.lower().endswith((".menu", ".txt", ".h", ".cfg", ".wav", ".ogg"))
            and not n.lower().startswith("fonts/")]


def arena_files(arenas_text, maps):
    """The included maps' arena entries, in files small enough for the game."""
    blocks = []
    for block in re.findall(r"\{[^}]*\}", arenas_text):
        found = re.search(r'map\s+"([^"]+)"', block)
        if not found or found.group(1).lower() not in maps:
            continue
        def types(match):
            return 'type\t\t"%s"' % " ".join(ARENA_TYPES.get(t, t) for t in match.group(1).split())
        blocks.append(re.sub(r'type\s+"([^"]*)"', types, block).replace("\r", "") + "\n")
    files, current = [], ""
    for block in blocks:
        if len(current) + len(block) > MAX_ARENA_FILE:
            files.append(current)
            current = ""
        current += block
    if current:
        files.append(current)
    return files


def write_pk3(path, ql, files, generated):
    """Writes the pk3, splitting MD3 surfaces that are too big for ioquake3.
    Returns the names of the models it split."""
    split = []
    tmp = path + ".tmp"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as out:
        entries = [(name, None) for name in files] + list(generated.items())
        for name, data in sorted(entries, key=lambda e: e[0].lower()):
            if data is None:
                data = ql.read(name)
                if name.lower().endswith(".md3"):
                    fixed = split_md3(data)
                    if fixed is not data:
                        split.append(name)
                    data = fixed
            info = zipfile.ZipInfo(name, PK3_DATE)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            out.writestr(info, data)
    os.replace(tmp, path)
    return split


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--quakelive", required=True, help="Quake Live's baseq3 directory, with pak00.pk3")
    parser.add_argument("--quake3", required=True, help="Quake III Arena's baseq3 directory, with pak0.pk3")
    parser.add_argument("--missionpack", required=True, help="Team Arena's missionpack directory, with pak0.pk3")
    parser.add_argument("--maps", default="all", help='"all" (default) or a comma-separated list')
    parser.add_argument("--output", default="quakelive.pk3")
    parser.add_argument("--list-maps", action="store_true", help="list Quake Live's maps and exit")
    args = parser.parse_args()

    ql = Pak([require_pk3(args.quakelive, "pak00.pk3", "Quake Live")])
    # only the retail paks, so that everyone builds the same pk3 whatever else
    # they have installed (ioquake3 needs all nine of Quake 3's anyway)
    base_pk3s = [require_pk3(args.quake3, "pak%d.pk3" % i, "Quake III Arena with its 1.32 point release")
                 for i in range(9)]
    base_pk3s.append(require_pk3(args.missionpack, "pak0.pk3", "Team Arena"))
    base = Pak(base_pk3s)

    available = sorted(n[5:-4] for n in ql.names() if re.match(r"maps/[^/]+\.bsp$", n))
    if args.list_maps:
        print("\n".join(available))
        return
    if args.maps == "all":
        maps = available
    else:
        maps = [m.strip().lower() for m in args.maps.split(",") if m.strip()]
        unknown = [m for m in maps if m not in available]
        if unknown:
            fail("not Quake Live maps: " + ", ".join(unknown))

    def all_shaders(pak):
        shaders = {}
        for name in sorted(pak.names()):
            if name.startswith("scripts/") and name.endswith(".shader"):
                for key, value in parse_shaders(pak.read(name).decode("latin-1")).items():
                    shaders.setdefault(key, value)
        return shaders

    collector = Collector(ql, base, all_shaders(ql), all_shaders(base))
    collector.add_items()
    collector.add_hud()
    for name in maps:
        if "maps/%s.bsp" % name in base:
            continue            # Quake 3's own map of the same name wins
        collector.add_map(name)

    generated = {}
    if collector.shaders:
        generated["scripts/quakelive.shader"] = (
            "// Quake Live shaders used by quakelive.pk3's maps and models, extracted by make-ql-pak.py\n\n"
            + "\n".join(collector.shaders[k] for k in sorted(collector.shaders))).encode("latin-1")
    for i, text in enumerate(arena_files(ql.read("scripts/arenas.txt").decode("latin-1"), set(maps))):
        generated["scripts/quakelive%d.arena" % (i + 1)] = text.encode("latin-1")
    generated["quakelive-pk3.txt"] = (
        "Built by baseql's make-ql-pak.py from a Quake Live installation.\n"
        "It contains Quake Live's copyrighted assets, for the owner's own use. Don't redistribute it.\n").encode()

    split = write_pk3(args.output, ql, collector.files, generated)

    size = os.path.getsize(args.output)
    print("wrote %s: %d maps, %d files from Quake Live, %d shaders, %.0f MB"
          % (args.output, len(maps), len(collector.files), len(collector.shaders), size / 1e6))
    if split:
        print("split surfaces too big for ioquake3 in: " + ", ".join(split))
    if collector.missing:
        print("%d referenced files exist in neither game (usually harmless):" % len(collector.missing))
        for name in sorted(collector.missing)[:20]:
            print("  " + name)
        if len(collector.missing) > 20:
            print("  ...")


if __name__ == "__main__":
    main()
