#!/usr/bin/env python3
"""Generate PSP GU swizzled texture atlases, Barlow fonts, and XMB art (ICON0/PIC1).

Produces checked-in C arrays in psp_app/include/psp_assets.h and
psp_app/source/psp_assets.c so ordinary builds do not require Python/Pillow.
"""
from __future__ import annotations

import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
PSP = ROOT / "psp_app"
INC = PSP / "include"
SRC = PSP / "source"
ASSETS_OUT = PSP / "assets"
FONTS_DIR = ROOT / "assets"

UI_FONT_PATH = str(FONTS_DIR / "Barlow-SemiBold.ttf")
NUM_FONT_PATH = str(FONTS_DIR / "BarlowCondensed-SemiBold.ttf")


def swizzle_psp(data: bytes, row_bytes: int, height: int) -> bytes:
    """Swizzle linear texture bytes into PSP GE 16-byte x 8-row block order."""
    assert row_bytes % 16 == 0, f"row_bytes={row_bytes} must be multiple of 16"
    assert height % 8 == 0, f"height={height} must be multiple of 8"
    out = bytearray(len(data))
    dst = 0
    for by in range(0, height, 8):
        for bx in range(0, row_bytes, 16):
            for r in range(8):
                off = (by + r) * row_bytes + bx
                out[dst:dst + 16] = data[off:off + 16]
                dst += 16
    return bytes(out)


def rgba_to_abgr_u32(im: Image.Image) -> list[int]:
    im = im.convert("RGBA")
    return [(a << 24) | (b << 16) | (g << 8) | r for r, g, b, a in im.getdata()]


def make_deck(w: int = 256, h: int = 256) -> Image.Image:
    im = Image.new("RGBA", (w, h))
    cx, cy = w * 0.5, h * 0.40
    px = []
    for y in range(h):
        for x in range(w):
            dx = (x - cx) / (w * 0.62)
            dy = (y - cy) / (h * 0.72)
            radial = max(0.0, 1.0 - math.sqrt(dx * dx + dy * dy))
            grain = (((x * 5 + y * 13) ^ (x >> 2)) & 3) - 1
            scan = -1.2 if (y & 1) else 0.6
            r = int(6 + 11 * radial + grain * 0.55 + scan * 0.4)
            g = int(13 + 17 * radial + grain * 0.75 + scan * 0.5)
            b = int(24 + 27 * radial + grain * 1.0 + scan * 0.7)
            px.append((max(0, min(255, r)), max(0, min(255, g)), max(0, min(255, b)), 255))
    im.putdata(px)
    return im


def make_dial(size: int = 256) -> Image.Image:
    s = 4
    S = size * s
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    c = S * 0.5
    outer_r = S * 0.492
    bevel_out = S * 0.476
    bevel_in = S * 0.448
    well_r = S * 0.434

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
            light = 0.5 + 0.5 * math.cos(ang + 2.35619)
            if r > bevel_out:
                t = (r - bevel_out) / (outer_r - bevel_out)
                a = int(225 * (1.0 - t))
                px.append((6, 10, 16, a))
            elif r > bevel_in:
                t = (r - bevel_in) / (bevel_out - bevel_in)
                crest = math.sin(t * math.pi)
                base = 42 + 92 * light * (0.42 + 0.58 * crest)
                rr = int(base * 0.88)
                gg = int(base * 0.98)
                bb = int(min(255, base * 1.14))
                px.append((rr, gg, bb, 255))
            elif r > well_r:
                px.append((16, 24, 36, 255))
            else:
                rn = r / well_r
                groove = 1.6 if int(r) % (4 * s) < (2 * s) else 0.0
                glass = max(0.0, 1.0 - math.hypot(dx / (S * 0.35) + 0.25, dy / (S * 0.35) + 0.35))
                rr = int(9 + 8 * (1.0 - rn) + 14 * glass * glass + groove)
                gg = int(16 + 13 * (1.0 - rn) + 22 * glass * glass + groove)
                bb = int(28 + 20 * (1.0 - rn) + 30 * glass * glass + groove)
                px.append((min(255, rr), min(255, gg), min(255, bb), 255))
    im.putdata(px)

    dr = ImageDraw.Draw(im)
    pad = int(S * 0.032)
    dr.arc((pad, pad, S - pad, S - pad), 205, 315, fill=(220, 238, 252, 185), width=s)
    return im.resize((size, size), Image.Resampling.LANCZOS)


