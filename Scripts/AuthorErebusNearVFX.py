import unreal

"""Author the near-camera Erebus Niagara systems (Phase 4.6 visual gate).

The heavy lifting lives in C++ (UAHPresentationAuthoringLibrary::AuthorErebusNearVFX):
fresh NE_/NS_Erebus_* assets with deliberate sprite size, spawn rate, lifetime and
velocity module values instead of the factory fountain defaults.
"""

if not unreal.AHPresentationAuthoringLibrary.author_erebus_near_vfx():
    unreal.log_error("[NearVFX] near-camera Niagara authoring reported failure")
else:
    unreal.log("[NearVFX] near-camera Niagara systems authored")
