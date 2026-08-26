"""Make the 50-clip sound library playable: cues, routing, and semantic ids.

Scripts/ImportAssetDrop.py brings the pack in as bare SoundWaves under
/Game/Ashes/Audio/Library. A SoundWave is not usable content in this project: gameplay
only ever asks UAHAudioSubsystem for a semantic id, and anything played without the
project's attenuation, concurrency and submix assets plays at full volume through the
master bus and is audible from anywhere in the level.

This groups all 50 clips into sixteen banks, authors one SoundCue per bank (a
SoundNodeRandom over the bank's variations, so nothing repeats back to back), routes each
through the same mix assets the existing cues use, and registers every bank in
DA_AudioPalette_Default under a `Library.*` id. Three events that were still synthesised
placeholders are repointed at the recorded banks.

Run with UnrealEditor-Cmd; idempotent, re-running rebuilds the cues in place.
"""

import unreal


LIBRARY_DIR = "/Game/Ashes/Audio/Library"
CUE_DIR = "/Game/Ashes/Audio/Cues"
PALETTE_PATH = "/Game/Ashes/Audio/DA_AudioPalette_Default"

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
REPORT = []

# Bank -> the SoundWave stems it randomises over. Split by what the player needs to tell
# apart, not by what the pack shipped in one folder: a rock impact and a metal impact are
# different information, so they are different banks even though both are "impact".
BANKS = {
    "Explosion":      ["ExplosionLarge11", "ExplosionMedium21"],
    "GunshotHeavy":   ["Gunshot11", "Gunshot71"],
    "GunshotEnergy":  ["SciFiGun11"],
    "WeaponHandling": ["SciFiGun1Reload", "DrawWeaponMetal11"],
    "ImpactMetal":    ["ShieldMetalImpact13", "HitGeneric21", "HitGeneric51"],
    "ImpactWood":     ["ShieldWoodImpact21", "Wood148", "WoodChop14", "NailWood11"],
    "ImpactRock":     ["RockImpact11", "RockImpact37", "RockLargeDebris13"],
    "Melee":          ["Punch21", "Stab41"],
    "Whoosh":         ["Whoosh11", "Whoosh41", "FireWhoosh215"],
    "CreatureVoice":  ["Creature121", "Zombie1Short101"],
    "Door":           ["DoorOpen41", "DoorClose41", "WoodAndDoorCreak01", "WoodMove21"],
    "Pickup":         ["SpecialCollectible91", "SpecialCollectible261", "CoinBag31", "Coins21",
                       "BagHandle15", "BookHandle15", "BookPage12", "CashRegister12"],
    "Interface":      ["Interface11", "Interface33", "MagicalInterface51", "MagicalInterface81",
                       "SciFiInterface81"],
    "Water":          ["WaterSplash52", "Underwater01"],
    "Industry":       ["Blacksmithing11", "SawingWood11"],
    # Loops get a SoundNodeLooping and stay LoadOnDemand; the rest are inlined one-shots.
    "AmbienceWind":   ["AmbientWindLoop1"],
    "AmbienceRain":   ["AmbientRainModerateLoop1"],
    "AmbienceBirds":  ["AmbientBirdsLoop04"],
    "AmbienceWater":  ["AmbientWaterStreamCalm3"],
    "AmbienceFire":   ["FireBurningLoop2"],
}

LOOPING_BANKS = {"AmbienceWind", "AmbienceRain", "AmbienceBirds", "AmbienceWater", "AmbienceFire"}
UI_BANKS = {"Interface", "Pickup"}

# Existing semantic events worth repointing. Everything else keeps what it has: the SciFi
# pack already owns weapon fire, footsteps and the UI stingers, and the player's own hurt and
# death voices must not become creature noises.
PALETTE_EVENTS = {
    "Combat.Grenade": "Explosion",
    "Weapon.M91.Impact": "ImpactMetal",
    "Combat.Melee": "Melee",
}


def _log(message):
    REPORT.append(message)


def _load(path):
    return unreal.load_asset(path) if EAL.does_asset_exist(path) else None


def _require(path):
    asset = _load(path)
    if not asset:
        raise RuntimeError("required audio asset missing: " + path)
    return asset


