# Level One — FOR A WHILE

`FOR A WHILE` is the complete first campaign level of *Ashes of Heaven*. It begins on Erebus during the opening attack and ends when Lucian authorizes the planetary failsafe, escapes the Cathedral, watches Erebus burn, and receives the impossible one-word Nysa transmission: `Lucian.`

The old `TenYearsLater` / `MayaScene` / `NysaTransmission` / `FleetDeparture` / `StarsDisappearing` enum values are retained only for save compatibility and later campaign migration. They are no longer Level One objectives.

## Runtime progression

1. `OpeningBlack` — black-screen cold open: CHILD `Did we win?` / LUCIAN `For a while.`
2. `ErebusOpening` — Maya rejoins Lucian; Sael orders Mourner Actual to the District Nine defensive line.
3. `OpeningBattle` — hold the line against the first Veil assault.
4. `TransitStation` — abandoned civilian transit station, broken evacuation announcements, Ivo radio banter.
5. `VeilRevelation` — a civilian says `It remembers us`; Maya realizes the Veil are converting people rather than simply invading.
6. `OpenBattlefield` — battlefield scale reveal and route toward the Manticore.
7. `ManticoreSection` — Ivo delivers Manticore Four-Seven and the Cathedral becomes the destination landmark.
8. `CathedralApproach` — the vehicle fails as the Cathedral responds to it.
9. `FailsafeOrder` — 08:42 until Erebus establishes an outbound carrier; Sael explains the signal and orders Planetary Failsafe.
10. `CathedralInterior` / `SaelTransmission` — the impossible duplicate Lucian appears and tells Lucian he has already seen him, just not yet.
11. `FailsafeTerminal` — casualty count `11,407,231`; Maya opposes the decision; Sael explains the interstellar risk; the player inspects and confirms the terminal.
12. `Escape` / `OtherLucian` — escape encounter, second silent Other Lucian sighting, Maya pulls Lucian forward.
13. `ErebusDestruction` — Erebus is destroyed, Sael reports containment, Nysa transmits `Lucian.`
14. `ChapterComplete` — Level One ends. No ten-years-later epilogue is entered from this level.

## Characters used by Level One

- **Lucian Vale** — playable character; restrained, competent, and increasingly burdened by the failsafe decision.
- **Maya Serrin** — squadmate / combat engineer / intelligence officer. Her dialogue is present from the opening route through the terminal and escape.
- **Admiral Sael Varek** — defense-fleet commander on radio. He is the source of the containment and failsafe order.
- **Ivo Ren** — Manticore pilot on radio, providing limited warmth and military banter before the Cathedral sequence.
- **Other Lucian** — impossible duplicate encountered inside the Cathedral and glimpsed again during escape.
- **Nysa** — unknown final transmission. Only one word is revealed in Level One.
- **Veil Pilgrim / Warden** — existing combat classes remain the core Level One enemies. The encounter architecture remains unchanged by this narrative pass.

## Environment contract

The existing authored zones remain authoritative:

- Erebus battlefield presentation level and authored Erebus mesh kit.
- Transit Station presentation layer.
- Open Battlefield / Manticore route.
- Cathedral Approach and Cathedral Interior presentation layer.
- Cathedral Escape route.

This change deliberately does **not** reintroduce visible greybox geometry or create a second environment pipeline.

## Unreal Engine material-only rule

All Level One environment and character-surface presentation must use Unreal Engine material assets (`UMaterialInterface`, Material, Material Instance, Material Instance Dynamic) already authored/cooked under `/Game/Ashes/...` or new Unreal-authored material assets created later.

Representative required material families are validated by automation:

- `MI_Concrete_Wet`
- `MI_HumanMetal_Dark`
- `MI_CathedralMatter_Dark`
- `MI_VeilObsidian_Black`
- `MI_EmissiveGlyph_Cyan`
- `MI_Erebus_BannerCloth`
- `MI_Erebus_BannerEmblem`
- `MI_Erebus_CathedralSilhouette`

