"""Shared helpers for authoring a Chapter One presentation zone as a saved level.

A zone level holds presentation only: every actor is non-colliding and excluded from
navigation, so shipping art for a section never changes how it plays. Coordinates are
ANCHOR-LOCAL - local (0,0,0) is the section's stage anchor, and the runtime director streams
the level in at that anchor (AAHChapterOneDirector::TryLoadAuthoredZone), so authored
placement never hard-codes world positions.

Regenerating a zone is destructive by design: the level is cleared and rebuilt each run.
"""

import json
import os

import unreal

MESH_DIR = "/Game/Ashes/Environment/Erebus/Meshes"
BP_DIR = "/Game/Ashes/Blueprints/Environment"
INSTANCE_DIR = "/Game/Ashes/Materials/Instances"

LEVELS = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


class Zone(object):
    """Accumulates one zone's actors and reports what could not be built."""

    def __init__(self, level_path, zone_tag):
        self.level_path = level_path
        self.zone_tag = zone_tag
        self.spawned = []
        self.missing = []

    # -- level lifecycle -------------------------------------------------------------
    def open_clean_level(self):
        if unreal.EditorAssetLibrary.does_asset_exist(self.level_path):
            if not LEVELS.load_level(self.level_path):
                raise RuntimeError("Could not open " + self.level_path)
            for actor in ACTORS.get_all_level_actors():
                if isinstance(actor, (unreal.WorldSettings, unreal.Brush)):
                    continue
                ACTORS.destroy_actor(actor)
        else:
            unreal.EditorAssetLibrary.make_directory(self.level_path.rsplit("/", 1)[0])
            if not LEVELS.new_level(self.level_path):
                raise RuntimeError("Could not create " + self.level_path)

    def save(self):
        if not LEVELS.save_current_level():
            raise RuntimeError("Could not save " + self.level_path)
        summary = {
            "zone": self.zone_tag,
            "level": self.level_path,
            "actors": len(self.spawned),
            "missing": sorted(set(self.missing)),
        }
        unreal.log("[%s] authored %d actors (%d missing assets: %s)" % (
            self.zone_tag, len(self.spawned), len(self.missing),
            ", ".join(summary["missing"]) if self.missing else "none"))
        # unreal.log from a commandlet is not reliably greppable from stdout, so the result
        # is also written to a file the caller can read. Absent report == the script did not
        # reach the end, which is a different failure from "built nothing".
        report_dir = os.path.join(unreal.Paths.project_saved_dir(), "ZoneAuthoring")
        try:
            os.makedirs(report_dir)
        except OSError:
            pass
        with open(os.path.join(report_dir, self.zone_tag + ".json"), "w") as handle:
            json.dump(summary, handle, indent=2)
        if self.missing:
            unreal.log_error("[%s] missing assets prevented a complete zone build" % self.zone_tag)
        return len(self.spawned)

    # -- placement -------------------------------------------------------------------
    def tag(self, actor, extra=None):
        tags = [unreal.Name("Phase4Presentation"), unreal.Name("AH.AuthoredZone"),
                unreal.Name("AH.Zone." + self.zone_tag)]
        if extra:
            tags.append(unreal.Name(extra))
        actor.tags = tags

    def mesh(self, name, loc, rotation=(0, 0, 0), scale=(1, 1, 1), label=None):
        asset = unreal.load_asset(MESH_DIR + "/" + name)
        if not asset:
            self.missing.append(name)
            return None
        # spawn_actor_from_object needs actor factories that are absent under -nullrhi;
        # class-spawn plus an explicit component assignment works in every editor mode.
        actor = ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), rot(*rotation))
        if not actor:
            self.missing.append(name + " (spawn)")
            return None
        actor.set_actor_scale3d(unreal.Vector(*scale))
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component:
            component.set_editor_property("static_mesh", asset)
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            component.set_editor_property("can_ever_affect_navigation", False)
        self.tag(actor)
        actor.set_actor_label(label or name)
        self.spawned.append(actor)
        return actor

    def row(self, name, start, step, count, rotation=(0, 0, 0), scale=(1, 1, 1), label=None):
        """Repeats one module along a straight line. Returns the actors placed."""
        placed = []
        for index in range(count):
            loc = (start[0] + step[0] * index, start[1] + step[1] * index, start[2] + step[2] * index)
            actor = self.mesh(name, loc, rotation, scale,
                              "%s_%02d" % (label or name, index))
            if actor:
                placed.append(actor)
        return placed

    def prop(self, bp_name, loc, rotation=(0, 0, 0), scale=(1, 1, 1), label=None):
        bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_DIR + "/" + bp_name)
        if not bp_class:
            self.missing.append(bp_name)
            return None
        actor = ACTORS.spawn_actor_from_class(bp_class, unreal.Vector(*loc), rot(*rotation))
        if not actor:
            self.missing.append(bp_name + " (spawn)")
            return None
        actor.set_actor_scale3d(unreal.Vector(*scale))
        self.tag(actor, "Phase4RuntimeProp")
        actor.set_actor_label(label or bp_name)
        self.spawned.append(actor)
        return actor

    def light(self, loc, color, intensity, radius, label="ZoneLight"):
        actor = ACTORS.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*loc), rot())
        if not actor:
            return None
        component = actor.get_component_by_class(unreal.PointLightComponent)
        if component:
            component.set_mobility(unreal.ComponentMobility.STATIC)
            component.set_intensity(intensity)
            component.set_light_color(unreal.LinearColor(color[0], color[1], color[2], 1.0))
            component.set_attenuation_radius(radius)
            # Presentation lights never cast shadows: the section's readability comes from
            # the authored key light, and shadowed fills pool light on the play surface.
            component.set_cast_shadows(False)
        self.tag(actor)
        actor.set_actor_label(label)
        self.spawned.append(actor)
        return actor

    def decal(self, material_name, loc, rotation=(0, -90, 0), size=(64, 128, 128), label="ZoneDecal"):
        material = unreal.load_asset(INSTANCE_DIR + "/" + material_name)
        if not material:
            self.missing.append(material_name)
            return None
        actor = ACTORS.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(*loc), rot(*rotation))
        if not actor:
            return None
        component = actor.get_component_by_class(unreal.DecalComponent)
        if component:
            component.set_decal_material(material)
            component.set_editor_property("decal_size", unreal.Vector(*size))
        self.tag(actor)
        actor.set_actor_label(label)
        self.spawned.append(actor)
        return actor


def rot(roll=0.0, pitch=0.0, yaw=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)