def make_gear(size: int = 32) -> Image.Image:
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


def make_wifi(size: int = 32) -> Image.Image:
    s = 4
    S = size * s
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    dr = ImageDraw.Draw(im)
    cx, cy = S * 0.5, S * 0.76
    for rad in (S * 0.56, S * 0.38, S * 0.21):
        dr.arc((cx - rad, cy - rad, cx + rad, cy + rad), 225, 315, fill=(255, 255, 255, 255), width=int(2.2 * s))
    r_dot = S * 0.08
    dr.ellipse((cx - r_dot, cy - r_dot, cx + r_dot, cy + r_dot), fill=(255, 255, 255, 255))
    return im.resize((size, size), Image.Resampling.LANCZOS)


def make_mil(size: int = 32) -> Image.Image:
    """Check-engine MIL silhouette icon at 4x supersampling."""
    s = 4
    S = size * s
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    dr = ImageDraw.Draw(im)
    w = int(2.2 * s)
    # Engine block outline
    dr.line([(20, 48), (44, 48), (50, 38), (82, 38), (88, 48), (104, 48), (104, 62), (112, 62),
             (112, 86), (102, 86), (102, 96), (48, 96), (38, 86), (26, 86), (26, 62), (20, 62), (20, 48)],
            fill=(255, 255, 255, 255), width=w, joint="curve")
    # Top cap + stem
    dr.line([(56, 26), (78, 26)], fill=(255, 255, 255, 255), width=w)
    dr.line([(67, 26), (67, 38)], fill=(255, 255, 255, 255), width=w)
    # Left intake/fan bar
    dr.line([(12, 54), (12, 84)], fill=(255, 255, 255, 255), width=w)
    dr.line([(12, 69), (20, 69)], fill=(255, 255, 255, 255), width=w)
    return im.resize((size, size), Image.Resampling.LANCZOS)


def build_font_atlas(font_path: str, px_size: int, chars: list[str],
                     atlas_w: int = 512, atlas_h: int = 256, cap_top_trim: int = 0):
    font = ImageFont.truetype(font_path, px_size)
    ascent, descent = font.getmetrics()
    line_h = ascent + descent

    atlas = Image.new("L", (atlas_w, atlas_h), 0)
    dr = ImageDraw.Draw(atlas)

    pad = 2
    cx = pad
    cy = pad
    row_h = 0
    glyphs = []

    for ch in chars:
        adv = float(font.getlength(ch))
        bbox = dr.textbbox((0, 0), ch, font=font)
        gw = max(1, bbox[2] - bbox[0])
        gh = max(1, bbox[3] - bbox[1])
        if ch == " ":
            gw, gh = 1, 1
            xoff, yoff = 0, 0
        else:
            xoff = int(bbox[0])
            yoff = int(bbox[1]) - cap_top_trim

        if cx + gw + pad > atlas_w:
            cx = pad
            cy += row_h + pad * 2
            row_h = 0
        assert cy + gh + pad <= atlas_h, f"Atlas overflow at '{ch}' ({cx},{cy})"

        if ch != " ":
            dr.text((cx - xoff, cy - int(bbox[1])), ch, font=font, fill=255)

        glyphs.append({
            "ch": ch,
            "code": ord(ch),
            "u0": cx,
            "v0": cy,
            "u1": cx + gw,
            "v1": cy + gh,
            "xoff": xoff,
            "yoff": yoff,
            "w": gw,
            "h": gh,
            "xadvance": adv,
        })
        cx += gw + pad * 2
        if gh > row_h:
            row_h = gh

    return atlas, glyphs, line_h