def bank_waves(bank, stems):
    waves = []
    for stem in stems:
        path = "%s/SW_%s" % (LIBRARY_DIR, stem)
        wave = _load(path)
        if not wave:
            raise RuntimeError("library wave missing: %s (run Scripts/ImportAssetDrop.py)" % path)
        if bank in LOOPING_BANKS:
            wave.set_editor_property("looping", True)
            wave.set_editor_property(
                "loading_behavior", unreal.SoundWaveLoadingBehavior.LOAD_ON_DEMAND)
        else:
            # A one-shot fires on a frame the player is already reacting to; a streamed miss
            # is a silent hit.
            wave.set_editor_property(
                "loading_behavior", unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
        EAL.save_asset(path, only_if_is_dirty=False)
        waves.append(wave)
    return waves


def author_cue(bank, waves):
    is_ui = bank in UI_BANKS
    is_loop = bank in LOOPING_BANKS
    attenuation = _require("/Game/Ashes/Audio/Mix/" + ("ATT_UI" if is_ui else "ATT_World3D"))
    concurrency = _require("/Game/Ashes/Audio/Mix/" + ("CONC_UI" if is_ui else "CONC_World"))
    submix_name = "SM_UI" if is_ui else ("SM_Ambience" if is_loop else "SM_World")
    submix = _require("/Game/Ashes/Audio/Submixes/" + submix_name)

    name = "SC_Lib_" + bank
    path = "%s/%s" % (CUE_DIR, name)
    cue = _load(path)
    if cue and cue.get_class().get_name() != "SoundCue":
        EAL.delete_asset(path)
        cue = None
    if not cue:
        cue = TOOLS.create_asset(name, CUE_DIR, unreal.SoundCue, unreal.SoundCueFactoryNew())
    if not cue:
        raise RuntimeError("could not create cue " + path)

    players = []
    for wave in waves:
        player = unreal.new_object(unreal.SoundNodeWavePlayer, cue)
        player.set_editor_property("sound_wave_asset_ptr", wave)
        if is_loop:
            player.set_editor_property("looping", True)
        players.append(player)

    if len(players) > 1:
        top = unreal.new_object(unreal.SoundNodeRandom, cue)
        top.set_editor_property("child_nodes", players)
        top.set_editor_property("randomize_without_replacement", True)
        top.set_editor_property("weights", [1.0] * len(players))
    else:
        top = players[0]

    if is_loop:
        looping = unreal.new_object(unreal.SoundNodeLooping, cue)
        looping.set_editor_property("child_nodes", [top])
        looping.set_editor_property("loop_indefinitely", True)
        top = looping

    attenuated = unreal.new_object(unreal.SoundNodeAttenuation, cue)
    attenuated.set_editor_property("child_nodes", [top])
    attenuated.set_editor_property("attenuation_settings", attenuation)

    cue.set_editor_property("first_node", attenuated)
    cue.set_editor_property("override_concurrency", False)
    cue.set_editor_property("concurrency_set", {concurrency})
    cue.set_editor_property("sound_submix_object", submix)
    EAL.save_asset(path, only_if_is_dirty=False)

    # Read back: a cue whose graph failed to attach plays nothing and logs nothing.
    if not _load(path).get_editor_property("first_node"):
        raise RuntimeError("%s has no first_node; its graph did not attach" % name)
    _log("%s -> %d variation(s), %s" % (name, len(players), submix_name))
    return path


def bind_palette(cue_paths):
    palette = _require(PALETTE_PATH)
    events = palette.get_editor_property("events")
    # Every bank gets a Library.* id so all fifty clips are reachable by semantic name,
    # not just the three that replace an existing event.
    for bank, path in cue_paths.items():
        events[unreal.Name("Library." + bank)] = _require(path)
    for event_name, bank in PALETTE_EVENTS.items():
        events[unreal.Name(event_name)] = _require(cue_paths[bank])
    palette.set_editor_property("events", events)
    EAL.save_asset(PALETTE_PATH, only_if_is_dirty=False)

    written = _load(PALETTE_PATH).get_editor_property("events")
    for event_name, bank in PALETTE_EVENTS.items():
        bound = written[unreal.Name(event_name)]
        if not bound or ("SC_Lib_" + bank) not in str(bound):
            raise RuntimeError("palette event %s did not bind to %s" % (event_name, bank))
    for bank in cue_paths:
        if unreal.Name("Library." + bank) not in written:
            raise RuntimeError("palette is missing Library.%s" % bank)
    _log("palette: %d Library ids, %d existing events repointed"
         % (len(cue_paths), len(PALETTE_EVENTS)))


def main():
    used = sum(len(stems) for stems in BANKS.values())
    cue_paths = {}
    for bank, stems in sorted(BANKS.items()):
        cue_paths[bank] = author_cue(bank, bank_waves(bank, stems))
    bind_palette(cue_paths)
    _log("%d clips across %d banks" % (used, len(BANKS)))

    import os
    report_path = os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
        "SoundLibraryReport.txt")
    with open(report_path, "w") as handle:
        handle.write("\n".join(REPORT) + "\n")


main()
