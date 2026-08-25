#!/usr/bin/env python3
"""Bake tileable PBR texture sets for the enemy creature bodies. Pure numpy, procedural.

Two of the four imported creature models ship no images at all - the x-com alien FBX has no
texture references and the bio-mech spider has one untextured material - so they shaded from a
flat tint and read as plastic no matter how the tint was set. These are the maps they were
missing, plus a shared fine detail normal that breaks up the flatness on the two models that DID
arrive with textures.

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


def bake_carapace():
    """Bio-mech shell: tighter panels, machined scratches, pitted edges."""
    panels = 1.0 - np.clip(worley(11, 3201) * 1.20, 0, 1)
    scratch = fbm(70, 2, 3202, gain=0.35)
    pits = np.clip(worley(34, 3203) * 1.6, 0, 1)
    height = np.clip(0.60 * warp(panels, 14, 3204) + 0.24 * pits + 0.16 * scratch, 0, 1)
    cavity = _cavity(height, 3205)

    write_png(f"{OUT}/T_Creature_Carapace_D.png", _albedo(height, cavity, 0.040, 0.150, (0.92, 0.97, 1.00)))
    write_png(f"{OUT}/T_Creature_Carapace_N.png", normal_from_height(height, 3.0))
    rough = 0.30 + 0.50 * (1.0 - cavity) + 0.16 * scratch
    write_png(f"{OUT}/T_Creature_Carapace_R.png", to_u8(np.clip(rough, 0, 1)))
    write_png(f"{OUT}/T_Creature_Carapace_AO.png", to_u8(np.clip(cavity * 0.80 + 0.20, 0, 1)))


def bake_detail_normal():
    """High-frequency skin/scale normal, sampled at a tighter UV tile by every creature.

    The two models that shipped albedo maps still had no surface at grazing angles; this is what
    catches the fill light and stops a body reading as a smooth shape with a picture on it.
    """
    micro = 0.55 * fbm(120, 3, 3301, gain=0.42) + 0.45 * (1.0 - np.clip(worley(64, 3302) * 1.4, 0, 1))
    write_png(f"{OUT}/T_Creature_DetailN.png", normal_from_height(np.clip(micro, 0, 1), 1.5))


if __name__ == "__main__":
    bake_chitin()
    bake_carapace()
    bake_detail_normal()
    import glob
    files = sorted(glob.glob(OUT + "/*.png"))
    assert len(files) == 9, files
    for f in files:
        assert os.path.getsize(f) > 20000, (f, os.path.getsize(f))
    print("OK", len(files), "creature textures ->", OUT)
