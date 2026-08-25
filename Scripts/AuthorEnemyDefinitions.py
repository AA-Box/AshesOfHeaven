"""Author the shipped Pilgrim/Warden Primary Data Assets and encounter manifests.

Run with UnrealEditor-Cmd after the AshesOfHeavenEditor target has been built. The
script is idempotent: existing assets are updated in place instead of duplicated.
"""

import unreal


ENEMY_PATH = "/Game/Ashes/Data/Enemies"
ENCOUNTER_PATH = "/Game/Ashes/Data/Encounters"


def _load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError(f"Required asset does not resolve: {path}")
    return asset


def _load_class(path):
    asset_class = unreal.load_class(None, path)
    if asset_class is None:
        raise RuntimeError(f"Required class does not resolve: {path}")
    return asset_class


def _create_or_load(name, package_path, asset_class):
    object_path = f"{package_path}/{name}.{name}"
    existing = unreal.load_asset(object_path)
    if existing is not None:
        if not isinstance(existing, asset_class):
            raise RuntimeError(f"{object_path} exists with class {existing.get_class().get_name()}")
        return existing

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = asset_tools.create_asset(name, package_path, asset_class, unreal.DataAssetFactory())
    if created is None:
        raise RuntimeError(f"Failed to create {object_path}")
    return created


def _primary_asset_id(asset_type, name):
    return unreal.PrimaryAssetId(
        primary_asset_type=unreal.PrimaryAssetType(name=asset_type),
        primary_asset_name=name,
    )


def _set_struct(owner, property_name, **values):
    value = owner.get_editor_property(property_name)
    for key, item in values.items():
        value.set_editor_property(key, item)
    owner.set_editor_property(property_name, value)


def _author_enemy(name, combat_class, display_name, health, armor, speed, headshot, scale, threat, abilities):
    definition = _create_or_load(f"DA_Enemy_{name}", ENEMY_PATH, unreal.AHEnemyDefinition)
    definition.set_editor_property("enemy_id", name)
    definition.set_editor_property("combat_class", combat_class)

    _set_struct(
        definition,
        "combat_defaults",
        faction=unreal.AHFaction.VEIL,
        max_health=health,
        max_armor=armor,
        walk_speed=speed,
        headshot_multiplier=headshot,
        destroy_on_death=True,
        corpse_life_span=30.0,
    )
    _set_struct(
        definition,
        "ai_settings",
        controller_class=_load_class("/Script/AshesOfHeaven.AHCombatAIController"),
        sight_range=2800.0 if name == "Warden" else 2400.0,
        accuracy=0.78 if name == "Warden" else 0.68,
        max_aim_error_degrees=6.0 if name == "Warden" else 10.0,
        prefer_cover=name != "Warden",
        preferred_engagement_range=1200.0,
        minimum_engagement_range=650.0,
        burst_rounds=5 if name == "Warden" else 4,
        min_burst_pause=0.65,
        max_burst_pause=1.45,
    )
    _set_struct(
        definition,
        "loadout",
        weapon_classes=[_load_class("/Script/AshesOfHeaven.AHWeaponBase")],
        weapon_mesh=_load("/Game/Weapons/Rifle/Meshes/SKM_Rifle.SKM_Rifle"),
        weapon_material=_load("/Game/Weapons/Rifle/Materials/M_Rifle.M_Rifle"),
        capacitor_mesh=_load("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube"),
        capacitor_material=_load("/Game/Ashes/Materials/M_VeilObsidian.M_VeilObsidian"),
        shot_sound=_load("/Game/Ashes/Audio/Weapons/M91/SC_M91_Fire.SC_M91_Fire"),
        reload_sound=_load("/Game/Ashes/Audio/Weapons/M91/SC_M91_Reload.SC_M91_Reload"),
        empty_sound=_load("/Game/Ashes/Audio/Weapons/M91/SC_M91_Empty.SC_M91_Empty"),
        impact_sound=_load("/Game/Ashes/Audio/Weapons/M91/SC_M91_Impact.SC_M91_Impact"),
    )
    _set_struct(
        definition,
        "visuals",
        skeletal_mesh=_load("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"),
        anim_class=_load_class("/Game/Variant_Shooter/Anims/ABP_TP_Rifle.ABP_TP_Rifle_C"),
        physics_asset=_load("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"),
        mesh_scale=unreal.Vector(scale, scale, scale),
    )
    _set_struct(
        definition,
        "audio",
        voice_palette=_load("/Game/Ashes/Audio/DA_AudioPalette_Veil.DA_AudioPalette_Veil"),
        hurt_sound=_load("/Game/Ashes/Audio/Cues/SC_Combat_Hurt.SC_Combat_Hurt"),
        armor_damage_sound=_load("/Game/Ashes/Audio/Cues/SC_Combat_Armor.SC_Combat_Armor"),
        death_sound=_load("/Game/Ashes/Audio/Cues/SC_Combat_Death.SC_Combat_Death"),
    )
    _set_struct(
        definition,
        "vfx",
        hit_effect=_load("/Game/Ashes/VFX/NS_Erebus_EmbersNear.NS_Erebus_EmbersNear"),
    )
    _set_struct(
        definition,
        "desktop_visuals",
        materials=[_load("/Game/Ashes/Materials/M_VeilObsidian.M_VeilObsidian")],
    )
    _set_struct(
        definition,
        "desktop_vfx",
        spawn_effect=_load("/Game/Ashes/VFX/NS_Erebus_SmokeLocal.NS_Erebus_SmokeLocal"),
        death_effect=_load("/Game/Ashes/VFX/NS_Erebus_FireWreck.NS_Erebus_FireWreck"),
    )
    # Mobile bodies were authored to the engine's BasicShapeMaterial, which UE substitutes with
    # an engine default at runtime - an engine material presenting a character on the whole mobile
    # tier. MI_VeilObsidian_Black is the cheap project instance for it, and being an instance
    # rather than M_VeilObsidian keeps the mobile bundle disjoint from the desktop one, which
    # AHEnemyAssetValidationCommandlet requires.
    _set_struct(
        definition,
        "mobile_visuals",
        materials=[_load("/Game/Ashes/Materials/Instances/MI_VeilObsidian_Black.MI_VeilObsidian_Black")],
    )
    _set_struct(
        definition,
        "mobile_vfx",
        spawn_effect=_load("/Game/Ashes/VFX/NS_Erebus_EmbersNear.NS_Erebus_EmbersNear"),
        death_effect=_load("/Game/Ashes/VFX/NS_Erebus_FireSmall.NS_Erebus_FireSmall"),
    )
    _set_struct(
        definition,
        "loot",
        currency_reward=35 if name == "Warden" else 10,
        weapon_can_be_recovered=True,
    )
    _set_struct(
        definition,
        "ui_marker",
        display_name=unreal.Text(display_name),
        marker_color=unreal.LinearColor(0.55, 0.08, 0.85, 1.0),
    )
    _set_struct(
        definition,
        "difficulty",
        threat_cost=threat,
        health_scale=1.0,
        armor_scale=1.0,
        damage_scale=1.0,
        accuracy_scale=1.0,
        ability_scalars=abilities,
    )
    unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False)
    return definition


