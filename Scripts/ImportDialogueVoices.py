"""Import and verify the generated dialogue WAVs in the editor after generation.

Run with Scripts/RunInEditor.py. Saves first and after import; no PIE may be active.
"""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
source = root / "Content/Ashes/Audio/Dialogue"
manifest = json.loads((root / "Docs/DialogueVoices.json").read_text())
if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
    raise RuntimeError("Stop PIE before importing voice assets")
if not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True):
    raise RuntimeError("Could not save the current project")
submix = unreal.load_asset("/Game/Ashes/Audio/Submixes/SM_Dialogue")
if not submix:
    raise RuntimeError("Dialogue submix is missing")
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
for line in manifest["lines"]:
    filename = source / (line["asset_name"] + ".wav")
    if not filename.is_file():
        raise RuntimeError(f"Missing source recording: {filename}")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(filename))
    task.set_editor_property("destination_path", "/Game/Ashes/Audio/Dialogue")
    task.set_editor_property("destination_name", line["asset_name"])
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    asset_tools.import_asset_tasks([task])
    sound = unreal.load_asset(line["asset_path"])
    if not isinstance(sound, unreal.SoundWave):
        raise RuntimeError(f"Import failed: {line['asset_path']}")
    sound.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
    sound.set_editor_property("sound_submix_object", submix)
    sound.set_editor_property("looping", False)
    sound.set_editor_property("volume", 1.0)
    duration = sound.get_editor_property("duration")
    if abs(duration - line["duration"]) > 0.05:
        raise RuntimeError(f"Unexpected imported duration: {line['asset_path']}")
    for key, value in {"Speaker": line["speaker"], "SpokenText": line["text"],
                       "VoiceModel": line["model"], "VoiceId": line["voice"],
                       "Provenance": manifest["disclosure"]}.items():
        unreal.EditorAssetLibrary.set_metadata_tag(sound, key, value)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {line['asset_path']}")
unreal.log(f"[DialogueVoice] Imported and verified {len(manifest['lines'])} voiced lines for {len(manifest['cast'])} characters.")
