import unreal

"""Author the fire and smoke sprite masters for the Erebus Niagara systems.

The previous pass gave every sprite one frozen look — a radial blob times two
panning noise samples — so a fire read as a scatter of identical orange popcorn
puffs and smoke read as nothing at all. Both masters are now driven by particle
age (ParticleRelativeTime), which is what gives a sprite a life arc:

  * age-driven erosion threshold — each puff BURNS AWAY from its edges instead of
    fading uniformly. This is what produces licking flame tongues and dissolving
    smoke rather than blobs that pop out at full strength.
  * age-driven radius — fake size-over-life (flames taper in, smoke billows out)
    without adding a single Niagara module. The sprite quad stays at its authored
    max size; the visible mass inside it grows or shrinks.
  * age-driven colour — fire runs white-hot -> orange -> dying red; smoke runs
    warm and dark at the source -> cool and pale as it disperses.
  * DepthFade — kills the razor line where a sprite intersects the ground or a
    wreck, the single loudest "these are billboards" tell.
  * ParticleRandom UV offset — every sprite samples a different part of the noise,
    so neighbouring particles never repeat the same silhouette.

Smoke is LIT (volumetric non-directional) so it takes light from the fires and the
sky instead of being a flat black cutout; fire stays unlit additive.

T_AH_VFXNoise packs three purpose-built channels: R = vertically stretched flame
noise, G = billowy smoke clumps, B = fine breakup detail.
"""

MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MAT_DIR = "/Game/Ashes/Materials"
NOISE_TEX = "/Game/Ashes/Textures/Erebus/T_AH_VFXNoise"


def make(mat, cls, x, y, **props):
    node = MEL.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        node.set_editor_property(key, value)
    return node


def wire(src, src_pin, dst, dst_pin):
    """Connect src's output pin to dst's input pin, by NAME.

    dst_pin is the destination pin's display name ("A", "B", "Alpha", "Base",
    "Coordinate", "UVs"). Single-input nodes (Saturate, OneMinus) expose one
    UNNAMED input, so they take "" - passing "Input" silently fails to connect.
    """
    if not MEL.connect_material_expressions(src, src_pin, dst, dst_pin):
        unreal.log_error("[VFXMat] failed to connect %s -> %s.%s" % (src, dst, dst_pin))


