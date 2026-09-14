"""Make the 50-clip sound library playable: cues, routing, and semantic ids.

Scripts/ImportAssetDrop.py brings the pack in as bare SoundWaves under
/Game/Ashes/Audio/Library. BANKS below is the authority on what the game ships: a wave in that
folder which no bank claims is deleted, along with any cue and palette id left behind. That is
how clips pulled from the source pack leave the project instead of lingering as orphans. A SoundWave is not usable content in this project: gameplay
only ever asks UAHAudioSubsystem for a semantic id, and anything played without the
project's attenuation, concurrency and submix assets plays at full volume through the
master bus and is audible from anywhere in the level.

This groups the clips into banks, authors one SoundCue per bank (a
SoundNodeRandom over the bank's variations, so nothing repeats back to back), routes each
through the same mix assets the existing cues use, and registers every bank in
DA_AudioPalette_Default under a `Library.*` id. Every semantic event that was still a
synthesised placeholder or a mismatched SciFi-pack stand-in is repointed at a recorded bank.

Run with UnrealEditor-Cmd; idempotent, re-running rebuilds the cues in place.
"""

import os

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
    # Gunshot11 alone, by direction: the M91 fires "Gunshot 1-1.wav", not a random pair.
    "GunshotHeavy":   ["Gunshot11"],
    "WeaponHandling": ["DrawWeaponMetal11"],
    "ImpactRock":     ["RockImpact11", "RockImpact37"],
    "Melee":          ["Punch21"],
    "Whoosh":         ["FireWhoosh215"],
    "CreatureVoice":  ["Creature121", "Zombie1Short101"],
    "Door":           ["DoorOpen41", "DoorClose41", "WoodAndDoorCreak01"],
    "Pickup":         ["BagHandle15", "CoinBag31"],
    "Industry":       ["Blacksmithing11"],
    # The player's feet. Walk steps are the rock impact, one per stride; the sprint layer is
    # a recorded running loop the player character carries while sprinting (a 6-second sample
    # retriggered every step would stack twenty deep). RockImpact11 is deliberately shared
    # with the ImpactRock bank - same clip, two semantic jobs.
    "FootstepRock":   ["RockImpact11"],
    "FootstepSprint": ["SpinopelRunningInPackedSnow475140"],
    # Loops get a SoundNodeLooping and stay LoadOnDemand; the rest are inlined one-shots.
    "AmbienceWind":   ["AmbientWindLoop1"],
    "AmbienceBirds":  ["AmbientBirdsLoop04"],
    "AmbienceFire":   ["FireBurningLoop2"],
}

LOOPING_BANKS = {"AmbienceWind", "AmbienceBirds", "AmbienceFire", "FootstepSprint"}
# No bank is UI. Pickup used to be, and that was wrong: UI.Pickup is raised through
# PlayWorldCue at the item's location, so ATT_UI/SM_UI made a dropped magazine audible at
# full volume from the far end of the level. Everything here is a world sound.
UI_BANKS = set()

# Semantic events this script owns. Each one was either a synthesised placeholder (a sine and
# noise render from GeneratePhase42Audio.py) or a SciFi-pack stand-in that did not match the
# thing making the sound - the M91 is a bolt-action rifle and was firing a laser. The player's
# own hurt/death voices stay on their existing cues: those are human, and must not become
# creature noises.
PALETTE_EVENTS = {
    "Combat.Grenade": "Explosion",
    "Combat.Melee": "Melee",
    "Weapon.M91.Fire": "GunshotHeavy",
    "Weapon.M91.Reload": "WeaponHandling",
    "Weapon.M91.Impact": "ImpactRock",
    "UI.Pickup": "Pickup",
    # Taken over from the SciFi metal steps, by direction. The run event is new: the player
    # character resolves it into a looping component (AAHCombatPlayerCharacter::UpdateSprintLoop)
    # and silences its per-step one-shots while the loop carries the feet.
    "Player.Footstep": "FootstepRock",
    "Player.Footstep.Run": "FootstepSprint",
}
# Events bound straight to an existing cue instead of to a library bank.
PALETTE_CUES = {
    # UINegative is the "action denied" stinger. It was firing on every line of dialogue, so
    # the whole script read as a stream of errors. UISelect is the neutral blip.
    "UI.Dialogue": "/Game/Ashes/Audio/Cues/SC_SciFi_UISelect",
}
# Events this script previously repointed at banks that no longer exist, and the cue each must
# be handed back to. Empty now that Weapon.M91.Impact has ImpactRock.
PALETTE_RESTORE = {}


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
    for event_name, cue_path in PALETTE_CUES.items():
        events[unreal.Name(event_name)] = _require(cue_path)
    palette.set_editor_property("events", events)
    EAL.save_asset(PALETTE_PATH, only_if_is_dirty=False)

    written = _load(PALETTE_PATH).get_editor_property("events")
    for event_name, bank in PALETTE_EVENTS.items():
        bound = written[unreal.Name(event_name)]
        if not bound or ("SC_Lib_" + bank) not in str(bound):
            raise RuntimeError("palette event %s did not bind to %s" % (event_name, bank))
    for event_name, cue_path in PALETTE_CUES.items():
        bound = written[unreal.Name(event_name)]
        if not bound or cue_path.split("/")[-1] not in str(bound):
            raise RuntimeError("palette event %s did not bind to %s" % (event_name, cue_path))
    for bank in cue_paths:
        if unreal.Name("Library." + bank) not in written:
            raise RuntimeError("palette is missing Library.%s" % bank)
    _log("palette: %d Library ids, %d existing events repointed"
         % (len(cue_paths), len(PALETTE_EVENTS) + len(PALETTE_CUES)))


