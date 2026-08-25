"""Author the shipped enemy Primary Data Assets and encounter manifests.

Run with UnrealEditor-Cmd after the AshesOfHeavenEditor target has been built, and after
ImportEnemyModels.py has written Saved/EnemyModelManifest.json. The script is idempotent:
existing assets are updated in place instead of duplicated.

The roster is four creature archetypes sharing one streaming and AI pipeline:

  Pilgrim  humanoid alien skirmisher, rifle, keeps its distance
  Warden   heavy armoured revenant, rifle, shield and teleport ability scalars
  Hound    quadruped biter, no weapon at all, closes and attacks in contact
  Spider   armoured bio-mech crawler, no weapon, slower and much harder to kill

Sizes are not authored here. Each source model is in its own units - one is in metres, one is
a forty-metre spider - so the mesh scale, capsule and body offset are derived from the imported
bounds recorded in the manifest against a target height in centimetres.
"""

import json
import os

import unreal


ENEMY_PATH = "/Game/Ashes/Data/Enemies"
ENCOUNTER_PATH = "/Game/Ashes/Data/Encounters"
MANIFEST_PATH = os.path.join(unreal.Paths.project_saved_dir(), "EnemyModelManifest.json")

PILGRIM_CLASS = "/Script/AshesOfHeaven.AHVeilPilgrimCharacter"
WARDEN_CLASS = "/Script/AshesOfHeaven.AHVeilWardenCharacter"


# --- roster ----------------------------------------------------------------------------
# ponytail: the beasts reuse AAHVeilPilgrimCharacter as their concrete combat class. It is the
# only thing the class still supplies (AAHCombatantCharacter is abstract, and every other value
# comes from the definition), so two more empty subclasses would buy nothing. Add real classes
# when a beast needs C++ behaviour of its own, the way the Warden needed its shield cycle.
ARCHETYPES = {
    "Pilgrim": {
        "model": "Stalker",
        "combat_class": PILGRIM_CLASS,
        "display_name": "Veil Stalker",
        "target_height_cm": 190.0,
        "capsule_radius": 34.0,
        "health": 120.0,
        "armor": 35.0,
        "speed": 340.0,
        "headshot": 2.0,
        "threat": 1.0,
        "currency": 10,
        "marker_color": (0.55, 0.08, 0.85, 1.0),
        "voice": "Alien",
        # Sight ranges cover the distance the director now spawns at. They were 2400-3200 while
        # bodies were placed 27-42m out, so nothing could ever notice the player and the fight
        # never started.
        "ranged": {
            "sight_range": 4000.0,
            "accuracy": 0.68,
            "max_aim_error": 10.0,
            "prefer_cover": True,
            "preferred_range": 1200.0,
            "minimum_range": 650.0,
            "burst_rounds": 4,
        },
        "abilities": {},
    },
    "Warden": {
        "model": "Ravager",
        "combat_class": WARDEN_CLASS,
        "display_name": "Veil Revenant",
        "target_height_cm": 235.0,
        "capsule_radius": 46.0,
        "health": 260.0,
        "armor": 120.0,
        "speed": 215.0,
        "headshot": 1.5,
        "threat": 4.0,
        "currency": 35,
        "marker_color": (0.85, 0.22, 0.10, 1.0),
        "voice": "Robo",
        # The heavy body gets the heavy bank, so the Revenant is audibly a different threat from
        # the skirmishers before the player has picked it out of the fog.
        "shot_cue": "SC_SciFi_LazerHeavy",
        "ranged": {
            "sight_range": 4500.0,
            "accuracy": 0.78,
            "max_aim_error": 6.0,
            "prefer_cover": False,
            "preferred_range": 1200.0,
            "minimum_range": 650.0,
            "burst_rounds": 5,
        },
        "abilities": {
            "ShieldCycleSeconds": 8.0,
            "ShieldDamageMultiplier": 0.35,
            "TeleportCycleSeconds": 6.0,
            "TeleportDistance": 520.0,
        },
    },
    "Hound": {
        "model": "Hound",
        "combat_class": PILGRIM_CLASS,
        "display_name": "Veil Hound",
        "target_height_cm": 115.0,
        "capsule_radius": 40.0,
        "health": 70.0,
        "armor": 0.0,
        "speed": 640.0,
        "headshot": 1.8,
        "threat": 1.5,
        "currency": 8,
        "marker_color": (0.95, 0.30, 0.15, 1.0),
        "voice": "Alien",
        # A pack of these is the pressure that makes the player stop holding a firing line. Fast,
        # fragile, and 22 damage every 1.15s - two hounds in contact is roughly a rifle's DPS,
        # which is survivable long enough to back off and shoot them.
        # Six takes ship with this model; this is the one it stands and moves in.
        "loop_clip": "Idle_Aggressive",
        "melee": {
            "damage": 22.0,
            "range": 180.0,
            "radius": 34.0,
            "cooldown": 1.15,
            "sight_range": 3800.0,
        },
        "abilities": {},
    },
    "Spider": {
        "model": "Spider",
        "combat_class": PILGRIM_CLASS,
        "display_name": "Bio-Mech Crawler",
        "target_height_cm": 140.0,
        "capsule_radius": 52.0,
        "health": 165.0,
        "armor": 55.0,
        "speed": 430.0,
        "headshot": 1.4,
        "threat": 2.5,
        "currency": 18,
        "marker_color": (0.20, 0.80, 0.70, 1.0),
        "voice": "Robo",
        "melee": {
            "damage": 34.0,
            "range": 210.0,
            "radius": 46.0,
            "cooldown": 1.7,
            "sight_range": 3400.0,
        },
        "abilities": {},
    },
}

