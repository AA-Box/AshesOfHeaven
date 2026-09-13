"""Read native Hound limb chains from the authored idle pose."""
import unreal

ape = unreal.AnimPoseExtensions
mesh = unreal.load_asset("/Game/Ashes/Enemies/Hound/SKM_Hound")
component = unreal.SkeletalMeshComponent()
component.set_skeletal_mesh_asset(mesh)
clip = unreal.load_asset("/Game/Ashes/Enemies/Hound/SKM_HoundAlien-Animal_1_5_01_Idle_Aggressive")
pose = ape.get_anim_pose_at_time(clip, 0.0, unreal.AnimPoseEvaluationOptions())
for bone in ape.get_bone_names(pose):
    name = str(bone)
    if name.startswith(("Bone_005_L", "Bone_009_L", "Bone_009_R", "Bone_005_R")):
        point = ape.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation * (115.0 / 2013.4168396)
        unreal.log_warning("HOUND_RIG %s parent=%s xyz=%s" % (name, component.get_parent_bone(name), point))
