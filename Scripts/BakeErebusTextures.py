#!/usr/bin/env python3
"""Bake tileable PBR texture sets for the Erebus kit. Pure numpy, fully procedural.

Outputs 1024x1024 PNGs: per family (concrete/metal/mud/asphalt) an albedo (sRGB
grayscale detail, tinted in-material), a tangent normal, a roughness map; plus a
shared crack/damage mask and an erosion noise for flame/smoke sprites.
"""
import numpy as np
import zlib, struct, os, sys

S = 1024
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "textures")
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(46)


def write_png(path, arr):
    """arr: HxW (gray) or HxWx3 uint8."""
    if arr.ndim == 2:
        arr = np.stack([arr] * 3, axis=-1)
    h, w, _ = arr.shape
    raw = b"".join(b"\x00" + arr[y].tobytes() for y in range(h))
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 6))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)
    print("wrote", os.path.basename(path))


def tile_noise(freq, seed):
    """Tileable value noise: random grid at freq x freq, bilinear upsample with wrap."""
    r = np.random.default_rng(seed)
    grid = r.random((freq, freq)).astype(np.float32)
    ys = np.linspace(0, freq, S, endpoint=False)
    xs = np.linspace(0, freq, S, endpoint=False)
    y0 = np.floor(ys).astype(int); x0 = np.floor(xs).astype(int)
    fy = (ys - y0)[:, None]; fx = (xs - x0)[None, :]
    fy = fy * fy * (3 - 2 * fy); fx = fx * fx * (3 - 2 * fx)  # smoothstep
    y1 = (y0 + 1) % freq; x1 = (x0 + 1) % freq
    g00 = grid[np.ix_(y0, x0)]; g01 = grid[np.ix_(y0, x1)]
    g10 = grid[np.ix_(y1, x0)]; g11 = grid[np.ix_(y1, x1)]
    return g00 * (1 - fy) * (1 - fx) + g01 * (1 - fy) * fx + g10 * fy * (1 - fx) + g11 * fy * fx


def fbm(base_freq, octaves, seed, gain=0.5, lac=2.0):
    total = np.zeros((S, S), np.float32)
    amp, freq, norm = 1.0, base_freq, 0.0
    for o in range(octaves):
        total += amp * tile_noise(int(freq), seed + o * 101)
        norm += amp
        amp *= gain; freq *= lac
    return total / norm


def worley(freq, seed):
    """Tileable Worley F1 distance (0 at feature point)."""
    r = np.random.default_rng(seed)
    pts = r.random((freq, freq, 2)).astype(np.float32)  # per-cell point
    ys = np.linspace(0, freq, S, endpoint=False)
    xs = np.linspace(0, freq, S, endpoint=False)
    cy = np.floor(ys).astype(int)[:, None] * np.ones(S, int)[None, :]
    cx = np.ones(S, int)[:, None] * np.floor(xs).astype(int)[None, :]
    fy = ys[:, None] - np.floor(ys)[:, None]
    fx = xs[None, :] - np.floor(xs)[None, :]
    best = np.full((S, S), 9.0, np.float32)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            ny = (cy + dy) % freq; nx = (cx + dx) % freq
            py = pts[ny, nx, 0] + dy; px = pts[ny, nx, 1] + dx
            d = (py - fy) ** 2 + (px - fx) ** 2
            best = np.minimum(best, d)
    return np.sqrt(best)