# Every archetype appears in at least one encounter. The Warden is held back from the generic
# patrol so a routine contact does not open on the heaviest body in the roster.
ENCOUNTERS = {
    "PilgrimPatrol": {"primary": "Pilgrim", "additional": ["Hound", "Spider"], "seed": 1337},
    "PilgrimWarden": {"primary": "Pilgrim", "additional": ["Warden", "Hound", "Spider"], "seed": 8821},
}


def _load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Required asset does not resolve: " + path)
    return asset


def _load_class(path):
    asset_class = unreal.load_class(None, path)
    if asset_class is None:
        raise RuntimeError("Required class does not resolve: " + path)
    return asset_class


def _create_or_load(name, package_path, asset_class):
    object_path = "%s/%s.%s" % (package_path, name, name)
    existing = unreal.load_asset(object_path)
    if existing is not None:
        if not isinstance(existing, asset_class):
            raise RuntimeError("%s exists with class %s" % (object_path, existing.get_class().get_name()))
        return existing

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = asset_tools.create_asset(name, package_path, asset_class, unreal.DataAssetFactory())
    if created is None:
        raise RuntimeError("Failed to create " + object_path)
    return created


def _primary_asset_id(asset_type, name):
    return unreal.PrimaryAssetId(
        primary_asset_type=unreal.PrimaryAssetType(name=asset_type),
        primary_asset_name=name,
    )


def _set_struct(owner, property_name, **values):
    """Author the whole struct. Never merge into what is already on the asset.

    Reading the existing struct and setting only the named keys leaves every other field frozen
    at whatever a previous version of this script wrote. That is how DA_Enemy_Pilgrim and
    DA_Enemy_Warden kept Visuals.AnimClass = ABP_TP_Rifle - the UE mannequin's third-person rifle
    AnimBP - long after they stopped using the mannequin: AAHCombatantCharacter checks AnimClass
    before AnimationSet, so the alien's one idle clip could never play, and re-running the script
    could not clear it because the script never names the field.
    """
    # A freshly constructed struct rejects set_editor_property on EditDefaultsOnly fields, so the
    # struct has to come off the asset - but every field it carries is then overwritten, either
    # with a value this call names or with the C++ default read back off a fresh instance.
    current = owner.get_editor_property(property_name)
    defaults = type(current)().to_dict()
    unknown = sorted(set(values) - set(defaults))
    if unknown:
        raise RuntimeError("%s has no field(s) %s" % (type(current).__name__, ", ".join(unknown)))
    for field, default in defaults.items():
        current.set_editor_property(field, values[field] if field in values else default)
    owner.set_editor_property(property_name, current)


def _read_manifest():
    if not os.path.isfile(MANIFEST_PATH):
        raise RuntimeError("model manifest missing - run Scripts/ImportEnemyModels.py first: " + MANIFEST_PATH)
    with open(MANIFEST_PATH) as handle:
        return json.load(handle)