def _erase(package_path):
    """Delete an asset AND its file.

    EditorAssetLibrary.delete_asset drops the asset from the registry but leaves the .uasset
    sitting on disk, so a later run cannot even see it to retry and the file returns on the
    next editor start.
    """
    EAL.delete_asset(package_path)
    on_disk = os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()),
        package_path.split(".")[0][len("/Game/"):] + ".uasset")
    if os.path.isfile(on_disk):
        os.remove(on_disk)


def prune(cue_paths):
    """Delete waves, cues and palette ids for anything no longer in BANKS."""
    wanted_waves = {"SW_" + stem for stems in BANKS.values() for stem in stems}
    removed_waves = removed_cues = 0
    for asset in EAL.list_assets(LIBRARY_DIR, recursive=False, include_folder=False):
        name = asset.split("/")[-1].split(".")[0]
        if name.startswith("SW_") and name not in wanted_waves:
            _erase(asset)
            removed_waves += 1
    for asset in EAL.list_assets(CUE_DIR, recursive=False, include_folder=False):
        name = asset.split("/")[-1].split(".")[0]
        if name.startswith("SC_Lib_") and name[len("SC_Lib_"):] not in BANKS:
            _erase(asset)
            removed_cues += 1

    # Sweep files the registry no longer lists but which are still on disk from an earlier
    # half-finished delete.
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    for folder, keep, prefix in ((LIBRARY_DIR, wanted_waves, "SW_"),
                                 (CUE_DIR, {"SC_Lib_" + b for b in BANKS}, "SC_Lib_")):
        disk_folder = os.path.join(content, folder[len("/Game/"):])
        for filename in sorted(os.listdir(disk_folder)) if os.path.isdir(disk_folder) else []:
            if not filename.endswith(".uasset"):
                continue
            name = filename[:-len(".uasset")]
            if name.startswith(prefix) and name not in keep:
                os.remove(os.path.join(disk_folder, filename))
                if prefix == "SW_":
                    removed_waves += 1
                else:
                    removed_cues += 1

    palette = _require(PALETTE_PATH)
    events = palette.get_editor_property("events")
    stale = [key for key in list(events.keys())
             if str(key).startswith("Library.") and str(key)[len("Library."):] not in BANKS]
    for key in stale:
        del events[key]
    handed_back = 0
    for event_name, cue_path in PALETTE_RESTORE.items():
        cue = _load(cue_path)
        if cue:
            events[unreal.Name(event_name)] = cue
            handed_back += 1
        elif unreal.Name(event_name) in events:
            del events[unreal.Name(event_name)]
    palette.set_editor_property("events", events)
    EAL.save_asset(PALETTE_PATH, only_if_is_dirty=False)
    # Report what is actually on disk afterwards, not what this function thought it changed:
    # deleting a cue can drop its palette entry as a side effect, which made the stale count
    # read zero while the palette had in fact been correct all along.
    written = _load(PALETTE_PATH).get_editor_property("events")
    library_ids = sorted(str(k)[len("Library."):] for k in written.keys()
                         if str(k).startswith("Library."))
    if set(library_ids) != set(BANKS):
        raise RuntimeError("palette Library ids %s do not match BANKS" % library_ids)
    _log("pruned %d waves, %d cues; palette holds %d Library ids, all matching BANKS; "
         "%d events handed back" % (removed_waves, removed_cues, len(library_ids), handed_back))


def main():
    used = sum(len(stems) for stems in BANKS.values())
    cue_paths = {}
    for bank, stems in sorted(BANKS.items()):
        cue_paths[bank] = author_cue(bank, bank_waves(bank, stems))
    bind_palette(cue_paths)
    prune(cue_paths)
    _log("%d clips across %d banks" % (used, len(BANKS)))

    import os
    report_path = os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
        "SoundLibraryReport.txt")
    with open(report_path, "w") as handle:
        handle.write("\n".join(REPORT) + "\n")


main()
