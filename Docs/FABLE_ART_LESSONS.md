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
- Niagara components saved inside a streamed sublevel do not render in packaged
  builds even when explicitly activated (verified: 19 components activated, zero
  visible). Spawn zone VFX at runtime (SpawnSystemAtLocation) from the director
  instead; keep authored levels mesh/light/decal only.
- `AssetTools.create_asset` returns None in unattended commandlets when the asset
  already exists ("CanCreateAsset cannot ask the user"), and delete+recreate of the
  same path inside one session corrupts references. Author assets load-or-create,
  never delete-first.
- AutoExposureMinBrightness is a floor on the adapted scene luminance, not a floor
  on brightness: a high min LOCKS a dark scene dark. Lower the min (0.05) and steer
  the read with exposure bias and light intensity instead.
- Packaged-app launches flake with a startup SIGSEGV inside LaunchServices/LLM
  bookkeeping (engine/OS race, not project code). Capture scripts must retry the
  launch and clear crash dialogs (`pkill -x UserNotificationCenter`) before judging
  a run failed.
- Test fixtures that build a standalone UGameInstance must call
  `GameInstance->Shutdown()` in teardown. Skipping it defers subsystem
  deinitialization to the GC-purge destructor, which asserts (UObjectArray.h
  `Index >= 0`) once neighbouring objects are freed first — deterministic crash
  that kills the whole automation queue after the test itself passes.
- A factory-fresh Niagara emitter exposes almost no rapid-iteration parameters,
  so it cannot be re-authored programmatically. Duplicate Epic's UI-built
  Fountain template emitter instead (LoadObject the template — unattended
  commandlets never registry-scan /Niagara) and overwrite its parameters.
  SpawnRate lives in the EMITTER update script, not the particle scripts.
- A NiagaraSystem embeds its own INHERITED COPY of each emitter, baked at
  creation. Editing the standalone NE_* asset afterwards never reaches the
  runtime: author the system's emitter handles
  (`System->GetEmitterHandle(i).GetInstance().GetEmitterData()`) directly.
- Opaque surface masters on sprites render as solid squares (the 'floating
  cube cloud' artifact). Sprite systems need unlit translucent/additive
  materials that read ParticleColor and fade by a radial gradient.
- PIE in the editor understates the packaged look (scalability can drop
  volumetric fog and atmosphere quality). Use PIE loops for composition and
  gross errors only; judge lighting/atmosphere from the packaged build.
- Under auto-exposure, skylight and point fills favour up-facing ground, so
  raising them never fixes black walls. Steer the wall:ground ratio with
  albedo (dark wet ground like the reference) and light direction; a second
  directional triggers an on-screen forward-shading warning in Development.
- Accidental cruciform silhouettes come from depth-composition, not single
  assets: a bare gate lintel plus ANY nearer vertical (pole, column, mast) on
  the camera axis reads as a giant cross. Fill gate centers with mass and keep
  crossarm poles off the corridor sightline.