def make_xmb_icon() -> Image.Image:
    """144x80 PSP XMB ICON0.PNG."""
    im = make_deck(144, 80)
    dr = ImageDraw.Draw(im)
    # Subtle cyan/border frame
    dr.rectangle((1, 1, 142, 78), outline=(42, 92, 152, 255), width=2)
    dr.rectangle((3, 3, 140, 5), fill=(56, 189, 248, 220))

    # Mini gauge dial on left
    dial = make_dial(58)
    im.paste(dial, (6, 11), dial)
    cx, cy = 35, 40
    # Cyan + red arc segments
    dr.arc((cx - 19, cy - 19, cx + 19, cy + 19), 135, 345, fill=(56, 189, 248, 255), width=4)
    dr.arc((cx - 19, cy - 19, cx + 19, cy + 19), 345, 405, fill=(239, 68, 68, 255), width=4)
    # Needle pointing ~315 deg
    ang = math.radians(315)
    dr.line([(cx - math.cos(ang) * 4, cy - math.sin(ang) * 4),
             (cx + math.cos(ang) * 18, cy + math.sin(ang) * 18)],
            fill=(249, 115, 22, 255), width=3)
    dr.ellipse((cx - 4, cy - 4, cx + 4, cy + 4), fill=(248, 250, 252, 255))

    f_big = ImageFont.truetype(NUM_FONT_PATH, 22)
    f_sub = ImageFont.truetype(UI_FONT_PATH, 10)
    dr.text((70, 16), "PSPOBD", font=f_big, fill=(248, 250, 252, 255))
    dr.text((70, 44), "PSP GAUGES", font=f_sub, fill=(56, 189, 248, 255))
    dr.text((70, 57), "OBD-II SUITE", font=f_sub, fill=(148, 163, 184, 255))
    return im


def make_xmb_pic1() -> Image.Image:
    """480x272 PSP XMB PIC1.PNG."""
    im = make_deck(480, 272)
    dr = ImageDraw.Draw(im)

    # Twin brushed dials cleanly side-by-side on the right (clear of XMB left column)
    dial = make_dial(138)
    positions = ((176, 70, 310, (56, 189, 248, 230)), (324, 70, 230, (249, 115, 22, 230)))
    for dx, dy, deg, col in positions:
        im.paste(dial, (dx, dy), dial)
        cx, cy = dx + 69, dy + 69
        dr.arc((cx - 50, cy - 50, cx + 50, cy + 50), 135, 345, fill=(56, 189, 248, 195), width=5)
        dr.arc((cx - 50, cy - 50, cx + 50, cy + 50), 345, 405, fill=(239, 68, 68, 215), width=5)
        ang = math.radians(deg)
        dr.line([(cx - math.cos(ang) * 11, cy - math.sin(ang) * 11),
                 (cx + math.cos(ang) * 46, cy + math.sin(ang) * 46)],
                fill=col, width=4)
        dr.ellipse((cx - 7, cy - 7, cx + 7, cy + 7), fill=(248, 250, 252, 255))

    f_title = ImageFont.truetype(NUM_FONT_PATH, 34)
    f_sub = ImageFont.truetype(UI_FONT_PATH, 12)
    dr.rectangle((0, 0, 480, 3), fill=(56, 189, 248, 220))
    dr.text((178, 18), "PSPOBD", font=f_title, fill=(248, 250, 252, 245))
    dr.text((178, 236), "WIFI OBD-II TELEMETRY  •  DTC SCANNER  •  CSV LOGGER",
            font=f_sub, fill=(148, 163, 184, 235))
    return im


def emit_u8(name: str, data: bytes) -> tuple[str, str]:
    h = f"extern const uint8_t {name}[{len(data)}];"
    lines = [f"const uint8_t {name}[{len(data)}] __attribute__((aligned(16))) = {{"]
    for i in range(0, len(data), 24):
        lines.append("    " + ",".join(str(b) for b in data[i:i + 24]) + ",")
    lines.append("};")
    return h, "\n".join(lines)


def emit_u32(name: str, data: list[int]) -> tuple[str, str]:
    h = f"extern const uint32_t {name}[{len(data)}];"
    lines = [f"const uint32_t {name}[{len(data)}] __attribute__((aligned(16))) = {{"]
    for i in range(0, len(data), 8):
        lines.append("    " + ",".join(f"0x{v:08X}u" for v in data[i:i + 8]) + ",")
    lines.append("};")
    return h, "\n".join(lines)