def _body_fit(entry, target_height_cm, capsule_radius):
    """Mesh scale, capsule and body offset for one imported model.

    The imported bounds are the only source of truth for how big a model actually is, and the Z
    term puts the lowest point of the scaled mesh on the bottom of the capsule, so a model whose
    pivot sits at its centre stops floating at half its own height.

    There is deliberately no lateral term. Centring the mesh's BOUNDS on the capsule axis sounds
    right and is wrong for anything with a long appendage: the Stalker's tail drags its bounds
    centre 92uu off the body, so "centring" pushed the body out of its own capsule and a
    visibility trace at chest height went straight past it - shots would miss a body that is
    plainly in the crosshair. These models are authored with the pivot at the body root, which is
    what the capsule wants anyway.
    """
    source_height = max(1.0, float(entry["height_cm"]))
    scale = target_height_cm / source_height
    half_height = target_height_cm * 0.5
    origin = [float(value) for value in entry["origin"]]
    extent_z = float(entry["extent"][2])
    # Rotated, because the component's relative location is expressed in capsule space while the
    # bounds are in mesh space, and the mesh is turned to face down +X on the way. Yaw leaves Z
    # alone, so the vertical term survives the rotation unchanged.
    offset = unreal.Vector(0.0, 0.0, -half_height - (origin[2] - extent_z) * scale)
    return scale, half_height, capsule_radius, offset


