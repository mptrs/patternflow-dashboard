# Mockup renderer for the dashboard weather icons (python3, Pillow). Draws every
# icon from signed distance functions; the firmware will use the same shapes.

# Procedural weather icons: every shape is a signed distance function, rendered with
# 4x4 supersampling so edges stay smooth at 64, 32 and 16 pixels. Same math ports to C++.
import math
from PIL import Image, ImageDraw, ImageFont

def clamp(v, a=0.0, b=1.0): return a if v < a else b if v > b else v
def mix(c1, c2, t): return tuple(c1[i] + (c2[i] - c1[i]) * t for i in range(3))

# ---- shapes (coordinates in icon units: 0..1 across the icon)
def sd_circle(x, y, cx, cy, r): return math.hypot(x - cx, y - cy) - r
def sd_cloud(x, y, ox, oy, s):
    # three bumps on a flat base
    d = min(sd_circle(x, y, ox + 0.30 * s, oy + 0.58 * s, 0.20 * s),
            sd_circle(x, y, ox + 0.52 * s, oy + 0.44 * s, 0.26 * s),
            sd_circle(x, y, ox + 0.74 * s, oy + 0.60 * s, 0.18 * s))
    box = max(abs(x - (ox + 0.52 * s)) - 0.32 * s, abs(y - (oy + 0.68 * s)) - 0.10 * s)
    return min(d, box)
def sd_segment(x, y, ax, ay, bx, by, r):
    px, py, vx, vy = x - ax, y - ay, bx - ax, by - ay
    h = clamp((px * vx + py * vy) / (vx * vx + vy * vy))
    return math.hypot(px - vx * h, py - vy * h) - r
def sd_poly(x, y, pts):
    d = min(sd_segment(x, y, *pts[i], *pts[(i + 1) % len(pts)], 0) for i in range(len(pts)))
    inside = False
    for i in range(len(pts)):
        (x1, y1), (x2, y2) = pts[i], pts[(i + 1) % len(pts)]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1: inside = not inside
    return -d if inside else d

SUN_IN, SUN_OUT = (255, 236, 120), (255, 150, 20)
CLOUD_TOP, CLOUD_BOT = (250, 252, 255), (175, 188, 210)
DARK_TOP, DARK_BOT = (175, 180, 200), (95, 100, 125)
RAIN, SNOW, BOLT, FOG = (80, 170, 255), (235, 245, 255), (255, 220, 40), (170, 175, 185)
MOON = (255, 238, 190)

# A layer: (sdf(x,y) -> distance, color(x,y) -> rgb). Later layers paint over earlier ones.
def sun_layers(cx=0.5, cy=0.5, r=0.22, rays=True):
    L = []
    if rays:
        def rays_sd(x, y):
            a = math.atan2(y - cy, x - cx); k = round(a / (math.pi / 4)) * (math.pi / 4)
            ax, ay = cx + math.cos(k) * (r + 0.08), cy + math.sin(k) * (r + 0.08)
            bx, by = cx + math.cos(k) * (r + 0.19), cy + math.sin(k) * (r + 0.19)
            return sd_segment(x, y, ax, ay, bx, by, 0.035)
        L.append((rays_sd, lambda x, y: SUN_OUT))
    L.append((lambda x, y: sd_circle(x, y, cx, cy, r),
              lambda x, y: mix(SUN_IN, SUN_OUT, clamp(math.hypot(x - cx + r * 0.3, y - cy + r * 0.3) / (r * 1.6)))))
    return L
def moon_layers(cx=0.5, cy=0.48, r=0.26, stars=True):
    L = [(lambda x, y: max(sd_circle(x, y, cx, cy, r), -sd_circle(x, y, cx + r * 0.55, cy - r * 0.35, r * 0.85)),
          lambda x, y: MOON)]
    if stars:
        for sx, sy, sr in ((0.80, 0.22, 0.035), (0.72, 0.50, 0.025), (0.88, 0.40, 0.02)):
            L.append((lambda x, y, sx=sx, sy=sy, sr=sr: sd_circle(x, y, sx, sy, sr), lambda x, y: (255, 255, 230)))
    return L
def cloud_layers(ox=0.08, oy=0.12, s=0.84, dark=False):
    top, bot = (DARK_TOP, DARK_BOT) if dark else (CLOUD_TOP, CLOUD_BOT)
    return [(lambda x, y: sd_cloud(x, y, ox, oy, s), lambda x, y: mix(top, bot, clamp((y - oy - 0.25 * s) / (0.55 * s))))]
