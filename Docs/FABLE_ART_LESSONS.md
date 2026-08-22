# FABLE ART LESSONS

Durable lessons only. Each entry prevents a repeated mistake.

- Machine tests cannot prove visual quality. Log-grep "PASS" and asset counts say an
  asset loads, never that it reads correctly on screen. Packaged human screenshots are
  the only visual evidence.
- Runtime primitive spawning (SpawnVisualShape) is acceptable for hidden gameplay
  collision and debug layers, never for approved art. A scaled cube reads as a scaled
  cube at every distance.
- Stage anchors are canonical; presentation consumes them. The authored Erebus zone is
  built in anchor-local space and streamed at the ErebusOpening anchor
  (ULevelStreamingDynamic + FlushLevelStreaming in the director), so authored placement
  never re-introduces absolute world constants.
- Geometry Script (UE 5.8) exposes its Python libraries as `unreal.GeometryScript_*`,
  not `GeometryScriptLibrary_*`. Probe the API on the installed engine before writing a
  generator; docstrings carry exact signatures.
- Pitch-90 primitive appends extrude toward -X from the transform base in this engine
  build. Never assume the sign: measure with a probe (as done for plane-cut side
  calibration) or check the report bbox before placement.
- `EditorActorSubsystem.spawn_actor_from_object` needs actor factories that are absent
  under `-nullrhi` commandlets and silently returns None. `spawn_actor_from_class` plus
  explicit component asset assignment works in every editor mode.
- Emissive materials on large flat shapes read as glowing monoliths in the war gloom
  (Transit gate sign, Niagara sprite plumes). Warm reads must come from lights on
  visible sources; emissive surfaces stay small.
- Regenerate assets in place (CopyMeshToStaticMesh) instead of delete+create so saved
  level references survive kit iteration.
- Capture evidence with a region that provably contains the full game window, and
  verify the viewmodel is in frame before drawing conclusions from its absence — a
  cropped capture region produced a false "M91 missing" finding.