def crack_mask(freq, seed, width=0.04):
    """Voronoi-edge cracks: F2-F1 small near cell borders."""
    r = np.random.default_rng(seed)
    pts = r.random((freq, freq, 2)).astype(np.float32)
    ys = np.linspace(0, freq, S, endpoint=False)
    xs = np.linspace(0, freq, S, endpoint=False)
    cy = np.floor(ys).astype(int)[:, None] * np.ones(S, int)[None, :]
    cx = np.ones(S, int)[:, None] * np.floor(xs).astype(int)[None, :]
    fy = ys[:, None] - np.floor(ys)[:, None]
    fx = xs[None, :] - np.floor(xs)[None, :]
    d1 = np.full((S, S), 9.0, np.float32); d2 = np.full((S, S), 9.0, np.float32)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            ny = (cy + dy) % freq; nx = (cx + dx) % freq
            py = pts[ny, nx, 0] + dy; px = pts[ny, nx, 1] + dx
            d = np.sqrt((py - fy) ** 2 + (px - fx) ** 2)
            closer = d < d1
            d2 = np.where(closer, d1, np.minimum(d2, d))
            d1 = np.where(closer, d, d1)
    edge = d2 - d1
    m = np.clip(1.0 - edge / width, 0, 1)
    # jitter the crack line so it isn't a clean voronoi edge
    m *= (fbm(24, 3, seed + 7) > 0.35).astype(np.float32)
    return m


def warp(field, amp_px, seed):
    """Domain-warp a tileable field by fbm offsets (breaks up round/regular shapes)."""
    dy = (fbm(4, 3, seed) - 0.5) * 2 * amp_px
    dx = (fbm(4, 3, seed + 77) - 0.5) * 2 * amp_px
    yy = (np.arange(S)[:, None] + dy).astype(int) % S
    xx = (np.arange(S)[None, :] + dx).astype(int) % S
    return field[yy, xx]


def to_u8(a, lo=0.0, hi=1.0):
    return np.clip((a - lo) / (hi - lo) * 255.0, 0, 255).astype(np.uint8)


def normal_from_height(h, strength):
    gy, gx = np.gradient(h.astype(np.float32))
    nx = -gx * strength; ny = gy * strength  # +Y green (OpenGL-style; UE flips green on import option; keep UE style: green = +Y down => use -gy)
    ny = -ny
    nz = np.ones_like(h, np.float32)
    l = np.sqrt(nx * nx + ny * ny + nz * nz)
    n = np.stack([nx / l, ny / l, nz / l], axis=-1)
    return (np.clip(n * 0.5 + 0.5, 0, 1) * 255).astype(np.uint8)


# --- CONCRETE: pores + patch blotches + cracks + chips ------------------------
def bake_concrete():
    macro = fbm(4, 3, 10)                     # big tonal patches
    mid = fbm(16, 4, 20)                      # medium mottle
    pores = worley(160, 30)                   # small pores (dark points)
    pore_dark = np.clip(0.20 - pores, 0, 1) * 2.2
    cracks = warp(crack_mask(7, 40, 0.030), 40, 60)
    stains = np.clip(fbm(6, 4, 50) - 0.55, 0, 1) * 2.0  # streaky damp stains
    albedo = 0.62 + 0.25 * macro + 0.12 * mid - 0.35 * pore_dark - 0.30 * stains - 0.45 * cracks
    write_png(f"{OUT}/T_Erebus_Concrete_D.png", to_u8(albedo, 0.05, 1.0))
    height = 0.5 * macro + 0.3 * mid - 0.8 * cracks - 0.5 * pore_dark
    write_png(f"{OUT}/T_Erebus_Concrete_N.png", normal_from_height(height, 6.0))
    rough = 0.80 + 0.15 * mid - 0.10 * macro + 0.15 * cracks + 0.25 * stains
    write_png(f"{OUT}/T_Erebus_Concrete_R.png", to_u8(rough, 0.4, 1.1))


# --- METAL: brushed streaks + dents + rust speckle + chipped paint edges ------
def bake_metal():
    streak_src = fbm(6, 5, 110)
    streaks = np.zeros((S, S), np.float32)   # vertical smear = drip/brush streaks
    acc = streak_src.copy()
    for i in range(6):
        acc = 0.5 * (acc + np.roll(acc, 1 << i, axis=0))
    streaks = acc
    dents = fbm(10, 3, 120)
    rust = np.clip(fbm(28, 4, 130) - 0.62, 0, 1) * 2.6   # rust speckle patches
    chips = (worley(48, 140) < 0.06).astype(np.float32)  # paint chips
    chips *= (fbm(8, 3, 145) > 0.52).astype(np.float32)  # cluster, not uniform speckle
    albedo = 0.55 + 0.22 * streaks + 0.10 * dents - 0.38 * rust + 0.25 * chips
    write_png(f"{OUT}/T_Erebus_Metal_D.png", to_u8(albedo, 0.05, 1.0))
    height = 0.5 * dents - 0.35 * rust - 0.3 * chips + 0.06 * streaks
    write_png(f"{OUT}/T_Erebus_Metal_N.png", normal_from_height(height, 5.0))
    rough = 0.45 + 0.20 * streaks + 0.45 * rust - 0.25 * chips + 0.10 * dents
    write_png(f"{OUT}/T_Erebus_Metal_R.png", to_u8(rough, 0.1, 1.05))


