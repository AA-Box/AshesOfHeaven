# ASHES OF HEAVEN performance budgets

These are initial configurable targets. The runtime values are exposed by `UAHPlatformManagerSubsystem` and must be tuned after representative profiling rather than treated as immutable physics limits.

| Budget | Desktop High | Desktop Low | High-End Mobile | Baseline Mobile |
| --- | ---: | ---: | ---: | ---: |
| Target FPS | 60 | 60 | 60 where sustainable | stable 30 |
| CPU frame time | <= 16.6 ms | <= 16.6 ms | <= 16.6 ms | <= 33.3 ms |
| GPU frame time | <= 16.6 ms | <= 16.6 ms | <= 16.6 ms | <= 33.3 ms |
| Tier-1 active AI | 24 | 15 | 16 | 8 |
| Mid-distance actors | 48 | 30 | 24 | 16 |
| Distant simulation actors | 96 | 64 | 48 | 32 |
| Persistent VFX | 64 | 40 | 24 | 12 |
| Dynamic lights | 16 | 8 | 4 | 2 |
| Projectile pool | 128 | 96 | 64 | 32 |
| Shadow distance | 12,000 cm | 8,000 cm | 3,500 cm | 2,500 cm |

## Profiling protocol

Run at least one 20–30 minute continuous Chapter One session on each representative device. Record `stat unit`, `stat gpu`, `stat memory`, `stat slate`, temperature/throttling, battery drain where available, and the active platform profile. Profile the opening, transit, Battle of Erebus, Cathedral exterior/interior, and escape transitions.

The battlefield uses three simulation tiers: full AI near the player, reduced-tick mid-distance actors, and cheap distant meshes/Niagara/tracers/explosions/audio/silhouettes. `AShooterNPCSpawner` registers tier-1 actors against the shared budget so mobile limits are enforced centrally.

## Renderer fallback policy

Nanite, Lumen, Virtual Shadow Maps, ray tracing, and cinematic post effects are optional desktop enhancements. Mobile device profiles disable them and retain the same silhouettes, lighting readability, combat feedback, and gameplay result through scalable alternatives.
