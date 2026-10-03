#!/usr/bin/env python3
"""Pictures of the dashboard screens, drawn by the real screen code.

    python3 tools/render_screens.py [out.png] [screen ...]   (after ./build.sh once)

Compiles feature/dashboard/preset_dashboard.h for this computer, against small
stand-ins for Arduino and the panel (tools/render/mock) and Patternflow's real
pixel fonts, feeds it today's Open-Meteo forecast for Amsterdam and writes one
picture with every screen in portrait and landscape, drawn as LEDs.
Screens: clock weather hourly forecast world.
"""
import json, os, shutil, subprocess, sys, urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / ".build/render"
FONTS = ROOT / ".build/patternflow/firmware/patternflow/src/fonts"
URL = ("https://api.open-meteo.com/v1/forecast?latitude=52.37&longitude=4.89&timezone=auto"
       "&forecast_days=5&forecast_hours=24"
       "&current=temperature_2m,apparent_temperature,weather_code,is_day,wind_speed_10m"
       "&hourly=temperature_2m,precipitation_probability,weather_code,is_day"
       "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset")


def build():
    if WORK.exists():
        shutil.rmtree(WORK)
    shutil.copytree(ROOT / "tools/render/mock", WORK / "mock")
    shutil.copytree(ROOT / "feature/dashboard", WORK / "features/dashboard")
    shutil.copytree(FONTS, WORK / "src/fonts")
    for f in (WORK / "mock/src").iterdir():
        shutil.copy(f, WORK / "src" / f.name)
    shutil.copy(ROOT / "tools/render/render.cpp", WORK)
    subprocess.run(["c++", "-std=c++17", "-O1", "-w", "-I", str(WORK / "mock"), "-I", str(WORK),
                    "-o", str(WORK / "render"), str(WORK / "render.cpp")], check=True)


def ppm(path):
    data = path.read_bytes()
    head, rest = data.split(b"\n", 1)
    _, w, h, _ = head.split()
    return int(w), int(h), rest


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else WORK / "screens.png"
    screens = sys.argv[2:]
    build()
    (WORK / "weather.json").write_bytes(urllib.request.urlopen(URL, timeout=20).read())
    (WORK / "out").mkdir()
    subprocess.run([str(WORK / "render"), str(WORK / "weather.json"), str(WORK / "out"), *screens], check=True)
    from PIL import Image, ImageDraw
    S, GAP = 5, 24  # 5 screen pixels per LED
    frames = sorted((WORK / "out").glob("*.ppm"), key=lambda p: (p.stem.endswith("landscape"), p.stem))
    pics = []
    for p in frames:
        w, h, px = ppm(p)
        lit = [(i % w, i // w) for i in range(w * h) if any(px[i * 3:i * 3 + 3])]
        if lit:  # the margins, to keep every screen clear of the edges
            xs, ys = [x for x, _ in lit], [y for _, y in lit]
            print(f"{p.stem:20s} margins: top {min(ys)}, bottom {h - 1 - max(ys)}, left {min(xs)}, right {w - 1 - max(xs)}")
        img = Image.new("RGB", (w * S, h * S), (14, 14, 14))
        d = ImageDraw.Draw(img)
        for y in range(h):
            for x in range(w):
                r, g, b = px[(y * w + x) * 3:(y * w + x) * 3 + 3]
                if r or g or b:
                    d.ellipse([x * S + 0.5, y * S + 0.5, x * S + S - 1, y * S + S - 1], fill=(r, g, b))
                else:
                    d.ellipse([x * S + 1.5, y * S + 1.5, x * S + S - 2, y * S + S - 2], fill=(28, 28, 28))
        pics.append(img)
    portrait = [p for p, f in zip(pics, frames) if "portrait" in f.stem]
    landscape = [p for p, f in zip(pics, frames) if "landscape" in f.stem]
    W = max(sum(p.width for p in portrait) + GAP * (len(portrait) - 1),
            sum(p.width for p in landscape[:3]) + GAP * 2) + 2 * GAP
    rows_l = (len(landscape) + 2) // 3
    H = GAP + (portrait[0].height + GAP if portrait else 0) + rows_l * (64 * S + GAP)
    sheet = Image.new("RGB", (W, H), (0, 0, 0))
    x = GAP
    for p in portrait:
        sheet.paste(p, (x, GAP)); x += p.width + GAP
    y0 = GAP + (portrait[0].height + GAP if portrait else 0)
    for i, p in enumerate(landscape):
        sheet.paste(p, (GAP + (i % 3) * (p.width + GAP), y0 + (i // 3) * (p.height + GAP)))
    sheet.save(out)
    print(out)


if __name__ == "__main__":
    main()