def build_common(mat, noise_tex, age, rnd, tc_scale_b, pan_a, pan_b, noise_chan,
                 radius_start, radius_end, shape_power,
                 erode_start, erode_end, erode_contrast, noise_gain, erode_ramp,
                 aspect=(1.0, 1.0)):
    """Shared half of both graphs. Returns the sprite mask: the age-shaped,
    noise-eroded silhouette in 0..1, which fire feeds to emissive and smoke feeds
    to opacity."""
    # --- shape: elliptical falloff whose radius is a function of age ----------
    # A circular mask on a square sprite can only make round puffs. Squashing the
    # distance metric in X (aspect) turns each fire sprite into a vertical lick.
    tc0 = make(mat, unreal.MaterialExpressionTextureCoordinate, -1500, -200)
    centre = make(mat, unreal.MaterialExpressionConstant2Vector, -1500, -80, r=0.5, g=0.5)
    delta = make(mat, unreal.MaterialExpressionSubtract, -1420, -160)
    wire(tc0, "", delta, "A")
    wire(centre, "", delta, "B")
    stretch = make(mat, unreal.MaterialExpressionConstant2Vector, -1420, -40,
                   r=aspect[0], g=aspect[1])
    skewed = make(mat, unreal.MaterialExpressionMultiply, -1360, -160)
    wire(delta, "", skewed, "A")
    wire(stretch, "", skewed, "B")
    origin = make(mat, unreal.MaterialExpressionConstant2Vector, -1360, -40, r=0.0, g=0.0)
    dist = make(mat, unreal.MaterialExpressionDistance, -1300, -160)
    wire(skewed, "", dist, "A")
    wire(origin, "", dist, "B")
    radius = make(mat, unreal.MaterialExpressionLinearInterpolate, -1300, -20,
                  const_a=radius_start, const_b=radius_end)
    wire(age, "", radius, "Alpha")
    ratio = make(mat, unreal.MaterialExpressionDivide, -1120, -120)
    wire(dist, "", ratio, "A")
    wire(radius, "", ratio, "B")
    inv = make(mat, unreal.MaterialExpressionOneMinus, -960, -120)
    wire(ratio, "", inv, "")
    clamped = make(mat, unreal.MaterialExpressionSaturate, -820, -120)
    wire(inv, "", clamped, "")
    shape = make(mat, unreal.MaterialExpressionPower, -680, -120, const_exponent=shape_power)
    wire(clamped, "", shape, "Base")

    # --- noise: two panning samples, offset per particle ---------------------
    # ParticleRandom is a scalar; append it against a decorrelated copy of itself
    # to get a 2D offset, so two sprites never land on the same noise texel.
    rnd_v = make(mat, unreal.MaterialExpressionMultiply, -1780, 420, const_b=0.61)
    wire(rnd, "", rnd_v, "A")
    offset = make(mat, unreal.MaterialExpressionAppendVector, -1620, 400)
    wire(rnd, "", offset, "A")
    wire(rnd_v, "", offset, "B")

    tc_b = make(mat, unreal.MaterialExpressionTextureCoordinate, -1780, 640,
                u_tiling=tc_scale_b[0], v_tiling=tc_scale_b[1])
    samples = []
    for index, (coords, speed) in enumerate(((tc0, pan_a), (tc_b, pan_b))):
        shifted = make(mat, unreal.MaterialExpressionAdd, -1440, 380 + index * 200)
        wire(coords, "", shifted, "A")
        wire(offset, "", shifted, "B")
        panner = make(mat, unreal.MaterialExpressionPanner, -1260, 380 + index * 200,
                      speed_x=speed[0], speed_y=speed[1])
        wire(shifted, "", panner, "Coordinate")
        sample = make(mat, unreal.MaterialExpressionTextureSample, -1080, 380 + index * 200,
                      texture=noise_tex, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        wire(panner, "", sample, "UVs")
        samples.append(sample)

    # Detail MODULATES the main channel (0.55..1.45) instead of gating it. Two
    # noises multiplied is what left the old flames as disconnected bright islands.
    detail = make(mat, unreal.MaterialExpressionMultiply, -900, 580, const_b=0.9)
    wire(samples[1], "B", detail, "A")
    detail_bias = make(mat, unreal.MaterialExpressionAdd, -760, 580, const_b=0.55)
    wire(detail, "", detail_bias, "A")
    noise = make(mat, unreal.MaterialExpressionMultiply, -620, 440)
    wire(samples[0], noise_chan, noise, "A")
    wire(detail_bias, "", noise, "B")
    noise_gained = make(mat, unreal.MaterialExpressionMultiply, -480, 440, const_b=noise_gain)
    wire(noise, "", noise_gained, "A")

    # --- erosion threshold climbs with age: the puff burns away ---------------
    threshold = make(mat, unreal.MaterialExpressionLinearInterpolate, -620, 260,
                     const_a=erode_start, const_b=erode_end)
    wire(age, "", threshold, "Alpha")
    cut = make(mat, unreal.MaterialExpressionSubtract, -340, 380)
    wire(noise_gained, "", cut, "A")
    wire(threshold, "", cut, "B")
    sharp = make(mat, unreal.MaterialExpressionMultiply, -200, 380, const_b=erode_contrast)
    wire(cut, "", sharp, "A")
    erode = make(mat, unreal.MaterialExpressionSaturate, -80, 380)
    wire(sharp, "", erode, "")

    # Erosion FADES IN with age (1.0 -> the eroded mask). A newborn particle is a
    # solid mass and only the ageing ones break into wisps; eroding from birth is
    # what made the first pass read as a swarm of identical orange flakes instead
    # of a flame with a dense burning core.
    onset_mul = make(mat, unreal.MaterialExpressionMultiply, -340, 180, const_b=erode_ramp)
    wire(age, "", onset_mul, "A")
    onset = make(mat, unreal.MaterialExpressionSaturate, -200, 180)
    wire(onset_mul, "", onset, "")
    erode_mix = make(mat, unreal.MaterialExpressionLinearInterpolate, -60, 200, const_a=1.0)
    wire(erode, "", erode_mix, "B")
    wire(onset, "", erode_mix, "Alpha")

    mask = make(mat, unreal.MaterialExpressionMultiply, 60, 120)
    wire(shape, "", mask, "A")
    wire(erode_mix, "", mask, "B")
    return mask


def author_fire(noise_tex):
    name = "M_AH_FireSprite"
    path = MAT_DIR + "/" + name
    mat = unreal.load_asset(path) or TOOLS.create_asset(name, MAT_DIR, unreal.Material,
                                                        unreal.MaterialFactoryNew())
    if not mat:
        unreal.log_error("[VFXMat] failed to create " + name)
        return False
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("used_with_niagara_sprites", True)
    MEL.delete_all_material_expressions(mat)

    age = make(mat, unreal.MaterialExpressionParticleRelativeTime, -1780, -320)
    rnd = make(mat, unreal.MaterialExpressionParticleRandom, -1960, 420)
    pc = make(mat, unreal.MaterialExpressionParticleColor, -700, -420)

    # Flame: mass shrinks as it rises (radius 0.52 -> 0.22) and erodes hard, so the
    # tips break into separate licks the way a real flame tapers into smoke.
    mask = build_common(
        mat, noise_tex, age, rnd,
        tc_scale_b=(2.1, 1.3), pan_a=(0.03, -0.62), pan_b=(-0.05, -1.05),
        noise_chan="R",
        radius_start=0.52, radius_end=0.22, shape_power=1.6,
        erode_start=0.05, erode_end=0.88, erode_contrast=2.0, noise_gain=1.35,
        erode_ramp=2.4, aspect=(1.7, 0.8))

    # Colour over life: white-hot core -> orange body -> dying red before it dies.
    hot = make(mat, unreal.MaterialExpressionConstant3Vector, -700, -260,
               constant=unreal.LinearColor(1.0, 0.72, 0.34, 1.0))
    body = make(mat, unreal.MaterialExpressionConstant3Vector, -700, -180,
                constant=unreal.LinearColor(1.0, 0.24, 0.035, 1.0))
    dying = make(mat, unreal.MaterialExpressionConstant3Vector, -700, 20,
                 constant=unreal.LinearColor(0.30, 0.035, 0.004, 1.0))
    early_t = make(mat, unreal.MaterialExpressionSaturate, -540, -340)
    early_mul = make(mat, unreal.MaterialExpressionMultiply, -680, -340, const_b=2.2)
    wire(age, "", early_mul, "A")
    wire(early_mul, "", early_t, "")
    warm = make(mat, unreal.MaterialExpressionLinearInterpolate, -400, -220)
    wire(hot, "", warm, "A")
    wire(body, "", warm, "B")
    wire(early_t, "", warm, "Alpha")

    late_sub = make(mat, unreal.MaterialExpressionSubtract, -680, 100, const_b=0.45)
    wire(age, "", late_sub, "A")
    late_mul = make(mat, unreal.MaterialExpressionMultiply, -540, 100, const_b=1.8)
    wire(late_sub, "", late_mul, "A")
    late_t = make(mat, unreal.MaterialExpressionSaturate, -400, 100)
    wire(late_mul, "", late_t, "")
    ramp = make(mat, unreal.MaterialExpressionLinearInterpolate, -240, -140)
    wire(warm, "", ramp, "A")
    wire(dying, "", ramp, "B")
    wire(late_t, "", ramp, "Alpha")

    # Brightness decays so the tips read as cooling gas, not fresh flame.
    inv_age = make(mat, unreal.MaterialExpressionOneMinus, -540, 200)
    wire(age, "", inv_age, "")
    # ^1.8, not ^1.0: the tips DO fragment in a real flame, but the fragments dim as
    # fast as they break up. At a linear falloff they stayed full-brightness orange
    # and read as burning confetti drifting off the fire; at ^2.6 the whole upper
    # half of the flame disappeared.
    brightness = make(mat, unreal.MaterialExpressionPower, -400, 200, const_exponent=1.8)
    wire(inv_age, "", brightness, "Base")

    # Soft intersection with world geometry.
    depth = make(mat, unreal.MaterialExpressionDepthFade, 60, 300,
                 opacity_default=1.0, fade_distance_default=70.0)

    tinted = make(mat, unreal.MaterialExpressionMultiply, 220, -200)
    wire(pc, "", tinted, "A")
    wire(ramp, "", tinted, "B")
    shaped = make(mat, unreal.MaterialExpressionMultiply, 380, -140)
    wire(tinted, "", shaped, "A")
    wire(mask, "", shaped, "B")
    dimmed = make(mat, unreal.MaterialExpressionMultiply, 540, -80)
    wire(shaped, "", dimmed, "A")
    wire(brightness, "", dimmed, "B")
    emissive = make(mat, unreal.MaterialExpressionMultiply, 700, -20)
    wire(dimmed, "", emissive, "A")
    wire(depth, "", emissive, "B")
    MEL.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log("[VFXMat] authored " + name)
    return True


def author_smoke(noise_tex):
    name = "M_AH_SmokeSoft"
    path = MAT_DIR + "/" + name
    mat = unreal.load_asset(path) or TOOLS.create_asset(name, MAT_DIR, unreal.Material,
                                                        unreal.MaterialFactoryNew())
    if not mat:
        unreal.log_error("[VFXMat] failed to create " + name)
        return False
    # Lit, not unlit: unlit smoke is a flat cutout in every frame. Volumetric
    # non-directional is the cheap particle lighting mode, so the plumes take the
    # sky and the fires' own point lights.
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("translucency_lighting_mode",
                            unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("used_with_niagara_sprites", True)
    MEL.delete_all_material_expressions(mat)

    age = make(mat, unreal.MaterialExpressionParticleRelativeTime, -1780, -320)
    rnd = make(mat, unreal.MaterialExpressionParticleRandom, -1960, 420)
    pc = make(mat, unreal.MaterialExpressionParticleColor, -700, -420)

    # Smoke: mass GROWS (radius 0.15 -> 0.50) — the fake size-over-life. Erosion is
    # gentler than fire's so puffs thin out rather than shatter.
    mask = build_common(
        mat, noise_tex, age, rnd,
        tc_scale_b=(1.7, 1.7), pan_a=(0.012, -0.055), pan_b=(-0.021, -0.032),
        noise_chan="G",
        radius_start=0.15, radius_end=0.50, shape_power=1.4,
        erode_start=-0.20, erode_end=0.72, erode_contrast=2.6, noise_gain=1.15,
        erode_ramp=1.3, aspect=(1.05, 0.95))

    # Fade in over the first 12% of life and out over the last 45%: without this
    # every puff pops into and out of existence at full opacity.
    fade_in_div = make(mat, unreal.MaterialExpressionDivide, -540, 700, const_b=0.12)
    wire(age, "", fade_in_div, "A")
    fade_in = make(mat, unreal.MaterialExpressionSaturate, -400, 700)
    wire(fade_in_div, "", fade_in, "")
    inv_age = make(mat, unreal.MaterialExpressionOneMinus, -540, 820)
    wire(age, "", inv_age, "")
    fade_out_mul = make(mat, unreal.MaterialExpressionMultiply, -400, 820, const_b=2.2)
    wire(inv_age, "", fade_out_mul, "A")
    fade_out = make(mat, unreal.MaterialExpressionSaturate, -260, 820)
    wire(fade_out_mul, "", fade_out, "")

    depth = make(mat, unreal.MaterialExpressionDepthFade, 60, 300,
                 opacity_default=1.0, fade_distance_default=140.0)

    op1 = make(mat, unreal.MaterialExpressionMultiply, 220, 160)
    wire(pc, "A", op1, "A")
    wire(mask, "", op1, "B")
    op2 = make(mat, unreal.MaterialExpressionMultiply, 380, 220)
    wire(op1, "", op2, "A")
    wire(fade_in, "", op2, "B")
    op3 = make(mat, unreal.MaterialExpressionMultiply, 540, 280)
    wire(op2, "", op3, "A")
    wire(fade_out, "", op3, "B")
    opacity = make(mat, unreal.MaterialExpressionMultiply, 700, 340)
    wire(op3, "", opacity, "A")
    wire(depth, "", opacity, "B")
    MEL.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    # Albedo cools and lightens with age: dense soot at the source, dispersed ash
    # grey by the top of the plume.
    # Soot albedo stays LOW. Volumetric non-directional lighting under a bright
    # inscattering sky turns a 0.5 albedo into a white blob that is brighter than
    # the sky behind it - distant smoke has to sit darker than its background or it
    # reads as a cloud of steam pasted over the skyline.
    soot = make(mat, unreal.MaterialExpressionConstant3Vector, -700, -260,
                constant=unreal.LinearColor(0.080, 0.070, 0.065, 1.0))
    ash = make(mat, unreal.MaterialExpressionConstant3Vector, -700, -180,
               constant=unreal.LinearColor(0.26, 0.25, 0.245, 1.0))
    cool_t = make(mat, unreal.MaterialExpressionSaturate, -540, -340)
    # 0.75, not 1.35: at the faster ramp a 13-second smoke column was ash-grey for
    # almost its whole life, so the plume never held any soot mass.
    cool_mul = make(mat, unreal.MaterialExpressionMultiply, -680, -340, const_b=0.75)
    wire(age, "", cool_mul, "A")
    wire(cool_mul, "", cool_t, "")
    ramp = make(mat, unreal.MaterialExpressionLinearInterpolate, -400, -220)
    wire(soot, "", ramp, "A")
    wire(ash, "", ramp, "B")
    wire(cool_t, "", ramp, "Alpha")
    base = make(mat, unreal.MaterialExpressionMultiply, 220, -200)
    wire(pc, "", base, "A")
    wire(ramp, "", base, "B")
    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # A small warm lift on the youngest smoke only: the fire lighting its own
    # column. Decays at ^6 so it never becomes a glowing sheet.
    glow_fall = make(mat, unreal.MaterialExpressionPower, -400, -40, const_exponent=6.0)
    wire(inv_age, "", glow_fall, "Base")
    glow_tint = make(mat, unreal.MaterialExpressionConstant3Vector, -400, 40,
                     constant=unreal.LinearColor(0.09, 0.032, 0.008, 1.0))
    glow = make(mat, unreal.MaterialExpressionMultiply, -240, 0)
    wire(glow_tint, "", glow, "A")
    wire(glow_fall, "", glow, "B")
    MEL.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log("[VFXMat] authored " + name)
    return True


noise = unreal.load_asset(NOISE_TEX)
if not noise:
    unreal.log_error("[VFXMat] noise texture missing at " + NOISE_TEX
                     + " — run BakeErebusTextures.py then ImportErebusTextures.py")
    ok = False
else:
    ok = author_smoke(noise)
    ok = author_fire(noise) and ok
if not ok:
    unreal.log_error("[VFXMat] sprite material authoring failed")
