# Chapter One: FOR A WHILE — player journey

Story and progression audit, 2026-09-12. This describes the implemented Chapter One, not a proposed rewrite of the whole campaign. Contains ending spoilers. Progression is traced from the chapter director, objective subsystem, spatial definitions, and narrative scripts; physical traversal still needs a live start-to-finish playtest.

## Story spine

Lucian Vale wakes in a dying human colony. His immediate job is to protect evacuation with Maya Serrin. A survivor reveals that the Veil is converting people, not simply invading. Admiral Sael sends them to the Cathedral, where the infection becomes a threat to every inhabited world. Lucian authorizes Erebus's destruction to contain it. He escapes with Maya, but another version of himself and a final voice from Nysa undermine the apparent victory.

The player should always understand the immediate action without knowing the next revelation. Keep late-game information out of opening briefings.

## Start to finish

| Beat | Where to go / what to do | What happens / what advances it |
| --- | --- | --- |
| Cold open | Watch and listen; no movement objective yet. | A child asks whether they won. Lucian answers, “For a while.” Dialogue ends; District Nine begins. |
| 1. Reach the defensive line | Advance to the District Nine barricades. | Maya finds Lucian after seventeen minutes unconscious. Sael orders surviving units toward Transit North. Entering the defensive-line trigger starts the assault. |
| 2. Hold the Erebus line | Fight from cover; defeat the opening Veil assault. | The line buys evacuation time. This is the combat-clear gate; wait for the objective to change before leaving for the station. |
| 3. Reach Platform 02 | Follow the North Line through Transit North to Maya and the survivor. | Broken evacuation announcements establish that something is wrong. Reaching the platform trigger starts the revelation. |
| 4. Check on the survivor | Stay nearby and listen. No interaction button is required. | The survivor says the Veil remembers them. Maya realizes people are being converted. Sael refuses to explain and orders the Cathedral. Dialogue completion advances the mission. |
| 5. Cross the battlefield | Move between cover toward Ivo's rendezvous on the Cathedral route. | Ivo brings Manticore Four-Seven. This is a destination gate, not an all-enemies-dead gate. Reaching the rendezvous unlocks boarding. |
| 6. Board the Manticore | Approach Ivo's vehicle and use its board interaction. | Boarding completes the objective and starts Cathedral Approach. Drive toward the drop-off indicated by the direction/distance cue. |
| 7. Cathedral approach | Drive to the ramp drop-off, then climb the ramp on foot. | At X=11000 the vehicle parks and returns control to Lucian. Walking up the continuous ramp to X=14000 / floor Z=790 starts Sael's failsafe order. |
| 8. Enter the Cathedral | Cross the entrance while the transmission countdown runs. | Sael explains that the signal can leave Erebus. The 08:42 countdown is real: it starts on this stage, includes dialogue time, and must not reach zero before authorization. |
| 9. Reach the failsafe terminal | Follow the expedition walkway to the control terminal. | Other Lucian accuses the player of invading his world in an event that has not happened yet. Continue toward the terminal; no fight or dialogue choice is required here. |
| 10. Authorize failsafe | Interact once to inspect. Read the casualty assessment; interact again to confirm. | 11,407,231 people are at stake. Maya objects; Sael will not risk every inhabited system. Confirmation stops the carrier countdown and starts escape. No alternate rescue ending is implemented. |
| 11. Escape | Follow the illuminated route to shelter. Keep moving through attackers. | Other Lucian appears again. Reaching shelter, not clearing the escape encounter, completes this objective. |
| 12. Shelter / last light | Stay with Maya and listen; no further destination to find. | Erebus fades to black. Sael reports containment; Nysa speaks Lucian's name. The finale waits for its dialogue plus reading margin, then completes the chapter and saves completion. |

Ten Years Later, Fleet Departure, and Stars Disappearing remain compatibility stages in code. They are not part of the active twelve-objective Chapter One. The playable chapter ends at Erebus; the later campaign is not implemented by this pass.

## Changes made in this pass

