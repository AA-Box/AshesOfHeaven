#!/usr/bin/env python3
"""Bake the enemy creature texture sets: procedural where a model shipped nothing, unpacked
where it shipped something.

The x-com alien FBX has no texture references at all, so its chitin is generated here. The
crawler that replaced the bio-mech spider is the opposite case - it arrives with a painted trim
sheet and a packed roughness map, and what it needs is splitting into the channels this project
samples. The shared fine detail normal breaks up flatness on every body.

Outputs 1024x1024 PNGs into Saved/CreatureTextureSource, which Scripts/ImportEnemyModels.py
reads with the "baked:" prefix. Helpers come from BakeErebusTextures so there is one noise
implementation in the project, not two.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from BakeErebusTextures import fbm, normal_from_height, to_u8, warp, worley, write_png

S = 1024
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "Saved", "CreatureTextureSource")
os.makedirs(OUT, exist_ok=True)


def _albedo(height, cavity, low, high, tint):
    """Grayscale detail lifted into a narrow dark band, then tinted.

    Creature albedo has to stay dark. The per-combatant fill light delivers an order of magnitude
    more than the Erebus scene key, so anything above roughly 0.2 albedo blows out to a white
    cut-out - the same failure the human bodies were dropped to ~0.12 to fix.
    """
    value = low + (high - low) * np.clip(height * 0.75 + 0.25, 0, 1)
    value = value * (0.55 + 0.45 * cavity)
    rgb = np.stack([value * tint[0], value * tint[1], value * tint[2]], axis=-1)
    return (np.clip(rgb, 0, 1) * 255).astype(np.uint8)


def _cavity(height, seed):
    """Crevice darkening: how buried a texel is, 0 in a crack and 1 on a plate."""
    blurred = warp(fbm(6, 3, seed, gain=0.55), 6, seed + 1)
    return np.clip((height - blurred) * 2.2 + 0.72, 0, 1)


def bake_chitin():
    """Segmented exoskeleton: overlapping plates, ridged seams, fine pores.

    The alien and the hound are the same material family - a hard shell over soft seams - so the
    plates are large and the seams are where roughness and grime collect.
    """
    plates = 1.0 - np.clip(worley(7, 3101) * 1.35, 0, 1)
    ridges = fbm(13, 4, 3102, gain=0.5)
    pores = fbm(46, 3, 3103, gain=0.45)
    height = np.clip(0.58 * warp(plates, 26, 3104) + 0.30 * ridges + 0.12 * pores, 0, 1)
    cavity = _cavity(height, 3105)

    write_png(f"{OUT}/T_Creature_Chitin_D.png", _albedo(height, cavity, 0.055, 0.185, (1.00, 1.02, 0.94)))
    write_png(f"{OUT}/T_Creature_Chitin_N.png", normal_from_height(height, 2.6))
    # Plates read wet-hard, seams read dry-matte. A single roughness value is what makes an
    # organic body look injection-moulded.
    rough = 0.42 + 0.46 * (1.0 - cavity) + 0.10 * pores
    write_png(f"{OUT}/T_Creature_Chitin_R.png", to_u8(np.clip(rough, 0, 1)))
    write_png(f"{OUT}/T_Creature_Chitin_AO.png", to_u8(np.clip(cavity * 0.85 + 0.15, 0, 1)))


FACEHUGGER_SOURCE = os.environ.get(
    "AH_FACEHUGGER_SOURCE", os.path.expanduser("~/Downloads/spider-new"))
FACEHUGGER_TEXTURES = os.path.join(
    FACEHUGGER_SOURCE, "this_is_us_the_last_survivors_signing_off_gltf", "textures")


def prepare_facehugger():
    """Split the crawler's authored glTF maps into the channels this project's material wants.

    This body is the one creature in the roster whose albedo is not procedural. The map is a
    TILEABLE trim sheet - dark ribbed panelling, shared in the source scene by the crawler, the
    statues and the terrain - not an atlas baked against one model's UVs. That distinction is what
    makes it safe to reuse: a foreign atlas lands another model's anatomy on this one, a tiling
    sheet just repeats, so the UV layout underneath it does not have to match anything.

    Its roughness and metal arrive packed into one image in the glTF convention - occlusion in
    red, roughness in green, metal in blue. Feeding that packed image straight to a roughness
    sampler reads red, and the surface comes out inverted; the same trap the quadruped's export
    set two models ago. Only green carries data here: red and blue are both flat 1.0, so there is
    no occlusion to keep, and the blue would otherwise declare a fleshy body fully metallic
    because glTF's default metallic factor is 1. Metal is a scalar on the material instead.

    The normal is derived from the albedo's luminance rather than baked from a sculpt, because no
    high-poly ships with this model. That is honest surface breakup, not invented detail: it puts
    the shading where the painted detail already is, and the shared detail normal carries the fine
    grain on top.
    """
    from PIL import Image
    Image.MAX_IMAGE_PIXELS = None

    color_path = os.path.join(FACEHUGGER_TEXTURES, "4e969798_baseColor.jpeg")
    packed_path = os.path.join(FACEHUGGER_TEXTURES, "4e969798_metallicRoughness.png")
    for path in (color_path, packed_path):
        if not os.path.isfile(path):
            raise SystemExit("facehugger source missing: %s (set AH_FACEHUGGER_SOURCE)" % path)

    size = 2048
    color = Image.open(color_path).convert("RGB").resize((size, size), Image.LANCZOS)
    write_png(f"{OUT}/T_Facehugger_D.png", np.asarray(color, dtype=np.uint8))

    packed = Image.open(packed_path).convert("RGB").resize((size, size), Image.LANCZOS)
    occlusion, roughness, metallic = [np.asarray(c, dtype=np.float32) / 255.0 for c in packed.split()]
    # Written out only if it carries something. A constant map costs a sampler and a texture
    # stream to deliver a number the material could have held as a scalar.
    for name, channel in (("R", roughness), ("M", metallic), ("AO", occlusion)):
        if float(channel.std()) < 1e-4:
            print("facehugger %s channel is flat at %.3f - using a scalar instead"
                  % (name, float(channel.mean())))
            continue
        write_png(f"{OUT}/T_Facehugger_{name}.png", to_u8(np.clip(channel, 0, 1)))

    luminance = np.asarray(color.convert("L"), dtype=np.float32) / 255.0
    write_png(f"{OUT}/T_Facehugger_N.png", normal_from_height(luminance, 2.2))
    print("facehugger maps: albedo %dx%d from %s, normal derived from its luminance"
          % (size, size, os.path.basename(color_path)))
    return True


def bake_detail_normal():
    """High-frequency skin/scale normal, sampled at a tighter UV tile by every creature.

    The two models that shipped albedo maps still had no surface at grazing angles; this is what
    catches the fill light and stops a body reading as a smooth shape with a picture on it.
    """
    micro = 0.55 * fbm(120, 3, 3301, gain=0.42) + 0.45 * (1.0 - np.clip(worley(64, 3302) * 1.4, 0, 1))
    write_png(f"{OUT}/T_Creature_DetailN.png", normal_from_height(np.clip(micro, 0, 1), 1.5))


if __name__ == "__main__":
    bake_chitin()
    bake_detail_normal()
    prepare_facehugger()
    import glob
    files = sorted(glob.glob(OUT + "/*.png"))
    # 4 chitin + 1 detail normal + 3 crawler maps. The crawler's metal and occlusion channels
    # are deliberately absent: they arrive flat and live on the material as scalars.
    assert len(files) == 8, files
    for f in files:
        assert os.path.getsize(f) > 20000, (f, os.path.getsize(f))
    print("OK", len(files), "creature textures ->", OUT)
