"""Reimport the one-shot source WAVs over the SoundWave assets the cues actually play.

The bookend to ReimportAmbience.py.  GeneratePhase42Assets.py skips any WAV that already has an
asset, so regenerating a one-shot on disk never reaches the game - the exact silent no-op that
left the beds stale for weeks.  Every one-shot SoundCue (Cues/, Weapons/M91/, UI/) references the
SoundWave in /Game/Ashes/Audio/Raw, so replacing the Raw wave is enough here: unlike the beds,
there is no second copy.  Asset paths, cue graphs, palette entries and semantic names are
untouched.

Reimporting all of them every time is deliberate and idempotent: a regenerated file that happens
to be byte-identical costs a reimport and nothing else, and no one has to remember which sources
changed.
"""

import os

import unreal

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

# The four 60s beds are ReimportAmbience.py's job - they have a second copy under Environment/
# and their own duration and stereo rules.
BEDS = {
    "SC_Erebus_Ambience",
    "SC_Transit_Ambience",
    "SC_Cathedral_Ambience",
    "SC_Manticore_Engine",
}

DESTINATION = "/Game/Ashes/Audio/Raw"
MAXIMUM_ONE_SHOT_SECONDS = 3.0

RAW_DIRECTORY = os.path.join(unreal.Paths.project_content_dir(), "Ashes", "Audio", "Raw")

names = sorted(
    os.path.splitext(entry)[0]
    for entry in os.listdir(RAW_DIRECTORY)
    if entry.lower().endswith(".wav") and os.path.splitext(entry)[0] not in BEDS
)

tasks = []
for source_name in names:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(RAW_DIRECTORY, source_name + ".wav"))
    task.set_editor_property("destination_path", DESTINATION)
    task.set_editor_property("destination_name", source_name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    tasks.append(task)

if not tasks:
    unreal.log_error("[Audio] no one-shot WAVs found in " + RAW_DIRECTORY)

TOOLS.import_asset_tasks(tasks)

failures = 0
for source_name in names:
    asset_path = DESTINATION + "/" + source_name
    wave = unreal.load_asset(asset_path)
    if not wave:
        unreal.log_error("[Audio] reimport produced no asset for " + asset_path)
        failures += 1
        continue

    channels = wave.get_editor_property("num_channels")
    duration = wave.get_editor_property("duration")
    # One-shots are spatialized through project attenuation, and a stereo source cannot be panned
    # in 3D - the engine will collapse or ignore one side. Mono is the requirement, not a default.
    if channels != 1:
        unreal.log_error("[Audio] %s is %dch; one-shots must be mono to spatialize" % (asset_path, channels))
        failures += 1
    if duration > MAXIMUM_ONE_SHOT_SECONDS:
        unreal.log_error("[Audio] %s is %.2fs; that is a bed, not a one-shot" % (asset_path, duration))
        failures += 1
    unreal.log_warning("[Audio] OK %s duration=%.2fs %dch@%dHz" % (
        asset_path, duration, channels, wave.get_editor_property("sample_rate")))

if failures:
    unreal.log_error("[Audio] %d one-shot(s) failed reimport" % failures)