Do not add a parallel runtime shader/material format, baked flat-color replacement, external renderer material system, or screenshot-derived surface treatment. Textures may feed Unreal materials, but the runtime surface authority is Unreal's material graph/material-instance pipeline.

## Narrative implementation

`AHLevelOneNarrative` owns the canonical dialogue text and stage-entry beats. `UAHDialogueSubsystem` resolves the legacy director sequence IDs through that canonical data and also starts one-shot stage-entry dialogue for stages that previously had no narrative sequence.

This avoids duplicating progression logic in a second director while letting the current Chapter One gameplay graph keep its tested triggers, encounters, checkpoints, Manticore, terminal, spatial recovery, HUD, and save integration.

Three rules keep that integration from failing silently:

- **Binding happens at `OnWorldBeginPlay`.** `UAHDialogueSubsystem` is a world subsystem and `UAHChapterSubsystem` lives on the game instance, which is not guaranteed to be attached when world subsystems initialize. `UWorld::BeginPlay` runs world-subsystem begin-play before any actor's `BeginPlay`, so the stage delegate is always bound before the director starts its first stage.
- **Stage-entry beats queue, they are never dropped.** A stage change that lands while a director sequence is talking is held in `PendingStageEntries` and played when the channel frees. A beat that is *already playing* when a director sequence preempts it is put back on the queue and resumes on the line it was cut off on, instead of being discarded unheard and never marked complete. The one-shot guard still prevents a replay.
- **A dialogue beat that finishes late never rewinds the chapter.** `AdvanceStageFromDialogue` only advances while the chapter is still on the stage that started the beat. `Ch01_Sael` used to call `StartStage(SaelTransmission)` unconditionally, so confirming the terminal during that beat dragged the chapter back out of `Escape` and every later trigger was rejected.
- **The destruction hold is derived, not hand-fitted.** `AHLevelOneNarrative::GetErebusDestructionHoldSeconds()` is the finale sequence's own length plus reading margin (7 s floor), and the director's `FinishDestructionSequence` timer uses it. If the finale started late because another sequence held the channel, `FinishDestructionSequence` waits for it (bounded at twice the hold) instead of completing the objective over the closing Nysa transmission.

## Objective completers

Every one of the 12 objectives needs something that can complete it, and
`AshesOfHeaven.LevelOne.ObjectiveCompleters` asserts it against the director's real spawned triggers.

The Cathedral doorway trigger `EnterCathedral` belongs to `FailsafeOrder`, not `CathedralApproach`. The director's `Tick` completes REACH THE CATHEDRAL APPROACH as soon as the player or the Manticore passes X=13700, several hundred units before the trigger box at X 14300-15100, so a trigger authored for `CathedralApproach` could only ever be overlapped once the chapter had already moved to `FailsafeOrder` — where `AAHChapterTrigger` rejects it. ACTIVATE PLANETARY FAILSAFE had no other completer, so the level could not be finished past the Cathedral ramp.

The stages completed by something other than a trigger are: `OpeningBattle` (encounter), `VeilRevelation` (dialogue completion), `ManticoreSection` (boarding), `CathedralApproach` (the X=13700 tick threshold), `FailsafeTerminal` (terminal confirmation) and `ErebusDestruction` (the destruction hold).

## Save migration

Level One now has 12 gameplay objectives. Save version 7 migrates any Level One state in `TenYearsLater` through `StarsDisappearing` to `ChapterComplete` rather than replaying those deprecated epilogue stages.

The migration is **not** version-gated. Gating it on `SaveVersion < 7` let a v7 write put the state straight back: `UAHCheckpointSubsystem::CaptureCheckpoint` stamps the checkpoint's own stage onto the saved chapter state, and `ChapterComplete`'s checkpoint (`Ch01_PresentDay`) carries `Stage=TenYearsLater`, so the boot after a migration restored into the removed epilogue with no objective and no way forward.

`CampaignProgress` divides by `AHChapterStateConstants::ObjectiveCount`, not the retired 17.

