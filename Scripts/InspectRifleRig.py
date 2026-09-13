"""Read component-space attachment measurements from the live editor."""
import unreal

ape = unreal.AnimPoseExtensions
mesh = unreal.load_asset("/Game/Ashes/Enemies/Stalker/SKM_Stalker")
pose = ape.get_reference_pose(mesh.get_editor_property("skeleton"))
clip = unreal.load_asset("/Game/Ashes/Enemies/Stalker/AS_Stalker_Run")
unreal.log_warning("TRACK_NAMES " + repr(clip.get_editor_property("data_model_interface").get_bone_track_names()))
for bone in ("Bip01-Spine3", "Bip01-Head", "Bip01-R-UpperArm", "Bip01-R-Forearm", "Bip01-R-Hand",
             "Bip01-L-UpperArm", "Bip01-L-Forearm", "Bip01-L-Hand"):
    t = ape.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
    unreal.log_warning("RIFLE_RIG " + bone + " " + str(t))
weapon = unreal.SkeletalMeshComponent()
weapon.set_skeletal_mesh_asset(unreal.load_asset("/Game/Weapons/Rifle/Meshes/SKM_Rifle"))
for name in ("GripPoint", "GripPoint_002", "Muzzle"):
    unreal.log_warning("RIFLE_RIG " + name + " " + str(weapon.get_socket_transform(name, unreal.RelativeTransformSpace.RTS_COMPONENT)))