def drops(n, heavy=False, snow=False, mixed=False):
    L = []
    xs = [0.28, 0.50, 0.72, 0.39, 0.61][:n]
    for i, x0 in enumerate(xs):
        y0 = 0.74 + (0.08 if i >= 3 else 0)
        is_snow = snow or (mixed and i % 2)
        if is_snow:
            L.append((lambda x, y, a=x0, b=y0 + 0.04: sd_circle(x, y, a, b, 0.04), lambda x, y: SNOW))
        else:
            L.append((lambda x, y, a=x0, b=y0: sd_segment(x, y, a + 0.03, b - 0.02, a - 0.02, b + (0.13 if heavy else 0.08), 0.022),
                      lambda x, y: RAIN))
    return L
def bolt():
    pts = [(0.52, 0.56), (0.40, 0.76), (0.50, 0.76), (0.43, 0.95), (0.64, 0.70), (0.53, 0.70), (0.60, 0.56)]
    return [(lambda x, y: sd_poly(x, y, pts), lambda x, y: BOLT)]
def fog():
    L = []
    for y0, a, b in ((0.42, 0.15, 0.85), (0.56, 0.08, 0.70), (0.70, 0.25, 0.92), (0.84, 0.12, 0.78)):
        L.append((lambda x, y, y0=y0, a=a, b=b: sd_segment(x, y, a, y0, b, y0, 0.035), lambda x, y: FOG))
    return L

def behind(sun_or_moon, cloud):  # sun/moon peeking out top-left of a cloud
    return sun_or_moon + cloud

ICONS = {
    "clear":        sun_layers(),
    "mainly clear": sun_layers(0.44, 0.44, 0.22) + cloud_layers(0.40, 0.48, 0.52),
    "partly":       behind(sun_layers(0.38, 0.38, 0.17), cloud_layers(0.10, 0.24, 0.86)),
    "overcast":     cloud_layers(0.24, 0.00, 0.74, dark=True) + cloud_layers(0.02, 0.20, 0.82),
    "fog":          cloud_layers(0.14, -0.08, 0.70) + fog(),
    "drizzle":      cloud_layers(0.08, -0.04, 0.84) + drops(3),
    "rain":         cloud_layers(0.08, -0.04, 0.84, dark=True) + drops(5, heavy=True),
    "showers":      behind(sun_layers(0.32, 0.30, 0.13), cloud_layers(0.12, 0.06, 0.80)) + drops(3, heavy=True),
    "snow":         cloud_layers(0.08, -0.04, 0.84) + drops(5, snow=True),
    "sleet":        cloud_layers(0.08, -0.04, 0.84) + drops(5, mixed=True),
    "thunder":      cloud_layers(0.08, -0.04, 0.84, dark=True) + bolt(),
    "clear night":  moon_layers(),
    "partly night": behind(moon_layers(0.36, 0.34, 0.20, stars=False), cloud_layers(0.10, 0.24, 0.86)),
}

def render(name, size, ss=4):
    im = Image.new("RGB", (size, size))
    layers = ICONS[name]
    for py in range(size):
        for px in range(size):
            acc = [0.0, 0.0, 0.0]
            for sy in range(ss):
                for sx in range(ss):
                    x, y = (px + (sx + 0.5) / ss) / size, (py + (sy + 0.5) / ss) / size
                    col = (0, 0, 0)
                    for sd, colf in layers:
                        if sd(x, y) <= 0: col = colf(x, y)
                    for i in range(3): acc[i] += col[i]
            im.putpixel((px, py), tuple(int(v / (ss * ss)) for v in acc))
    return im

def led(im, S):
    out = Image.new("RGB", (im.width * S, im.height * S), (10, 10, 10)); d = ImageDraw.Draw(out)
    for y in range(im.height):
        for x in range(im.width):
            p = im.getpixel((x, y)); d.ellipse((x * S + 1, y * S + 1, x * S + S - 2, y * S + S - 2), fill=p if any(p) else (24, 24, 24))
    return out

if __name__ == "__main__":
    F6 = ImageFont.load("MatrixLight6.pil")
    names = list(ICONS)
    cols = 5; cell_w = 64 * 5 + 30; cell_h = 64 * 5 + 32 * 5 + 16 * 5 + 70
    sheet = Image.new("RGB", (cols * cell_w, math.ceil(len(names) / cols) * cell_h), (0, 0, 0))
    d = ImageDraw.Draw(sheet)
    for i, n in enumerate(names):
        ox, oy = (i % cols) * cell_w, (i // cols) * cell_h
        d.text((ox + 4, oy + 4), n.upper(), fill=(200, 200, 200), font=ImageFont.load_default())
        sheet.paste(led(render(n, 64), 5), (ox, oy + 22))
        sheet.paste(led(render(n, 32), 5), (ox, oy + 22 + 64 * 5 + 10))
        sheet.paste(led(render(n, 16), 5), (ox + 32 * 5 + 20, oy + 22 + 64 * 5 + 10))
    sheet.save("icons_sheet.png")
    print("ok", len(names), "iconen")
