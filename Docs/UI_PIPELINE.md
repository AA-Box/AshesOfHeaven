# Phase 4.2 — UI pipeline

The production HUD is Unreal UMG. Canvas drawing is not a production fallback; it is no longer
used by `AAHCombatHUD::DrawHUD()`.

## Widget contract

`AAHCombatHUD` owns lifecycle and compatibility. `UAHHUDRootWidget` owns presentation state.
Gameplay emits:

- objective changed/completed/mission complete;
- health, armor, damage and death feedback;
- inventory, grenade and weapon-ammo changes;
- interaction target/prompt changes;
- dialogue line changes;
- countdown changed and chapter-stage changed;
- Manticore driver and low-rate presentation changes.

The root binds once, removes bindings on destruction, and forwards changes to child presentation
controls. There is no HUD Tick polling loop.

## Visual hierarchy

The baseline layout intentionally removes the permanent Lucian dossier label, the large mission
metadata box, and the giant dialogue strip. Objective text is a brief top-left reveal; countdown
is a compact top-right warning; health/armor and weapon/ammo are peripheral; the reticle is
restrained and context-aware; dialogue is subtitle-first; completion is a centered chapter title.

The baseline is safe-zone anchored and uses FText for state text. The checked-in Widget Blueprints
already own the hierarchy, text widgets, rules, progress bars, and composition slots; designers can
replace those authored elements with localized fonts, materials, input glyphs, CommonUI, or UMG
animations without altering the state contract.

## Required authored assets

The following saved assets exist under `/Game/Ashes/UI`:

`WBP_HUD_Root`, `WBP_Objective`, `WBP_PlayerStatus`, `WBP_WeaponStatus`, `WBP_Crosshair`,
`WBP_InteractionPrompt`, `WBP_DamageIndicator`, `WBP_Countdown`, `WBP_Dialogue`,
`WBP_TerminalIntel`, `WBP_ManticoreHUD`, `WBP_ChapterTitle`, and `WBP_TerminalWorld`.

`DA_HUDStyle_Default` stores the default Bone/Amber/Cyan style and reveal duration. It is the
designer handoff point for typography, brushes, color grading, and motion.

## Validation

`AshesOfHeaven.Presentation.AssetManifest` loads every UMG class. The same asset list is checked
by `AHCombatVerificationCommandlet` and the full automation suite. Runtime visual hierarchy,
readability at 1280×720, mobile safe zones, localization review, and final visual match still
require a human review on the packaged build.
