# Enemy motion pass — 2026-09-09

Preserves the updated crawler and Teuthisan bodies, materials, loadouts, and story changes.
Recovery archive: `Saved/EnemyMotionBefore-20260909.tar.gz` (affected assets before this pass).

## Changes

- Stalker idle/walk/run rewritten with two-bone leg IK, a fixed knee pole, grounded support phases, and separate walk/run foot trajectories. Original attack/death clips retained.
- Walk/run transitions preserve phase and synchronize both clips through the crossfade.
- Playback uses per-clip reference ground speeds instead of treating movement thresholds as stride measurements. Old definitions retain a fallback.
- Full-body melee stops translation and AI turning through recovery. Active attacks cannot be restarted by an expired cooldown. Timing includes the animation asset's RateScale.
- Impact points: Stalker 58%, Hound 62%, crawler 60%, Teuthisan 70%. Previously all were 32%, ahead of the visible strikes.
- Crawler maximum speed changed from 430 to 110 cm/s. Its short-limb run covers about 44 cm/s at native rate; the previous speed required nearly 10x cadence. This is a gameplay balance change worth playtesting.
- Teuthisan idle/walk/run close exactly at the loop seam. Updated source scripts preserve existing asset identities.
- All four desktop/mobile death effects remain empty: no generic death fire.

## Evidence and limits

Saved Stalker walk foot lift is 11.46/11.49 cm (left/right), run 23.06/23.05 cm. Previously run lift was approximately 42.7/69.2 cm. Sampled planted-foot speeds are approximately 75.5 cm/s walking and 271.5 cm/s running, matching the new reference speeds.

Teuthisan loop readback gaps: idle 0 cm; walk/run 0.0000073 cm. Reports: `Saved/EnemyMotionAudit.json` and `Saved/TeuthisanAnims/tanim_verify.json`.

The Unreal regression suite includes an actual native graph on a skeletal mesh: advancing time, walk/run phase continuity in both directions, and attack restarts. Clip previews and numeric sampling do not replace a full combat playtest.

At the end of September 9, Hound still needed a four-beat walk and Stalker needed a rifle hold. The continuation below addresses those two items. Teuthisan run remains a sped-up crawl. Crawler's current rig/skin weighting still limits natural foot contact. Clip/rig limitations are not all finished.

Rebuild Stalker locomotion with `AH_ANIM_TASKS=stalker_gait` and `Scripts/AuthorCreatureAnimations.py` inside Unreal. `Scripts/PolishEnemyMotion.py` applies motion-only definition tuning and regenerates Teuthisan loop closure; save/close the editor and back up assets first. `Scripts/AuditEnemyMotion.py` is read-only.

## Continuation — 2026-09-10

- Stalker idle/walk/run now solve both arms onto the rifle's measured primary and support grips. The new `HandGrip_R` socket follows `Bip01-R-Hand`, with inverse reference-hand rotation and 1/2.54 scale compensation. Saved-pose support-hand drift stays below 0.000062 cm. The rifle attachment now also checks socket name when the parent mesh is unchanged, covering asynchronously loaded bodies.
- Hound walk is now a 1.2-second four-beat cycle, authored from the native aggressive idle with leg IK. Each foot has 75% support and 25% swing, with four evenly staggered phases, 45 cm stance travel and 7 cm clearance. Native walking speed is 50 cm/s. The rear hock orientation is preserved, not flattened into a human ankle. Saved paw-target error is below 0.000055 cm.
- Hound starts walking at 5 cm/s and transitions to running at 140 cm/s, before the walk playback reaches its 3x cap. Both visual tiers receive the same tuning. Maximum chase speed remains 640 cm/s; original run, bite and death assets are unchanged.
- Stalker/Hound mesh materials and the user's replacement crawler/Teuthisan bodies remain unchanged. No death fire was reintroduced.

Recovery archives: `Saved/RiflePoseBefore-20260910.tar.gz` and `Saved/HoundWalkBefore-20260910.tar.gz`.

Rifle verification: `Scripts/AuthorStalkerRifle.py` and `Saved/StalkerRifleGrip.json`. Hound authoring: `Scripts/AuthorHoundWalk.py`, or the existing `AH_ANIM_TASKS=hound` entry point; `Saved/CreatureAnimations.json` records paw error. When submitting the Hound wrapper through remote execution, use `ExecuteStatement` with `exec(open(absolute_script_path).read())`: the runner's default ExecuteFile mode can misinterpret embedded `.py` strings as a filename.

Rendered rifle fit was reviewed in the September 10 `Saved/EnemyLineup.png` capture. That static bench suspends actors for inspection and is not proof of gameplay ground contact. Full-speed combat playtesting remains necessary; finger curl, recoil/reload, and vertical rifle aiming are not authored by this pass.

Final verification: Mac Development editor build passed; all 10 `AshesOfHeaven.Assets.Enemies` tests passed, including the real Pilgrim attachment regression and Hound walk settings on both tiers. Three Python loop tests passed. Report: `Saved/AutomationReport-EnemyMotion-Final-20260910/index.json`. This targeted run does not claim full-project or Shipping validation.
