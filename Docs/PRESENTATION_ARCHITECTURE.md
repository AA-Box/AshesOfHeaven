# Phase 4.3 — Presentation architecture

Phase 4.3 extends the Unreal-native presentation boundary. Gameplay code publishes state and
semantic events; Unreal content owns the visual and audio response.

## Runtime boundary

`AAHCombatHUD` is retained as the compatibility entry point used by the existing game modes, but
its production `DrawHUD()` is intentionally empty. It creates
`/Game/Ashes/UI/HUD/WBP_HUD_Root` and adds it to the viewport. No production UI path calls
`Canvas`, `GEngine->DefaultTexture`, or hard-coded Canvas coordinates.

`UAHHUDRootWidget` consumes the saved authored UMG hierarchy through safe-zone anchors and binds to health, armor, inventory,
ammo, interaction, objective, dialogue, countdown, chapter-stage, and Manticore presentation
delegates. Updates are event-driven. It does not tick to poll gameplay state.

The named WidgetBlueprint assets are saved entry points for designers:

`WBP_HUD_Root`, `WBP_Objective`, `WBP_PlayerStatus`, `WBP_WeaponStatus`, `WBP_Crosshair`,
`WBP_InteractionPrompt`, `WBP_DamageIndicator`, `WBP_Countdown`, `WBP_Dialogue`,
`WBP_TerminalIntel`, `WBP_ManticoreHUD`, `WBP_ChapterTitle`, and `WBP_TerminalWorld`.

The generator creates real designer-editable widget trees and named presentation children; runtime
C++ owns only state binding and visibility. Typography, brushes, materials, and animations can be
iterated in the saved Widget Blueprints without changing gameplay contracts.

## Data and content

Saved content is isolated below `/Game/Ashes`:

- UI: `/Game/Ashes/UI`
- audio: `/Game/Ashes/Audio`
- materials: `/Game/Ashes/Materials`
- Niagara: `/Game/Ashes/VFX`
- reusable presentation actors: `/Game/Ashes/Blueprints/Environment`
- presentation data: `/Game/Ashes/Presentation`

`AAHPresentationPropActor` is the native base for reusable art-target Blueprint actors. These
actors are presentation-only and do not replace the authoritative Phase 3 collision, encounter,
checkpoint, or navigation graph.

## Debug and review

Development-only console commands are registered as `AH.Debug.UI`,
`AH.Debug.UI.Objective`, `AH.Debug.UI.Damage`, `AH.Debug.UI.Countdown`,
`AH.Debug.Audio.M91`, `AH.Debug.Audio.Erebus`, `AH.Debug.Audio.Transit`,
`AH.Debug.Audio.Cathedral`, and `AH.Debug.Materials`.

`Scripts/Run-Mac-ArtTarget.sh` accepts `Erebus`, `Transit`, `Cathedral`, `LucianMaya`, `M91`,
`UI`, and `Audio`. It launches the real packaged viewport and never fabricates a screenshot or
human approval.

## Performance and platform rules

The root uses a real `USafeZone` and adaptive viewport layout. UI state changes are delegate
driven, materials are saved assets, and Niagara is authored in project emitter/system assets with
effect-specific renderer materials, deterministic seeds, allocation budgets, and fixed bounds rather
than copied engine templates. Desktop may
use richer presentation tiers; the baseline must remain readable and scalable on mobile. Final
device FPS, touch glyphs, and subjective visual approval remain human/device review gates.