- Station objective now names Platform 02 instead of asking a player already inside to enter the station.
- Objective descriptions distinguish combat completion, reaching a destination, listening, and interacting.
- Terminal objective explains both interactions and their irreversible fictional consequence.
- Escape explicitly points to shelter rather than implying every attacker must die.
- Finale says to stay in shelter, not to find shelter again.
- Briefing orders match these gates, with regression assertions for destination, listening, escape, and finale instructions.
- Existing objective IDs, save version, plot, and user asset changes are preserved.

## Remaining continuity / traversal risks

1. **Vehicle handoff (code revised September 13):** drive from X=8300 to the X=11000 drop-off, disembark automatically, then climb the continuous ramp to X=14000 / floor Z=790. Fixed suspension moving the root pawn and exit looking for the controller on the unpossessed character. Physical acceptance remains pending.
2. **Entry direction / checkpoint (code revised):** approach recovery now starts before the ramp; FailsafeOrder recovery starts before the entrance trigger. Triggers recheck existing overlaps so a dialogue-driven stage change cannot strand a player already inside.
3. **Navigation cues (code revised):** objective metadata now shows relative direction and distance. Approach targets drop-off, ramp base, then ramp top. SaelTransmission points at the terminal; OtherLucian keeps the escape destination. Visibility and physical traversal remain acceptance checks.
4. **Decision pacing:** two presses confirm the terminal even if its moral-argument dialogue is still playing. There is no separate choice UI or implemented refusal route. Decide whether confirmation should wait for the scene before adding a gate.
5. **Finale depiction:** the current effect is a camera fade with subtitles, not a fully staged destruction cinematic. Shelter survival and Nysa's voice need visual storytelling to carry the ending.

## Acceptance playtest

Start New Game, not autoplay. At each row above, record current objective, visible destination, required input, completion event, and next objective. Test each checkpoint separately; fail the countdown once; inspect without confirming once; confirm and reach shelter once. A debug stage jump or an automated objective-completion script is not proof that a player can traverse the route.

## Verification status

September 13 continuation: Mac Development editor build passed. All **126 Unreal automation tests passed**, including **22 Level One tests** and the enemy asset/motion checks. Three Python loop tests passed. Report: `Saved/AutomationReport-StoryFinal-20260913/index.json`; exact input hash and test overrides are recorded in `Docs/automation-evidence.json`.

The new physical route regression uses a locally owned player and normal character movement from X=11100 to X=24402.374, ending at Z=888.150 on the raised shelter route. It does not teleport between destinations. Separate vehicle tests verify boarding, root position, controller return, restored visibility/collision, and parking. Campaign lifecycle tests verify progression and persistence but still complete some objectives programmatically; these are not a manual combat playthrough.

The mounted weapon removes only explicitly tagged breakable barricades, not floor meshes. The escape obstruction was moved off the walkway. Two primitive-based escape fins were selectively replaced with existing facade kit meshes through live MCP; the rest of the authored level was preserved. Recovery copy: `Saved/CathedralBefore-20260913.umap`.

Build reproduction uses `-NoHotReload -NoUBA`: the accelerator stalled in `libUbaDetours` before linker initialization. Test launch uses `-DPCvars=TEDS.AddParentColumnToActors=0,TEDS.Feature.ActorCompatibility.ActorComponents.Enable=0` to avoid the UE 5.8 editor hierarchy crash. These disable editor bookkeeping for the isolated test process, not gameplay systems or assertions; no project configuration was changed. Use `-ExecCmds="Automation RunTests AshesOfHeaven;Quit"`, `-TestExit="Automation Test Queue Empty"`, and a unique report/log path and MCP port.

Fresh Mac Shipping packaging now passes build, cook, stage, archive, and PSO validation. Full manual start-to-finish combat, visual navigation acceptance, and a staged destruction cinematic remain unverified or unfinished.

Mac Development editor build passed. `AshesOfHeaven.LevelOne.PlayerBriefing` passed with the new destination and completion-condition assertions. Report: `Saved/AutomationReport-StoryBriefing-20260912/index.json`.

The September 12 broad run crashed with `pthread_rwlock_init failed with error: 16` in editor MassEntity/TEDS hierarchy bookkeeping. That failed run is superseded by the September 13 results above. The editor is restored to `/Game/ChapterOne/L_ChapterOne_Greybox`; assets were saved before and after the MCP changes.
