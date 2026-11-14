#!/usr/bin/env python3
"""Generate high-resolution 3DS GPU assets (.t3x atlas + .bcfnt Barlow fonts)."""
import math
import subprocess
import tempfile
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
APP = ROOT / "3ds_app"
DATA = APP / "data"
INC = APP / "include"
ASSETS = ROOT / "assets"

DEVKITPRO = Path("/Users/brendan/DSCarplay/tools/devkitpro")
TEX3DS = str(DEVKITPRO / "tools/bin/tex3ds")
MKBCFNT = str(DEVKITPRO / "tools/bin/mkbcfnt")


def make_deck(w: int, h: int) -> Image.Image:
    im = Image.new("RGBA", (w, h))
    cx, cy = w * 0.5, h * 0.42
    max_d = math.hypot(cx, h - cy)
    px = []
    for y in range(h):
        for x in range(w):
            dx = (x - cx) / (w * 0.65)
            dy = (y - cy) / (h * 0.75)
            radial = max(0.0, 1.0 - math.sqrt(dx * dx + dy * dy))
            grain = (((x * 3 + y * 11) ^ (x >> 2)) & 3) - 1
            r = int(6 + 9 * radial + grain * 0.6)
            g = int(13 + 15 * radial + grain * 0.8)
            b = int(24 + 24 * radial + grain * 1.1)
            px.append((max(0, min(255, r)), max(0, min(255, g)), max(0, min(255, b)), 255))
    im.putdata(px)
    return im


def make_dial(size: int) -> Image.Image:
    s = 4
    S = size * s
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    c = S * 0.5
    outer_r = S * 0.49
    bevel_out = S * 0.475
    bevel_in = S * 0.448
    well_r = S * 0.435

    px = []
    for py in range(S):
        dy = py - c
        for px_x in range(S):
            dx = px_x - c
            r = math.hypot(dx, dy)
            if r > outer_r:
                px.append((0, 0, 0, 0))
                continue
            ang = math.atan2(dy, dx)
            # Directional brushed metal lighting from top-left (-135 deg)
            light = 0.5 + 0.5 * math.cos(ang + 2.35619)
            if r > bevel_out:
                # Outer soft shadow rim
                t = (r - bevel_out) / (outer_r - bevel_out)
                a = int(220 * (1.0 - t))
                px.append((8, 12, 18, a))
            elif r > bevel_in:
                # Machined metallic bezel
                t = (r - bevel_in) / (bevel_out - bevel_in)
                crest = math.sin(t * math.pi)
                base = 38 + 86 * light * (0.45 + 0.55 * crest)
                rr = int(base * 0.88)
                gg = int(base * 0.98)
                bb = int(min(255, base * 1.12))
                px.append((rr, gg, bb, 255))
            elif r > well_r:
                # Inner dark chamfer ring
                px.append((18, 26, 38, 255))
            else:
                # Recessed carbon/matte dial face with subtle concentric grooves
                rn = r / well_r
                groove = 1.5 if int(r) % (4 * s) < (2 * s) else 0.0
                # Specular top-left glass reflection tint
                glass = max(0.0, 1.0 - math.hypot(dx / (S * 0.35) + 0.25, dy / (S * 0.35) + 0.35))
                rr = int(10 + 7 * (1.0 - rn) + 12 * glass * glass + groove)
                gg = int(17 + 12 * (1.0 - rn) + 20 * glass * glass + groove)
                bb = int(28 + 18 * (1.0 - rn) + 28 * glass * glass + groove)
                px.append((min(255, rr), min(255, gg), min(255, bb), 255))
    im.putdata(px)

    # Crisp subtle top highlight arc on metal bezel
    dr = ImageDraw.Draw(im)
    pad = int(S * 0.032)
    dr.arc((pad, pad, S - pad, S - pad), 205, 310, fill=(215, 235, 250, 180), width=s)
    return im.resize((size, size), Image.Resampling.LANCZOS)


def make_gear(size: int) -> Image.Image:
    s = 4
    S = size * s
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    dr = ImageDraw.Draw(im)
    c = S / 2.0
    teeth = 8
    for i in range(teeth):
        ang = i * (2 * math.pi / teeth)
        r0 = S * 0.30
        r1 = S * 0.45
        w = 0.20
        pts = [
            (c + math.cos(ang - w) * r0, c + math.sin(ang - w) * r0),
            (c + math.cos(ang - w * 0.72) * r1, c + math.sin(ang - w * 0.72) * r1),
            (c + math.cos(ang + w * 0.72) * r1, c + math.sin(ang + w * 0.72) * r1),
            (c + math.cos(ang + w) * r0, c + math.sin(ang + w) * r0),
        ]
        dr.polygon(pts, fill=(255, 255, 255, 255))
    r_outer = S * 0.34
    r_inner = S * 0.14
    dr.ellipse((c - r_outer, c - r_outer, c + r_outer, c + r_outer), fill=(255, 255, 255, 255))
    dr.ellipse((c - r_inner, c - r_inner, c + r_inner, c + r_inner), fill=(0, 0, 0, 0))
    return im.resize((size, size), Image.Resampling.LANCZOS)


def main() -> None:
    DATA.mkdir(parents=True, exist_ok=True)
    INC.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        make_deck(400, 240).save(tmp / "deck_top.png")
        make_dial(216).save(tmp / "dial_large.png")
        make_dial(164).save(tmp / "dial_small.png")
        make_gear(24).save(tmp / "gear.png")

        t3s = tmp / "sprites_3ds.t3s"
        t3s.write_text(
            "--atlas -f rgba8888 -z auto\n"
            "deck_top.png\n"
            "dial_large.png\n"
            "dial_small.png\n"
            "gear.png\n"
        )
        subprocess.run(
            [TEX3DS, "-i", str(t3s), "-H", str(INC / "sprites_3ds.h"), "-o", str(DATA / "sprites_3ds.t3x")],
            check=True,
        )

        wl_ui = tmp / "wl_ui.txt"
        wl_ui.write_text(" ".join(f"0x{c:04X}" for c in range(32, 127)) + " 0x00B0 0x2022\n")
        subprocess.run(
            [MKBCFNT, "-s", "16", "-w", str(wl_ui), "-o", str(DATA / "barlow_ui.bcfnt"),
             str(ASSETS / "Barlow-SemiBold.ttf")],
            check=True,
        )

        num_chars = "0123456789-.+%: km/hRPMVCF°"
        wl_num = tmp / "wl_num.txt"
        wl_num.write_text(" ".join(f"0x{ord(c):04X}" for c in sorted(set(num_chars))) + "\n")
        subprocess.run(
            [MKBCFNT, "-s", "56", "-w", str(wl_num), "-o", str(DATA / "barlow_num.bcfnt"),
             str(ASSETS / "BarlowCondensed-SemiBold.ttf")],
            check=True,
        )

    for p in sorted(DATA.iterdir()):
        print(f"  {p.name}: {p.stat().st_size:,} bytes")


if __name__ == "__main__":
    main()