# --- MUD: lumpy churned earth + wet hollows + debris speckle ------------------
def bake_mud():
    lumps = warp(fbm(9, 5, 210, gain=0.55), 90, 250)
    ruts = fbm(3, 2, 220)
    hollows = warp(np.clip(0.5 - worley(14, 230), 0, 1), 120, 260)  # wet pockets, warped organic
    debris = (worley(90, 240) < 0.045).astype(np.float32)
    albedo = 0.42 + 0.30 * lumps + 0.15 * ruts - 0.40 * hollows + 0.30 * debris
    write_png(f"{OUT}/T_Erebus_Mud_D.png", to_u8(albedo, 0.0, 1.05))
    height = 0.8 * lumps + 0.5 * ruts - 1.0 * hollows + 0.35 * debris
    write_png(f"{OUT}/T_Erebus_Mud_N.png", normal_from_height(height, 9.0))
    rough = 0.85 + 0.10 * lumps - 0.55 * hollows        # hollows read wet (low rough)
    write_png(f"{OUT}/T_Erebus_Mud_R.png", to_u8(rough, 0.1, 1.0))


# --- ASPHALT: fine aggregate + cracks + patch seams ---------------------------
def bake_asphalt():
    grain = tile_noise(256, 310) * 0.5 + tile_noise(128, 311) * 0.5
    macro = fbm(5, 3, 320)
    cracks = crack_mask(9, 330, 0.024)
    seams = (np.abs(fbm(2, 2, 340) - 0.5) < 0.015).astype(np.float32)
    albedo = 0.50 + 0.18 * grain + 0.22 * macro - 0.40 * cracks - 0.25 * seams
    write_png(f"{OUT}/T_Erebus_Asphalt_D.png", to_u8(albedo, 0.05, 1.0))
    height = 0.25 * grain + 0.3 * macro - 0.9 * cracks - 0.4 * seams
    write_png(f"{OUT}/T_Erebus_Asphalt_N.png", normal_from_height(height, 7.0))
    rough = 0.78 + 0.15 * grain + 0.20 * cracks - 0.12 * macro
    write_png(f"{OUT}/T_Erebus_Asphalt_R.png", to_u8(rough, 0.3, 1.1))


# --- shared damage/crack mask + flame erosion noise ---------------------------
def bake_masks():
    cracks = crack_mask(6, 410, 0.05)
    chips = np.clip(fbm(20, 4, 420) - 0.62, 0, 1) * 2.5
    write_png(f"{OUT}/T_Erebus_DamageMask.png", to_u8(np.clip(cracks + chips, 0, 1)))
    # erosion: billowy fbm for flame/smoke alpha erosion (soft, high contrast mid)
    ero = fbm(5, 5, 510, gain=0.6)
    ero = np.clip((ero - 0.35) * 2.2, 0, 1)
    write_png(f"{OUT}/T_Erebus_Erosion.png", to_u8(ero))


if __name__ == "__main__":
    bake_concrete(); bake_metal(); bake_mud(); bake_asphalt(); bake_masks()
    # self-check: every output exists, is tileable-sized, non-degenerate
    import glob
    files = sorted(glob.glob(OUT + "/*.png"))
    assert len(files) == 14, files
    for f in files:
        assert os.path.getsize(f) > 20000, (f, os.path.getsize(f))
    print("OK", len(files), "textures")
