"""Import the SciFiCore sound pack and bind it to the game's semantic audio events.

Everything the game plays today is synthesised placeholder from GeneratePhase42Audio.py - one
tone per event, no variation. This replaces the events where a recorded pack is plainly better
and where repetition is most audible:

  Combat.Hurt / Combat.Death   per-archetype creature voices, organic vs mechanical
  Weapon.M91.Fire              the laser bank instead of a synthesised click
  Player.Footstep              ten metal steps instead of one
  UI.Objective / .Pickup / .Dialogue   real UI stingers

Each event gets a SoundCue with a SoundNodeRandom over its variations, so nothing repeats
back-to-back, routed through the same attenuation, concurrency and submix assets the existing
cues use - a cue that skips that routing plays at full volume through the master bus and is
audible from anywhere in the level.

Source WAVs are copied into Content/Ashes/Audio/Raw/SciFi and committed, matching the existing
Raw folder where every .wav sits next to the .uasset built from it. AH_AUDIO_SOURCE overrides
where they are copied from.
"""

import os
import shutil

import unreal


SOURCE_ROOT = os.environ.get(
    "AH_AUDIO_SOURCE", "/Volumes/Backup/Unreal Projects/SciFiCoreSounds")
RAW_DIR = "/Game/Ashes/Audio/Raw/SciFi"
CUE_DIR = "/Game/Ashes/Audio/Cues"
RAW_DISK = os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()),
    "Ashes", "Audio", "Raw", "SciFi")

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

# name -> (relative source dir, filename prefixes). Split rather than lumped: a creature that
# uses the same bank for "hit" and "died" gives the player no way to tell whether it is still
# coming.
BANKS = {
    "AlienHurt": ("Creatures/Organic", ["AlienChitter_01", "AlienChitter_02", "AlienChitter_03",
                                        "AlienChitter_04", "AlienChitter_05"]),
    "AlienDeath": ("Creatures/Organic", ["AlienChitter_06", "AlienChitter_07", "AlienChitter_08",
                                         "AlienChitter_09", "Alien_Chitter_10"]),
    "RoboHurt": ("Creatures/Robots", ["Robo_01", "Robo_02", "Robo_03", "Robo_04", "Robo_05"]),
    "RoboDeath": ("Creatures/Robots", ["Robo_06", "Robo_07", "Robo_08", "Robo_09", "Robo_10",
                                       "Robo_11"]),
    "Lazer": ("Lazers", ["Lazer_01", "Lazer_02", "Lazer_03", "Lazer_04", "Lazer_05",
                         "Lazer_06", "Lazer_07", "Lazer_08", "Lazer_09", "Lazer_10"]),
    "LazerHeavy": ("Lazers", ["Lazer_Heavy_01", "Lazer_Heavy_02", "Lazer_Heavy_03",
                              "Lazer_Heavy_04"]),
    "MetalStep": ("Loops & Stingers/Footsteps/Metal_Steps",
                  ["Metal_Footstep_%02d" % n for n in range(1, 11)]),
    "UISelect": ("UI", ["UISelect", "UISelect_02"]),
    "UIPositive": ("UI", ["UI_Positive_01"]),
    "UINegative": ("UI", ["UI_Negative_01"]),
}

# Which bank drives which semantic event in DA_AudioPalette_Default. Combat.Hurt/Death stay on
# the existing synthesised cues: those are the PLAYER's, and a human taking a hit should not
# chitter. Creature voices are bound per archetype in AuthorEnemyDefinitions.py instead.
# Weapon.M91.Fire and UI.Pickup are NOT here: the M91 is a bolt-action rifle, not a laser, and
# a picked-up magazine is foley, not a menu blip. Both belong to recorded banks bound by
# Scripts/AuthorSoundLibraryCues.py, and re-running this script must not take them back.
PALETTE_EVENTS = {
    "Player.Footstep": "SC_SciFi_MetalStep",
    "UI.Objective": "SC_SciFi_UIPositive",
    # UISelect, not UINegative: UINegative is the "denied" stinger and made every line of
    # dialogue sound like a rejected input.
    "UI.Dialogue": "SC_SciFi_UISelect",
}

UI_BANKS = {"UISelect", "UIPositive", "UINegative"}


def _log(message):
    unreal.log_warning("[SciFiAudio] " + message)


def _load(path):
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None


def _require(path):
    asset = _load(path)
    if not asset:
        raise RuntimeError("required audio asset missing: " + path)
    return asset