def _author_enemy(name, spec, manifest):
    entry = manifest.get(spec["model"])
    if not entry:
        raise RuntimeError("%s has no imported model %s in the manifest" % (name, spec["model"]))

    # Skeletons are authored facing +Y, matching the combat class default; a creature that comes
    # in sideways is corrected per archetype here rather than by re-exporting the source model.
    mesh_yaw = spec.get("mesh_yaw", -90.0)
    # Keyword arguments on purpose: unreal.Rotator's positional order is (roll, pitch, yaw), so
    # Rotator(0, -90, 0) sets PITCH and lays every creature face-down on the ground.
    mesh_rotation = unreal.Rotator(pitch=0.0, yaw=mesh_yaw, roll=0.0)
    scale, half_height, radius, offset = _body_fit(
        entry, spec["target_height_cm"], spec["capsule_radius"])
    melee = spec.get("melee")
    ranged = spec.get("ranged")
    voice = spec["voice"]
    shot_cue = spec.get("shot_cue", "SC_SciFi_Lazer")

    definition = _create_or_load("DA_Enemy_" + name, ENEMY_PATH, unreal.AHEnemyDefinition)
    definition.set_editor_property("enemy_id", name)
    definition.set_editor_property("combat_class", _load_class(spec["combat_class"]))

    _set_struct(
        definition,
        "combat_defaults",
        faction=unreal.AHFaction.VEIL,
        max_health=spec["health"],
        max_armor=spec["armor"],
        walk_speed=spec["speed"],
        headshot_multiplier=spec["headshot"],
        destroy_on_death=True,
        corpse_life_span=30.0,
        capsule_half_height=half_height,
        capsule_radius=radius,
    )

    ai = {
        "controller_class": _load_class("/Script/AshesOfHeaven.AHCombatAIController"),
        "melee_only": bool(melee),
    }
    if melee:
        ai.update(
            sight_range=melee["sight_range"],
            accuracy=1.0,
            max_aim_error_degrees=0.0,
            prefer_cover=False,
            # A biter's engagement distances are its own reach; the controller clamps these
            # again, but authoring them consistently keeps the asset readable.
            preferred_engagement_range=melee["range"],
            minimum_engagement_range=100.0,
            melee_damage=melee["damage"],
            melee_range=melee["range"],
            melee_radius=melee["radius"],
            melee_cooldown=melee["cooldown"],
        )
    else:
        ai.update(
            sight_range=ranged["sight_range"],
            accuracy=ranged["accuracy"],
            max_aim_error_degrees=ranged["max_aim_error"],
            prefer_cover=ranged["prefer_cover"],
            preferred_engagement_range=ranged["preferred_range"],
            minimum_engagement_range=ranged["minimum_range"],
            burst_rounds=ranged["burst_rounds"],
            min_burst_pause=0.65,
            max_burst_pause=1.45,
        )
    _set_struct(definition, "ai_settings", **ai)

    if melee:
        # No weapon classes at all. UAHEnemyDefinition::IsDataValid allows the empty loadout only
        # because bMeleeOnly is set, which is what makes "this thing has no gun" an authored fact
        # rather than a missing reference.
        _set_struct(definition, "loadout", weapon_classes=[])
    else:
        _set_struct(
            definition,
            "loadout",
            weapon_classes=[_load_class("/Script/AshesOfHeaven.AHWeaponBase")],
            weapon_mesh=_load("/Game/Weapons/Rifle/Meshes/SKM_Rifle.SKM_Rifle"),
            weapon_material=_load("/Game/Weapons/Rifle/Materials/M_Rifle.M_Rifle"),
            capacitor_mesh=_load("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube"),
            capacitor_material=_load("/Game/Ashes/Materials/M_VeilObsidian.M_VeilObsidian"),
            shot_sound=_load("/Game/Ashes/Audio/Cues/%s.%s" % (shot_cue, shot_cue)),
            reload_sound=_load("/Game/Ashes/Audio/Weapons/M91/SC_M91_Reload.SC_M91_Reload"),
            empty_sound=_load("/Game/Ashes/Audio/Weapons/M91/SC_M91_Empty.SC_M91_Empty"),
            impact_sound=_load("/Game/Ashes/Audio/Weapons/M91/SC_M91_Impact.SC_M91_Impact"),
        )

    visuals = {
        "skeletal_mesh": _load(entry["mesh"]),
        # Named explicitly, not left to the struct default. These archetypes used to be UE
        # mannequins and their assets still carried ABP_TP_Rifle; AAHCombatantCharacter checks
        # AnimClass before AnimationSet, so a stale AnimBP on a creature skeleton silently wins
        # over the only clip the creature has.
        "anim_class": None,
        "mesh_scale": unreal.Vector(scale, scale, scale),
        "mesh_offset": offset,
        "mesh_rotation": mesh_rotation,
        "override_mesh_transform": True,
    }
    if entry.get("physics_asset"):
        visuals["physics_asset"] = _load(entry["physics_asset"])
    if entry.get("animations"):
        # There is no AnimBP: these skeletons share nothing with the mannequin, so
        # AAHCombatantCharacter loops AnimationSet[0] in single-node mode. Index 0 is therefore
        # the archetype's resting pose and has to be named here - the manifest lists clips in
        # alphabetical order, which put Attack_Bite_B first and had the Hound biting the air
        # from spawn to death.
        paths = list(entry["animations"])
        loop_clip = spec.get("loop_clip")
        if loop_clip:
            match = next((path for path in paths if loop_clip.lower() in path.lower()), None)
            if match:
                paths.remove(match)
                paths.insert(0, match)
            else:
                unreal.log_error("[Enemies] %s loop clip %r not among %s" % (name, loop_clip, paths))
        visuals["animation_set"] = [_load(path) for path in paths]
    _set_struct(definition, "visuals", **visuals)

    # Desktop is authored empty on purpose. The previous roster overrode its material slot with
    # M_VeilObsidian, and FAHEnemyVisualPayload::OverlayOnto copies a non-empty Materials array
    # over the base, so that would silently repaint every creature body back to a flat tint on the
    # platform that actually ships.
    _set_struct(definition, "desktop_visuals")
    # Mobile keeps the cheap single-material override introduced with the streaming roster: four
    # texture samples per creature slot is not a mobile budget. MI_VeilObsidian_Black is an
    # instance rather than M_VeilObsidian on purpose - that is what keeps the mobile bundle
    # disjoint from the desktop one, which AHEnemyAssetValidationCommandlet checks for.
    _set_struct(
        definition,
        "mobile_visuals",
        materials=[_load("/Game/Ashes/Materials/Instances/MI_VeilObsidian_Black.MI_VeilObsidian_Black")],
    )

    # Voice per archetype, not per faction. The roster is two organic bodies and two mechanical
    # ones, and the biters are the archetypes the player most needs to identify by ear - a hound
    # closing from behind is a sound before it is a silhouette. Armour is still the shared cue:
    # it is the plate reacting, not the creature.
    _set_struct(
        definition,
        "audio",
        voice_palette=_load("/Game/Ashes/Audio/DA_AudioPalette_Veil.DA_AudioPalette_Veil"),
        hurt_sound=_load("/Game/Ashes/Audio/Cues/SC_SciFi_%sHurt.SC_SciFi_%sHurt" % (voice, voice)),
        armor_damage_sound=_load("/Game/Ashes/Audio/Cues/SC_Combat_Armor.SC_Combat_Armor"),
        death_sound=_load("/Game/Ashes/Audio/Cues/SC_SciFi_%sDeath.SC_SciFi_%sDeath" % (voice, voice)),
    )
    _set_struct(
        definition,
        "vfx",
        hit_effect=_load("/Game/Ashes/VFX/NS_Erebus_EmbersNear.NS_Erebus_EmbersNear"),
    )
    _set_struct(
        definition,
        "desktop_vfx",
        spawn_effect=_load("/Game/Ashes/VFX/NS_Erebus_SmokeLocal.NS_Erebus_SmokeLocal"),
        death_effect=_load("/Game/Ashes/VFX/NS_Erebus_FireWreck.NS_Erebus_FireWreck"),
    )
    # Mobile keeps the same bodies and materials - they are the archetype's identity - and only
    # trades the effects down.
    _set_struct(
        definition,
        "mobile_vfx",
        spawn_effect=_load("/Game/Ashes/VFX/NS_Erebus_EmbersNear.NS_Erebus_EmbersNear"),
        death_effect=_load("/Game/Ashes/VFX/NS_Erebus_FireSmall.NS_Erebus_FireSmall"),
    )
    _set_struct(
        definition,
        "loot",
        currency_reward=spec["currency"],
        weapon_can_be_recovered=not melee,
    )
    _set_struct(
        definition,
        "ui_marker",
        display_name=unreal.Text(spec["display_name"]),
        marker_color=unreal.LinearColor(*spec["marker_color"]),
    )
    _set_struct(
        definition,
        "difficulty",
        threat_cost=spec["threat"],
        health_scale=1.0,
        armor_scale=1.0,
        damage_scale=1.0,
        accuracy_scale=1.0,
        ability_scalars=spec["abilities"],
    )
    unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False)

    # Read back the fields whose failure mode is silence: a stale AnimBP, a missing body, and a
    # platform material override are all things that look fine in the log and wrong on screen.
    written = definition.get_editor_property("visuals")
    if written.get_editor_property("anim_class") is not None:
        raise RuntimeError("%s kept a stale Visuals.AnimClass" % name)
    if written.get_editor_property("skeletal_mesh") is None:
        raise RuntimeError("%s has no Visuals.SkeletalMesh" % name)
    if definition.get_editor_property("desktop_visuals").get_editor_property("materials"):
        raise RuntimeError("%s still overrides desktop_visuals materials" % name)
    if not definition.get_editor_property("mobile_visuals").get_editor_property("materials"):
        raise RuntimeError("%s lost its cheap mobile material override" % name)

    unreal.log_warning(
        "[Enemies] %s model=%s scale=%.4f capsule=r%.0f/h%.0f offset=(%.1f, %.1f, %.1f) melee=%s anims=%d" % (
            name, spec["model"], scale, radius, half_height,
            offset.x, offset.y, offset.z, bool(melee), len(entry.get("animations") or [])))
    return definition