def main() -> None:
    INC.mkdir(parents=True, exist_ok=True)
    SRC.mkdir(parents=True, exist_ok=True)
    ASSETS_OUT.mkdir(parents=True, exist_ok=True)

    deck_im = make_deck(256, 256)
    dial_im = make_dial(256)
    gear_im = make_gear(32)
    wifi_im = make_wifi(32)
    mil_im = make_mil(32)

    ui_chars = [chr(c) for c in range(32, 127)] + ["\u00B0", "\u2022"]
    ui_atlas, ui_glyphs, ui_line_h = build_font_atlas(UI_FONT_PATH, 22, ui_chars, 512, 256, cap_top_trim=5)

    num_chars = list("0123456789-.+%:")
    num_atlas, num_glyphs, num_line_h = build_font_atlas(NUM_FONT_PATH, 88, num_chars, 512, 256, cap_top_trim=24)

    # Save XMB PNG assets
    make_xmb_icon().save(ASSETS_OUT / "ICON0.PNG")
    make_xmb_pic1().save(ASSETS_OUT / "PIC1.PNG")

    clut = [((i << 24) | 0x00FFFFFF) for i in range(256)]

    # Swizzle all textures in 16-byte x 8-row PSP GE block format
    deck_swz = swizzle_psp(deck_im.convert("RGBA").tobytes(), 256 * 4, 256)
    dial_swz = swizzle_psp(dial_im.convert("RGBA").tobytes(), 256 * 4, 256)
    gear_swz = swizzle_psp(gear_im.convert("RGBA").tobytes(), 32 * 4, 32)
    wifi_swz = swizzle_psp(wifi_im.convert("RGBA").tobytes(), 32 * 4, 32)
    mil_swz = swizzle_psp(mil_im.convert("RGBA").tobytes(), 32 * 4, 32)
    ui_swz = swizzle_psp(ui_atlas.tobytes(), 512, 256)
    num_swz = swizzle_psp(num_atlas.tobytes(), 512, 256)

    hdr = [
        "// Generated by psp_app/tools/build_art_psp.py; do not hand edit.",
        "#ifndef PSP_ASSETS_H",
        "#define PSP_ASSETS_H",
        "#include <stdint.h>",
        "",
        "typedef struct {",
        "    uint16_t code;",
        "    uint16_t u0, v0, u1, v1;",
        "    int16_t xoff, yoff;",
        "    uint16_t w, h;",
        "    float xadvance;",
        "} PspGlyph;",
        "",
        f"#define PSP_UI_GLYPH_COUNT {len(ui_glyphs)}",
        f"#define PSP_UI_LINE_H {ui_line_h}",
        f"#define PSP_NUM_GLYPH_COUNT {len(num_glyphs)}",
        f"#define PSP_NUM_LINE_H {num_line_h}",
        "",
        f"extern const PspGlyph psp_ui_glyphs[{len(ui_glyphs)}];",
        f"extern const PspGlyph psp_num_glyphs[{len(num_glyphs)}];",
    ]

    src = [
        "// Generated by psp_app/tools/build_art_psp.py; do not hand edit.",
        '#include "psp_assets.h"',
        "",
    ]

    def emit_glyphs(name: str, glyphs: list[dict]) -> None:
        src.append(f"const PspGlyph {name}[{len(glyphs)}] = {{")
        for g in glyphs:
            src.append(
                f"    {{ {g['code']}, {g['u0']}, {g['v0']}, {g['u1']}, {g['v1']}, "
                f"{g['xoff']}, {g['yoff']}, {g['w']}, {g['h']}, {g['xadvance']:.3f}f }},"
            )
        src.append("};")

    emit_glyphs("psp_ui_glyphs", ui_glyphs)
    emit_glyphs("psp_num_glyphs", num_glyphs)

    for arr_name, raw in (
        ("psp_tex_deck", deck_swz),
        ("psp_tex_dial", dial_swz),
        ("psp_tex_gear", gear_swz),
        ("psp_tex_wifi", wifi_swz),
        ("psp_tex_mil", mil_swz),
        ("psp_tex_font_ui", ui_swz),
        ("psp_tex_font_num", num_swz),
    ):
        h, s = emit_u8(arr_name, raw)
        hdr.append(h)
        src.append(s)

    h, s = emit_u32("psp_clut_alpha", clut)
    hdr.append(h)
    src.append(s)
    hdr.append("#endif // PSP_ASSETS_H")

    (INC / "psp_assets.h").write_text("\n".join(hdr) + "\n")
    (SRC / "psp_assets.c").write_text("\n".join(src) + "\n")

    total = sum(len(x) for x in (deck_swz, dial_swz, gear_swz, wifi_swz, mil_swz, ui_swz, num_swz)) + len(clut) * 4
    print(f"Generated PSP assets: {total:,} bytes textures + ICON0.PNG + PIC1.PNG")


if __name__ == "__main__":
    main()