def copy_and_import(bank, relative_dir, stems):
    """Copy each WAV next to where its .uasset will live, then import it.

    The Raw folder already works this way - SC_Combat_Hurt.wav sits beside SC_Combat_Hurt.uasset -
    so a reimport never needs the original drive.
    """
    os.makedirs(RAW_DISK, exist_ok=True)
    waves = []
    for stem in stems:
        source = os.path.join(SOURCE_ROOT, relative_dir, stem + ".wav")
        if not os.path.isfile(source):
            raise RuntimeError("source wav missing: " + source)
        asset_name = "SW_SciFi_" + stem.replace("-", "_")
        local = os.path.join(RAW_DISK, asset_name + ".wav")
        shutil.copyfile(source, local)

        task = unreal.AssetImportTask()
        task.set_editor_property("filename", local)
        task.set_editor_property("destination_path", RAW_DIR)
        task.set_editor_property("destination_name", asset_name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        TOOLS.import_asset_tasks([task])

        full = "%s/%s" % (RAW_DIR, asset_name)
        wave = _load(full)
        if not wave:
            raise RuntimeError("wav import failed: " + full)
        # One-shots stay resident rather than streaming: they are short, they fire on a frame the
        # player is already reacting to, and a streamed miss is a silent hit.
        wave.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
        unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
        waves.append(wave)
    _log("%s imported %d wave(s)" % (bank, len(waves)))
    return waves


def author_random_cue(bank, waves):
    """One cue per bank, randomising over its variations.

    SoundNodeRandom with bRandomWithoutReplacement is what stops the same chitter twice running,
    which is the thing that makes a pack of creatures sound like one creature.
    """
    is_ui = bank in UI_BANKS
    attenuation = _require("/Game/Ashes/Audio/Mix/" + ("ATT_UI" if is_ui else "ATT_World3D"))
    concurrency = _require("/Game/Ashes/Audio/Mix/" + ("CONC_UI" if is_ui else "CONC_World"))
    submix = _require("/Game/Ashes/Audio/Submixes/" + ("SM_UI" if is_ui else "SM_Weapons"))

    name = "SC_SciFi_" + bank
    path = "%s/%s" % (CUE_DIR, name)
    cue = _load(path)
    if cue and cue.get_class().get_name() != "SoundCue":
        unreal.EditorAssetLibrary.delete_asset(path)
        cue = None
    if not cue:
        cue = TOOLS.create_asset(name, CUE_DIR, unreal.SoundCue, unreal.SoundCueFactoryNew())
    if not cue:
        raise RuntimeError("could not create cue " + path)

    players = []
    for wave in waves:
        player = unreal.new_object(unreal.SoundNodeWavePlayer, cue)
        player.set_editor_property("sound_wave_asset_ptr", wave)
        players.append(player)

    if len(players) > 1:
        top = unreal.new_object(unreal.SoundNodeRandom, cue)
        top.set_editor_property("child_nodes", players)
        # Already the engine default, set anyway: without it a random node can pick the same
        # variation twice running, which is exactly the repetition these banks exist to remove.
        top.set_editor_property("randomize_without_replacement", True)
        top.set_editor_property("weights", [1.0] * len(players))
    else:
        top = players[0]

    attenuated = unreal.new_object(unreal.SoundNodeAttenuation, cue)
    attenuated.set_editor_property("child_nodes", [top])
    attenuated.set_editor_property("attenuation_settings", attenuation)

    cue.set_editor_property("first_node", attenuated)
    cue.set_editor_property("override_concurrency", False)
    cue.set_editor_property("concurrency_set", {concurrency})
    cue.set_editor_property("sound_submix_object", submix)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)

    # Read back: a cue whose graph failed to attach plays nothing and logs nothing.
    written = _load(path)
    if not written.get_editor_property("first_node"):
        raise RuntimeError("%s has no first_node; its graph did not attach" % name)
    _log("%s cue over %d variation(s)" % (name, len(players)))
    return path


def bind_palette(cue_paths):
    """Point the default palette's semantic events at the new cues."""
    palette_path = "/Game/Ashes/Audio/DA_AudioPalette_Default"
    palette = _require(palette_path)
    events = palette.get_editor_property("events")
    for event_name, cue_name in PALETTE_EVENTS.items():
        cue = _require(cue_paths[cue_name])
        events[unreal.Name(event_name)] = cue
    palette.set_editor_property("events", events)
    unreal.EditorAssetLibrary.save_asset(palette_path, only_if_is_dirty=False)

    written = _load(palette_path).get_editor_property("events")
    for event_name, cue_name in PALETTE_EVENTS.items():
        bound = written[unreal.Name(event_name)]
        if not bound or cue_name not in str(bound):
            raise RuntimeError("palette event %s did not bind to %s" % (event_name, cue_name))
    _log("bound %d palette events" % len(PALETTE_EVENTS))


def main():
    if not os.path.isdir(SOURCE_ROOT):
        raise RuntimeError("sound source folder not found: " + SOURCE_ROOT)
    unreal.EditorAssetLibrary.make_directory(RAW_DIR)

    cue_paths = {}
    for bank, (relative_dir, stems) in BANKS.items():
        waves = copy_and_import(bank, relative_dir, stems)
        path = author_random_cue(bank, waves)
        cue_paths["SC_SciFi_" + bank] = path

    bind_palette(cue_paths)
    unreal.EditorAssetLibrary.save_directory(RAW_DIR, only_if_is_dirty=False, recursive=True)
    _log("authored %d cues from %s" % (len(cue_paths), SOURCE_ROOT))


if __name__ == "__main__":
    main()
