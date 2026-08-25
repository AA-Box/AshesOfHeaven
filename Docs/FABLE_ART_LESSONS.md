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
- Textures imported with TC_Masks compression need SAMPLERTYPE_MASKS on their
  sample nodes; SAMPLERTYPE_LINEAR_COLOR fails the whole material compile and
  every mesh silently falls back to the checkerboard default.
- Budget silhouettes by angular height before placing: an object whose
  height/distance exceeds tan(vertical half-FOV) seals the sky from the player
  camera. The 26m banner monoliths at 30m read as canyon walls, not landmarks;
  the reference's blocks work because they sit at 60m+.
- The MCP Scene/Actor toolsets operate on the PIE world while PIE runs:
  find_actors returns UEDPIE actors and set_actor_transform moves them live
  (remove_from_scene is blocked). Teleporting suspects to -100000 Z one at a
  time is the fastest way to identify an unknown mass in the frame.
- The reference's tonal hierarchy (bright sky, dark ground, silhouetted
  structures) is an exposure-anchoring problem, not an albedo problem. Bright
  fog inscattering + a visible bright sky give auto-exposure something to key
  on so everything terrestrial falls dark; ambient point fills over the street
  light the FLOOR first (inverse square) and invert the hierarchy — delete
  them and light walls with a shadowless cross-street key instead. Albedo cuts
  cannot out-run AE: after adaptation only relative light distribution moves
  the wall:ground ratio.
- Do the angular-size math against the REAL chapter scale before placing any
  landmark: the chapter is a ~350m diorama (1uu = 1cm), so "8.2km down the
  corridor" is 82 METERS. A 200m-tall Cathedral tower at 82m is a wall that
  seals the sky; the same tower reads as the reference's haze-veiled landmark
  at 250-350m (X 24000-34000 anchor-local) with ground-hugging fog.
- The full reference tonal recipe that finally inverted the value hierarchy:
  sun intensity ~32 at pitch -15 / yaw -150 (backlit, shadows ON) driving a
  bright SkyAtmosphere (RayleighScale 2.2, multiscatter 4) and a storm-cloud
  material instance (engine m_SimpleVolumetricCloud_Inst with Cloud_GlobalCoverage
  0.35+, StormClouds 1), plus GROUND-HUGGING fog (density 0.022, height falloff
  0.30, bright inscattering 0.55) and AE bias -0.7. SkyLight intensity is the
  ground-brightness knob (0.55-0.75); wetness above ~0.3 on horizontal ground
  bounces the bright sky and lifts the floor right back up.
- Ring-scatter copings (RuinEdge on RuinBlock tops) must be seated on the
  block's ACTUAL randomized height; a fixed-height guess leaves 25m dark beams
  floating against the bright sky, which read as major artifacts in every frame.
- Freshly LOADED authored Niagara systems can render nothing in editor
  SIE/PIE sessions until AuthorErebusNearVFX recompiles them in-session; run
  the author step before starting the session when judging VFX in-editor, and
  treat the packaged build (cook compiles systems) as the only acceptance
  surface for particles.
- Fire recipe geometry is legibility: flame sprites need >=16-38uu to read
  past 5m; a tight cone/small shape radius stacks sprites into a flame tornado
  next to the camera, while cone ~35 deg + shape radius ~40 reads as a burning
  pool. Ember jets over ~120uu/s velocity stack into a sourceless vertical
  string above every fire.
- The Python remote-execution plugin is the fastest art-iteration control
  plane on this Mac, but multicast is dead: drive the protocol with UNICAST
  UDP to 127.0.0.1:6766 and omit `dest` (empty dest passes the receive
  filter), listening on TCP 6776 for the editor's connect-back. A crashed
  editor's CrashReportClient inherits BOTH the MCP port 8000 AND UDP 6766 —
  pkill it before every relaunch, and approve the macOS firewall prompt for
  the listening python binary once.