def _author_encounter(name, spec):
    definition = _create_or_load("DA_Encounter_" + name, ENCOUNTER_PATH, unreal.AHEncounterDefinition)
    definition.set_editor_property("encounter_id", name)
    definition.set_editor_property("primary_enemy", _primary_asset_id("AHEnemy", spec["primary"]))
    definition.set_editor_property(
        "additional_enemies",
        [_primary_asset_id("AHEnemy", enemy_name) for enemy_name in spec["additional"]],
    )
    # UAHEncounterDefinition::BuildSpawnSequence draws the line-up from this seed, so two
    # encounters with different seeds field different mixes and a checkpoint reload rebuilds
    # the same one.
    definition.set_editor_property("deterministic_seed", spec["seed"])
    definition.set_editor_property("preload_visuals", True)
    definition.set_editor_property("preload_audio", True)
    unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False)
    unreal.log_warning("[Enemies] encounter %s = %s + %s (seed %d)" % (
        name, spec["primary"], ", ".join(spec["additional"]) or "-", spec["seed"]))
    return definition


def main():
    manifest = _read_manifest()
    unreal.EditorAssetLibrary.make_directory(ENEMY_PATH)
    unreal.EditorAssetLibrary.make_directory(ENCOUNTER_PATH)
    for name, spec in ARCHETYPES.items():
        _author_enemy(name, spec, manifest)
    for name, spec in ENCOUNTERS.items():
        _author_encounter(name, spec)
    unreal.log_warning("[Enemies] authored %d archetypes and %d encounter manifests." % (
        len(ARCHETYPES), len(ENCOUNTERS)))


if __name__ == "__main__":
    main()
