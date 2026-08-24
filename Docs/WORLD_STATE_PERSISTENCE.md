# World-State Persistence

Ashes stores meaningful campaign-world changes in the existing `UAHSaveGame` owned by
`UAHPlatformSaveSubsystem`. The platform subsystem remains the only disk/slot authority; world state is
captured by `UAHWorldStateSubsystem` and embedded atomically at checkpoint, explicit save, and suspension
boundaries.

## Save schema

`UAHSaveGame::WorldState` is an `FAHWorldStateSaveData`:

- `SchemaVersion`: version of the global actor-record table.
- `Actors`: deterministic, GUID-sorted `FAHWorldActorState` records.

Each actor record contains:

- `PersistentId`: authored or deterministically assigned `FGuid`.
- `ActorClass`: soft class path used to reject accidental ID reuse across classes.
- `LevelPackageName`: diagnostic ownership for streamed content; records do not require that level to be loaded.
- `StateVersion`: actor-local payload version.
- `SerializationMode`: compact explicit payload or `UPROPERTY(SaveGame)` proxy serialization.
- optional `Transform` plus `bHasTransform`.
- `bDestroyedOrConsumed`: a tombstone that removes the actor whenever it appears again.
- `Payload` and `PayloadCrc`: actor-local data and corruption isolation.

The current global campaign save version is 6 and the world-state schema version is 1. World-state data
first appeared in campaign save version 5.

## Persistent identity

Persistent actors implement `UAHSavableActor`. C++ actors normally own a
`UAHPersistentIdComponent`, whose `EditInstanceOnly` GUID is serialized into the level and cooked build.
Newly authored editor instances receive a GUID, non-PIE duplication receives a new GUID, and deterministic
runtime actors receive a fixed GUID before `FinishSpawning`.

Actor names are never used as identity. Renaming, streamed package reload, cooking, and generated runtime
names therefore do not change the save key.

The component participates in editor data validation. It reports missing IDs, ownership without the
savable interface, and duplicates inside the level. Runtime registration performs a second duplicate check
and refuses the later actor rather than letting two actors overwrite one record. Validation searches every
loaded level in the editor world so duplicate IDs across streamed sublevels are reported. Run the standard editor
**Validate Assets in Folder** command on maps/content before a release cook.

## Actor contract

`UAHSavableActor` provides:

- persistent ID;
- actor state version;
- explicit-payload or `SaveGame`-property mode;
- transform opt-in;
- capture, restore, and post-restore callbacks.

Use compact explicit payloads for state machines, inventory-like data, destructive changes, or any class
expected to evolve. `UPROPERTY(SaveGame)` mode is suitable for small, stable data-only actors. Never place
transient object pointers in an explicit payload.

When an actor changes meaningful state, call `UAHWorldStateSubsystem::MarkActorDirty`. Before a consumed or
destroyed persistent actor calls `Destroy`, call `MarkActorDestroyed`; this captures a tombstone immediately.
Dirty state is captured in memory at save boundaries, not every frame.

## Streaming lifecycle

The world subsystem imports all records when the game world initializes, even if their levels are absent.
It listens to `LevelAddedToWorld` and discovers every `UAHSavableActor` when a streamed level appears. A live
actor instance is registered once, matched by GUID, then restored. A later instance created by unloading and
reloading the level is a new instance and is restored again from the same authoritative record.

Before a level is removed, the subsystem captures its savable actors in memory and unregisters their live
instances. Records for absent actors, destroyed actors, and unloaded levels remain in the snapshot. No disk
write is caused by streaming.

Runtime-spawned persistent actors must use deferred spawning, set their deterministic GUID, and only then
call `FinishSpawning`. This prevents a default/invalid identity from registering during `BeginPlay`.

## Save and reset flow

- `UAHPlatformSaveSubsystem::SaveCombatCheckpoint` captures dirty world state into the same save object as
  the combat/chapter checkpoint.
- `SaveCheckpoint` does the same for the legacy/general checkpoint path.
- `SaveSuspensionCheckpoint` preserves the combat checkpoint and captures world state without invalidating it.
- `ResetProgress` clears the in-memory world-state authority and deletes the existing platform slot. Chapter
  new-game flows continue to reload the map, restoring authored actor defaults.

No world scan or disk write occurs per frame. Stream unload captures only that level in memory; authorized
save boundaries serialize the retained table.

## Versions, migration, and failure behavior

Global migration is centralized in `UAHPlatformSaveSubsystem::MigrateSaveObject`. Version 4 and earlier saves
keep their campaign/checkpoint data and receive an empty version-1 world-state table, so actors use authored
defaults. A save from a newer unsupported global version is not rewritten.

Actor payload migration belongs to the actor. Saved versions newer than the live class are skipped. Older
versions are passed to `RestoreWorldState`; the Chapter terminal demonstrates version-0 migration, where one
legacy confirmation byte becomes the current inspected/confirmed state.

Invalid GUIDs, missing classes, class mismatches, duplicate records, payload CRC failures, malformed actor
payloads, missing actors, and unsupported actor versions affect only that actor record. They are logged and
do not prevent the checkpoint, chapter, or other world actors from loading. A future global world-state
schema is treated as optional and ignored as a whole rather than bricking the campaign save.

## Representative actors

- `AAHWorldStateDoor`: explicit open/closed state and optional actor transform; interaction marks it dirty.
- `AAHChapterTerminal`: explicit inspected/confirmed state with version-0 migration; the Chapter One director
  assigns a deterministic terminal GUID before `BeginPlay`.
- `AAHWeaponPickup`: weapon, ammunition, and grenade pickups create tombstones before destruction. The Erebus
  combat-slice director assigns four stable deterministic pickup GUIDs before `BeginPlay`.

## Verification

Automation coverage under `AshesOfHeaven.WorldState` includes:

- save/load serialization;
- actor-name-independent stable IDs and duplicate detection;
- late/streamed actor application;
- destroyed/consumed pickup tombstones;
- transform plus explicit door state;
- retained missing-actor records;
- newer actor versions and corrupt payload isolation;
- global old-save and terminal payload migration;
- new-expedition/reset clearing.

Run the focused suite headlessly:

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  AshesOfHeaven.uproject \
  -ExecCmds="Automation RunTests AshesOfHeaven.WorldState;Quit" \
  -Unattended -NullRHI -NoSound -NoSplash -NoP4 -TestExit="Automation Test Queue Empty"
```

For package validation, build the `AshesOfHeaven` target and run a Development cook/package after the editor
automation suite. Any missing authored IDs or duplicate IDs should also be resolved through editor asset
validation before the package is signed.