def _author_encounter(name, primary_enemy, additional_enemies):
    definition = _create_or_load(f"DA_Encounter_{name}", ENCOUNTER_PATH, unreal.AHEncounterDefinition)
    definition.set_editor_property("encounter_id", name)
    definition.set_editor_property("primary_enemy", _primary_asset_id("AHEnemy", primary_enemy))
    definition.set_editor_property(
        "additional_enemies",
        [_primary_asset_id("AHEnemy", enemy_name) for enemy_name in additional_enemies],
    )
    definition.set_editor_property("preload_visuals", True)
    definition.set_editor_property("preload_audio", True)
    unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False)
    return definition


def main():
    unreal.EditorAssetLibrary.make_directory(ENEMY_PATH)
    unreal.EditorAssetLibrary.make_directory(ENCOUNTER_PATH)
    pilgrim = _author_enemy(
        "Pilgrim",
        _load_class("/Script/AshesOfHeaven.AHVeilPilgrimCharacter"),
        "Veil Pilgrim",
        120.0,
        35.0,
        300.0,
        2.0,
        1.0,
        1.0,
        {},
    )
    warden = _author_enemy(
        "Warden",
        _load_class("/Script/AshesOfHeaven.AHVeilWardenCharacter"),
        "Veil Warden",
        220.0,
        110.0,
        220.0,
        1.5,
        1.18,
        4.0,
        {
            "ShieldCycleSeconds": 8.0,
            "ShieldDamageMultiplier": 0.35,
            "TeleportCycleSeconds": 6.0,
            "TeleportDistance": 520.0,
        },
    )
    patrol = _author_encounter("PilgrimPatrol", "Pilgrim", [])
    mixed = _author_encounter("PilgrimWarden", "Pilgrim", ["Warden"])
    unreal.log("Authored AHEnemy:Pilgrim, AHEnemy:Warden and their encounter preload manifests.")


if __name__ == "__main__":
    main()
