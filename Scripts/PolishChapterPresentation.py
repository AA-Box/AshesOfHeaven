"""Calibrate saved Chapter One practical lights without rebuilding geometry.

Run in the editor, outside PIE, after generating any presentation zone. Existing
maps must be saved and backed up first. The pass is idempotent and retains actors,
collision, materials, navigation, and streaming anchors.
"""
import json
from pathlib import Path

import unreal


LEVELS = (
    "/Game/Ashes/Environment/Erebus/L_ErebusOpening_Presentation",
    "/Game/Ashes/Environment/Transit/L_Transit_Presentation",
    "/Game/Ashes/Environment/Cathedral/L_Cathedral_Presentation",
)


def run():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor.get_game_world():
        raise RuntimeError("Stop PIE before authoring presentation lights.")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    original = levels.get_current_level().get_outer().get_path_name().split(".")[0]
    report = []
    for path in LEVELS:
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            raise RuntimeError("Missing presentation level: " + path)
    try:
        for path in LEVELS:
            if not levels.load_level(path):
                raise RuntimeError("Cannot open " + path)
            count = 0
            for actor in actors.get_all_level_actors():
                if not isinstance(actor, unreal.PointLight):
                    continue
                light = actor.get_component_by_class(unreal.PointLightComponent)
                label = actor.get_actor_label()
                fire = any(word in label for word in ("Fire", "Smolder"))
                veil = any(word in label for word in ("Veil", "Axial"))
                color = (1.0, 0.32, 0.09) if fire else (0.63, 0.75, 0.85) if veil else (1.0, 0.67, 0.34)
                intensity = 100.0 if fire else 180.0 if veil else 120.0
                radius = 1500.0 if veil else 950.0
                if "Emergency" in label:
                    color, intensity, radius = (1.0, 0.10, 0.025), 40.0, 700.0
                # Static lights have no baked contribution in these dynamically streamed
                # levels. Set mobility before changing any rendering properties.
                light.set_mobility(unreal.ComponentMobility.MOVABLE)
                light.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
                light.set_intensity(intensity)
                light.set_light_color(unreal.LinearColor(*color, 1.0))
                light.set_attenuation_radius(radius)
                light.set_cast_shadows(False)
                light.set_editor_property("volumetric_scattering_intensity", 0.35)
                if light.get_editor_property("mobility") != unreal.ComponentMobility.MOVABLE:
                    raise RuntimeError("Light mobility did not apply: " + label)
                if abs(light.get_editor_property("intensity") - intensity) > 0.01:
                    raise RuntimeError("Light intensity did not apply: " + label)
                count += 1
            if count == 0:
                raise RuntimeError("No authored point lights in " + path)
            if not levels.save_current_level():
                raise RuntimeError("Cannot save " + path)
            report.append({"level": path, "calibrated_lights": count})
    finally:
        if not levels.load_level(original):
            raise RuntimeError("Cannot restore the original level: " + original)
    output = Path(unreal.Paths.project_saved_dir()) / "ProfessionalPass"
    output.mkdir(parents=True, exist_ok=True)
    (output / "lighting.json").write_text(json.dumps(report, indent=2))
    unreal.log("[ProfessionalPass] " + json.dumps(report))


run()
