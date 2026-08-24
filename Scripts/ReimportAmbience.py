"""Reimport the regenerated ambience beds over the SoundWave assets the game actually plays.

GeneratePhase42Assets.py deliberately skips any WAV that already has an asset, so a
regenerated source on disk never reaches the game.  This is the reimport half of that
pipeline.  Each bed exists twice: once in Raw as the imported source of record, and once in
Environment as the asset the audio palette binds to a semantic environment id - both are
SoundWaves with their own copy of the audio, so both have to be replaced or the palette keeps
serving the old bed.  Asset paths, palette entries and semantic event names are untouched.
"""

import os

import unreal

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

AMBIENCE = (
    "SC_Erebus_Ambience",
    "SC_Transit_Ambience",
    "SC_Cathedral_Ambience",
    "SC_Manticore_Engine",
)

DESTINATIONS = ("/Game/Ashes/Audio/Raw", "/Game/Ashes/Audio/Environment")

MINIMUM_BED_SECONDS = 55.0

# Resolved from the project, not from __file__: the commandlet is invoked through a
# path-safe alias because the project directory name contains spaces.
RAW_DIRECTORY = os.path.join(unreal.Paths.project_content_dir(), "Ashes", "Audio", "Raw")

tasks = []
for source_name in AMBIENCE:
    source_file = os.path.join(RAW_DIRECTORY, source_name + ".wav")
    if not os.path.isfile(source_file):
        unreal.log_error("[Audio] missing WAV on disk for " + source_name)
        continue
    for destination in DESTINATIONS:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source_file)
        task.set_editor_property("destination_path", destination)
        task.set_editor_property("destination_name", source_name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        tasks.append(task)

TOOLS.import_asset_tasks(tasks)

failures = 0
for destination in DESTINATIONS:
    for source_name in AMBIENCE:
        asset_path = destination + "/" + source_name
        wave = unreal.load_asset(asset_path)
        if not wave:
            unreal.log_error("[Audio] reimport produced no asset for " + asset_path)
            failures += 1
            continue
        # A bed played on repeat has to be marked as a looping source, or the streaming path
        # treats it as a one-shot and the seam gets a gap no crossfade can close.
        wave.set_editor_property("looping", True)
        # A minute of real stereo recording is megabytes of decoded audio to keep resident for a
        # bed that plays once per stage. Stream it instead - this is the property that decides
        # whether a designer's replacement costs memory or not, and it does not follow the WAV.
        wave.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.LOAD_ON_DEMAND)
        unreal.EditorAssetLibrary.save_asset(asset_path)
        duration = wave.get_editor_property("duration")
        # Channels and rate come from whatever WAV was on disk, so print them: a designer
        # swapping in a real recording finds out here that it landed as mono or at 44.1k.
        try:
            fmt = "%dch@%dHz" % (wave.get_editor_property("num_channels"), wave.get_editor_property("sample_rate"))
        except Exception:
            fmt = "format unavailable"
        if duration < MINIMUM_BED_SECONDS:
            unreal.log_error("[Audio] %s is only %.2fs; the loop will still be audible" % (asset_path, duration))
            failures += 1
        else:
            unreal.log_warning("[Audio] OK %s duration=%.2fs %s looping=%s streaming=LoadOnDemand" % (asset_path, duration, fmt, wave.get_editor_property("looping")))

if failures:
    unreal.log_error("[Audio] %d ambience bed(s) failed reimport" % failures)