- A sprite whose look does not change over its own life cannot read as fire or
  smoke, no matter how the emitter is tuned. Drive the sprite master from
  `ParticleRelativeTime`: an erosion threshold that climbs with age (the puff
  burns away from its edges), a falloff radius that shrinks for flame and grows
  for smoke (size-over-life without adding a Niagara module), a colour ramp, and
  an alpha fade-in/out. Erosion must FADE IN with age too — eroding from birth
  gives a swarm of identical flakes instead of a flame with a dense core.
- Fire density is spawn rate, not sprite size. 70 sprites/s against half-second
  lives leaves ~35 particles spread over 1.5m, which reads as orange popcorn with
  gaps; ~260/s is where the licks join into one body.
- One isotropic fbm can only make round puffs. Bake purpose-built channels
  instead (`T_AH_VFXNoise`: R = vertically stretched flame noise, G = two-scale
  Worley billows for smoke, B = fine breakup) and squash the sprite's distance
  metric in X so each flame sprite is a vertical lick rather than a disc.
- Unlit smoke is a flat cutout. Smoke sprites want DefaultLit translucent with
  `TLM_VolumetricNonDirectional` so the plume takes the fires and the sky — but
  its ALBEDO has to stay low (~0.08 soot to ~0.26 ash). Under this scene's bright
  inscattering a 0.5 albedo turns a distant column into a white egg brighter than
  the sky behind it.
- Overlapping translucent sprites accumulate alpha: 40 spawns/s of 2000uu smoke
  over a 13s life is 520 stacked quads on one column and reads as a solid mass.
  Plumes need particle depth (fewer, longer-lived, lower alpha), not opacity.
- `DepthFade` on every sprite master. The razor line where a billboard
  intersects the ground or a wreck is the loudest "these are particles" tell,
  and it costs one node.
- `MaterialEditingLibrary.connect_material_expressions` matches the DESTINATION
  pin by name; single-input nodes (`Saturate`, `OneMinus`) expose an UNNAMED
  input, so pass `""`, not `"Input"`. Math nodes' `ConstA`/`ConstB`/
  `ConstExponent`/`ConstAlpha` defaults remove most constant nodes from an
  authored graph.
- Judging VFX in MOTION needs a capture harness that is itself verified, and this
  one is not. Three separate artifacts were each mistaken for a game defect before
  being caught: (1) `screencapture -v` imposes its own ~1s encoding cadence, so
  autocorrelating any region's brightness reports a confident loop — a static HUD
  region scores r=0.56 and an empty skyline r=0.71 at the SAME lag in the SAME
  recording; (2) the packaged window renders a BLACK 3D scene whenever it is not
  frontmost while Slate keeps drawing the HUD, so a lost-focus frame looks exactly
  like the effect vanishing and a HUD-based sanity check cannot see it by
  construction; (3) a fixed screen-space ROI is meaningless unless the frame content
  is proven stable first. Never report a motion number without a control region that
  the effect does not touch, and treat the EXCESS over that control as the only
  signal. `Scripts/../scratchpad/motion_metric.py` does this; its docstring carries
  the reasoning.
- Invariance is evidence. A measured "defect" that does not move when you change
  four different parameters that should all affect it is almost certainly in your
  instrument, not in the game. Fire "dropout" held at 25.0 / 25.3 / 25.7 / 24.1%
  across four builds with different determinism, pan speeds, loop durations and
  distance-cull settings; that flatness was the tell, not the number.
- Consecutive-frame contact sheets are the cheap, trustworthy motion check: five
  native frames ~27ms apart show whether an effect EVOLVES or merely translates, and
  whether particles pop, with no statistics to get wrong.
- Fountain exposes far more rapid-iteration parameters than the recipe table reads.
  `InitializeParticle.Sprite Rotation Angle Min`/`Max`, `EmitterState.Loop Duration`,
  `EmitterState.MinDistance`/`MaxDistance` are all settable and were all being left
  at template defaults. Dump the parameter list before concluding something needs a
  module added.
