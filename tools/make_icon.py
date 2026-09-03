#!/usr/bin/env python3
"""Generates the polyPi app icon (.icns for macOS, .ico for Windows, PNGs for
the web) from one vector-ish description, so the icon matches the app's own
geometric pi glyph and black/green palette.

Usage: python3 tools/make_icon.py [outdir]
Requires Pillow. Uses macOS `iconutil` for the .icns when available.
"""
import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFilter

# palette, matching theme.cpp / style.css
BG_TOP = (10, 20, 14)
BG_BOTTOM = (5, 11, 7)
GREEN = (61, 220, 120)
GREEN_SOFT = (61, 220, 120)

SS = 4  # supersampling factor


def rounded_mask(size, radius):
    m = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(m)
    d.rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=255)
    return m


def draw_pi(draw, size, color):
    """The same bar-plus-two-flared-legs pi the particle field builds."""
    s = size

    bar_x0, bar_x1 = 0.185 * s, 0.815 * s
    bar_y0, bar_y1 = 0.300 * s, 0.410 * s
    leg_y1 = 0.760 * s

    leg_w = 0.115 * s
    flare = 0.030 * s
    l_x0 = 0.315 * s
    r_x0 = 0.570 * s

    # top bar, slightly rounded so it doesn't read as a hard slab
    draw.rounded_rectangle([bar_x0, bar_y0, bar_x1, bar_y1],
                           radius=0.035 * s, fill=color)

    # left leg, flaring outward toward the foot
    draw.polygon([
        (l_x0, bar_y1),
        (l_x0 + leg_w, bar_y1),
        (l_x0 + leg_w - flare * 0.4, leg_y1),
        (l_x0 - flare, leg_y1),
    ], fill=color)

    # right leg, mirrored
    draw.polygon([
        (r_x0, bar_y1),
        (r_x0 + leg_w, bar_y1),
        (r_x0 + leg_w + flare, leg_y1),
        (r_x0 + flare * 0.4, leg_y1),
    ], fill=color)


def render(size):
    """Render one square icon at `size` px."""
    big = size * SS

    # vertical gradient ground
    base = Image.new("RGB", (1, big))
    gp = base.load()
    for y in range(big):
        t = y / max(1, big - 1)
        gp[0, y] = tuple(
            int(BG_TOP[i] + (BG_BOTTOM[i] - BG_TOP[i]) * t) for i in range(3)
        )
    icon = base.resize((big, big))

    # soft glow behind the glyph
    glow = Image.new("L", (big, big), 0)
    draw_pi(ImageDraw.Draw(glow), big, 90)
    glow = glow.filter(ImageFilter.GaussianBlur(big * 0.045))
    icon.paste(Image.new("RGB", (big, big), GREEN_SOFT), (0, 0), glow)

    # the glyph itself
    glyph = Image.new("L", (big, big), 0)
    draw_pi(ImageDraw.Draw(glyph), big, 255)
    icon.paste(Image.new("RGB", (big, big), GREEN), (0, 0), glyph)

    # hairline inner edge, keeps it from looking flat on dark backgrounds
    edge = ImageDraw.Draw(icon)
    edge.rounded_rectangle(
        [1, 1, big - 2, big - 2], radius=big * 0.185, outline=(28, 60, 40),
        width=max(1, int(big * 0.006)),
    )

    icon = icon.convert("RGBA")
    icon.putalpha(rounded_mask(big, int(big * 0.185)))
    return icon.resize((size, size), Image.LANCZOS)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "desktop/assets/icon"
    os.makedirs(outdir, exist_ok=True)

    # master PNGs
    sizes = [16, 32, 64, 128, 256, 512, 1024]
    rendered = {s: render(s) for s in sizes}
    for s in (256, 512, 1024):
        rendered[s].save(os.path.join(outdir, f"polypi-{s}.png"))

    # Windows .ico (Pillow writes the whole multi-size bundle)
    ico_path = os.path.join(outdir, "polypi.ico")
    rendered[256].save(
        ico_path, format="ICO",
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )
    print("wrote", ico_path)

    # macOS .icns via iconutil, which wants a specific .iconset layout
    iconset = os.path.join(outdir, "polypi.iconset")
    os.makedirs(iconset, exist_ok=True)
    spec = [
        (16, "icon_16x16.png"), (32, "icon_16x16@2x.png"),
        (32, "icon_32x32.png"), (64, "icon_32x32@2x.png"),
        (128, "icon_128x128.png"), (256, "icon_128x128@2x.png"),
        (256, "icon_256x256.png"), (512, "icon_256x256@2x.png"),
        (512, "icon_512x512.png"), (1024, "icon_512x512@2x.png"),
    ]
    for s, name in spec:
        img = rendered[s] if s in rendered else render(s)
        img.save(os.path.join(iconset, name))

    icns_path = os.path.join(outdir, "polypi.icns")
    try:
        subprocess.run(["iconutil", "-c", "icns", iconset, "-o", icns_path], check=True)
        print("wrote", icns_path)
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print("iconutil unavailable, skipped .icns:", e, file=sys.stderr)

    # favicon for the web app
    web_ico = os.path.join(outdir, "favicon.ico")
    rendered[256].save(web_ico, format="ICO",
                       sizes=[(16, 16), (32, 32), (48, 48), (64, 64)])
    print("wrote", web_ico)


if __name__ == "__main__":
    main()