## Acceptance criteria

A Level One implementation is not considered complete unless all of these hold in a packaged Development build:

- cold open displays/plays `Did we win?` → `For a while.` with gameplay HUD hidden;
- Maya/Sael opening briefing plays after control returns;
- objective progression reaches the defensive line, Transit, battlefield, Manticore, Cathedral, terminal, escape, and destruction without contradictory objective/completion UI;
- failsafe countdown starts at 08:42;
- terminal displays casualty count `11,407,231` and requires inspect + confirmation before activation;
- Other Lucian dialogue occurs in the Cathedral and the second escape sighting does not deliver the old `No more time` line;
- the Erebus destruction sequence includes the Nysa `Lucian.` transmission before completion;
- final mission completion does not teleport into `TenYearsLater`;
- save/checkpoint restore cannot resurrect the removed post-Erebus Level One epilogue;
- required Unreal materials resolve in editor/commandlet automation;
- no visible Engine cube/checker material is used as normal runtime presentation;
- Mac packaged build and automated Level One narrative/progression/material tests pass before merge.

## Verification

`Scripts/Run-AutomationTests.sh` builds `AshesOfHeavenEditor` and runs the automation suite headless (`-nullrhi -nosound`), then fails the run if any test is not `Success` or if fewer than four `AshesOfHeaven.LevelOne.*` tests actually executed — a filter that matches nothing must not report green.

`Scripts/AuthorErebusStormCloud.py` authors `MI_Erebus_StormCloud` from the engine cloud master; it fails loudly if the engine renames a parameter rather than writing nothing.

`AshesOfHeaven.Combat.CombatantIsShootable` and `AshesOfHeaven.Combat.CorpseIsLootable` were failing on `main` as well: both spawned a combatant into a bare `UWorld` with no game instance, so `AAHCombatantCharacter::BeginPlay` could not reach `UAHEnemyAssetSubsystem` and the body never received its mesh, physics asset or loadout. Both are now latent tests on a standalone game-instance world that wait for the streamed definition, so the whole suite passes with an empty `AH_KNOWN_FAILURES`. That escape hatch stays available for a future inherited failure, and the runner fails if a listed test starts passing, so the list cannot rot.

`.github/workflows/cross-platform.yml` runs `source-validation`, `automation-tests` and `macos-shipping` on every pull request; the Windows/Android/iOS packages stay `workflow_dispatch`. `automation-tests` and `macos-shipping` need the self-hosted UE5 macOS runner and the `UNREAL_ENGINE_MAC_ROOT` repository variable, so treat them as required status checks — a repository without that runner configured skips them, and a skipped job is not a passing gate.

The Level One contract tests are:

- `AshesOfHeaven.LevelOne.NarrativeContract` — canonical lines, the exact casualty count, the Nysa closer, and the destruction-hold-covers-the-finale invariant.
- `AshesOfHeaven.LevelOne.ProgressionContract` — 12 objectives, stage/objective mapping, epilogue migration at any save version, and objective-index clamping.
- `AshesOfHeaven.LevelOne.StageDialogueQueue` — canonical lines replace a director's inline copy; a stage change landing mid-sequence queues its beat; a preempted beat resumes where it was cut.
- `AshesOfHeaven.LevelOne.ObjectiveCompleters` — every objective has a completer, and the Cathedral doorway trigger belongs to `FailsafeOrder`.
- `AshesOfHeaven.LevelOne.DialogueDoesNotRewindStage` — a late `Ch01_Sael` does not drag the chapter backwards, and an on-time one still advances.
- `AshesOfHeaven.LevelOne.UnrealMaterialContract` — every material the runtime loads by path resolves, including `MI_Erebus_StormCloud` for the cloud deck. A by-path load that returns null does not crash; it leaves the engine default material on the surface, which is how the Erebus sky was rendering on the stock volumetric cloud shader. `Scripts/Validate-CrossPlatform.sh` also asserts the material-instance files exist, so a deletion is caught on any runner.
