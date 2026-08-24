"""Author finite Chapter One Encounter Director Primary Data Assets.

Run after AuthorEnemyDefinitions.py. The script is idempotent and deliberately
authors only approved regions/phases; it does not place or edit level actors.
"""

import unreal


ENCOUNTER_PATH = "/Game/Ashes/Data/Encounters"
PILGRIM = "Pilgrim"
WARDEN = "Warden"
ALL_DIRECTIONS = 1 | 2 | 4 | 8


def _primary_asset_id(asset_type, name):
    return unreal.PrimaryAssetId(
        primary_asset_type=unreal.PrimaryAssetType(name=asset_type),
        primary_asset_name=unreal.Name(name),
    )


def _create_or_load(name):
    object_path = f"{ENCOUNTER_PATH}/{name}.{name}"
    existing = unreal.load_asset(object_path)
    if existing is not None:
        if not isinstance(existing, unreal.AHEncounterDefinition):
            raise RuntimeError(f"{object_path} is not an AHEncounterDefinition")
        return existing

    factory = unreal.DataAssetFactory()
    created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, ENCOUNTER_PATH, unreal.AHEncounterDefinition, factory
    )
    if created is None:
        raise RuntimeError(f"Failed to create {object_path}")
    return created


def _struct(struct_type, **properties):
    value = struct_type()
    for name, property_value in properties.items():
        value.set_editor_property(name, property_value)
    return value


def _slot(enemy_name, count, regions=None):
    return _struct(
        unreal.AHEncounterSpawnSlot,
        archetype_id=_primary_asset_id("AHEnemy", enemy_name),
        count=count,
        allowed_regions=[unreal.Name(region) for region in (regions or [])],
    )


def _pool(enemy_name, weight, cost, minimum_phase=0, maximum=0, veteran=1.0, damnation=1.0):
    return _struct(
        unreal.AHEncounterEnemyPoolEntry,
        archetype_id=_primary_asset_id("AHEnemy", enemy_name),
        weight=weight,
        spawn_cost_override=cost,
        minimum_phase=minimum_phase,
        maximum_per_encounter=maximum,
        veteran_weight_multiplier=veteran,
        damnation_weight_multiplier=damnation,
    )


def _region(region_id, center, extent, direction):
    return _struct(
        unreal.AHEncounterSpawnRegion,
        region_id=unreal.Name(region_id),
        center=unreal.Vector(*center),
        extent=unreal.Vector(*extent),
        direction=direction,
    )


def _phase(
    phase_id,
    trigger,
    fixed,
    regions,
    directions,
    bonus=0.0,
    delay=0.0,
    force_ratio=0.5,
    fill=False,
    maximum=0,
    scripted_trigger="",
):
    return _struct(
        unreal.AHEncounterPhaseDefinition,
        phase_id=unreal.Name(phase_id),
        trigger=trigger,
        force_remaining_ratio=force_ratio,
        scripted_trigger_id=unreal.Name(scripted_trigger),
        bonus_credits=bonus,
        reinforcement_delay=delay,
        fixed_composition=fixed,
        fill_from_enemy_pool=fill,
        maximum_spawns=maximum,
        allowed_regions=[unreal.Name(region) for region in regions],
        allowed_directions=directions,
    )


def _difficulty(difficulty, budget, delay, active_delta, sophistication):
    return _struct(
        unreal.AHEncounterDifficultyModifier,
        difficulty=difficulty,
        tactical_budget_multiplier=budget,
        reinforcement_delay_multiplier=delay,
        maximum_active_enemy_delta=active_delta,
        ai_sophistication_multiplier=sophistication,
    )


def _configure_common(
    definition,
    encounter_id,
    stage,
    objective_id,
    budget,
    starting_credits,
    active_cap,
    mobile_cap,
    total_cap,
    seed,
    regions,
    phases,
    pool,
    boss_slots=None,
):
    spawn_query = unreal.load_asset(
        "/Game/Variant_Shooter/Blueprints/AI/EQS_FindRoamLocation.EQS_FindRoamLocation"
    )
    if spawn_query is None:
        raise RuntimeError("EQS_FindRoamLocation is required for authored encounter spawning")

    definition.set_editor_property("encounter_id", unreal.Name(encounter_id))
    definition.set_editor_property("stage", stage)
    definition.set_editor_property("objective_id", unreal.Name(objective_id))
    definition.set_editor_property("enemy_budget", budget)
    definition.set_editor_property("starting_credits", starting_credits)
    definition.set_editor_property("credit_regeneration_per_second", 0.0)
    definition.set_editor_property("enemy_archetype_pool", pool)
    definition.set_editor_property("maximum_active_enemies", active_cap)
    definition.set_editor_property("mobile_maximum_active_enemies", mobile_cap)
    definition.set_editor_property("maximum_total_enemies", total_cap)
    definition.set_editor_property("phases", phases)
    definition.set_editor_property("default_reinforcement_delay", 2.0)
    definition.set_editor_property("spawn_query", spawn_query)
    definition.set_editor_property(
        "spawn_query_float_params",
        {
            unreal.Name("RoamBoxSize"): 1800.0,
            unreal.Name("RoamBoxSampleDistance"): 250.0,
        },
    )
    definition.set_editor_property("allowed_spawn_regions", regions)
    definition.set_editor_property("minimum_distance_from_player", 700.0)
    definition.set_editor_property("los_restriction", unreal.AHEncounterLOSRule.HIDDEN_FROM_PLAYER)
    definition.set_editor_property("allowed_directions", ALL_DIRECTIONS)
    definition.set_editor_property(
        "difficulty_modifiers",
        [
            _difficulty(unreal.AHEncounterDifficulty.STORY, 0.85, 1.20, -1, 0.85),
            _difficulty(unreal.AHEncounterDifficulty.SOLDIER, 1.00, 1.00, 0, 1.00),
            _difficulty(unreal.AHEncounterDifficulty.VETERAN, 1.15, 0.90, 0, 1.10),
            _difficulty(unreal.AHEncounterDifficulty.DAMNATION, 1.25, 0.75, 1, 1.25),
        ],
    )
    definition.set_editor_property(
        "completion_rule", unreal.AHEncounterCompletionRule.ALL_PHASES_AND_ENEMIES_DEFEATED
    )
    definition.set_editor_property("completion_trigger_id", unreal.Name())
    definition.set_editor_property("scripted_triggers", [unreal.Name("WestLaneOpened")])
    definition.set_editor_property("boss_hero_slots", boss_slots or [])
    definition.set_editor_property("deterministic_seed", seed)

    # Clear the legacy fixed-sequence manifest. Predicted streaming comes from the directed pool/slots.
    definition.set_editor_property("primary_enemy", unreal.PrimaryAssetId())
    definition.set_editor_property("additional_enemies", [])
    definition.set_editor_property("preload_visuals", True)
    definition.set_editor_property("preload_audio", True)
    unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False)
    return definition


def _author_defensive_line():
    initial = unreal.Name("InitialLine")
    west = unreal.Name("WestServiceLane")
    definition = _create_or_load("DA_Encounter_DefensiveLine")
    return _configure_common(
        definition=definition,
        encounter_id="Erebus_DefensiveLine",
        stage=unreal.AHChapterStage.OPENING_BATTLE,
        objective_id="Ch01_SurviveOpeningBattle",
        budget=16.0,
        starting_credits=8.0,
        active_cap=8,
        mobile_cap=5,
        total_cap=9,
        seed=71337,
        regions=[
            _region(initial, (2350.0, 250.0, 120.0), (600.0, 850.0, 260.0), unreal.AHEncounterDirection.EAST),
            _region(west, (2050.0, -1125.0, 120.0), (750.0, 300.0, 260.0), unreal.AHEncounterDirection.WEST),
        ],
        phases=[
            _phase(
                "InitialAssault",
                unreal.AHEncounterPhaseTrigger.IMMEDIATE,
                [_slot(PILGRIM, 4, [initial])],
                [initial],
                2,
            ),
            _phase(
                "WestLaneReinforcement",
                unreal.AHEncounterPhaseTrigger.FORCE_REMAINING_RATIO,
                [_slot(PILGRIM, 3, [west])],
                [west],
                8,
                bonus=4.0,
                delay=2.5,
                force_ratio=0.5,
                fill=True,
                maximum=4,
            ),
        ],
        pool=[
            _pool(PILGRIM, 1.0, 1.0, maximum=8),
            _pool(WARDEN, 0.55, 4.0, minimum_phase=1, maximum=2, veteran=1.10, damnation=1.25),
        ],
        boss_slots=[_slot(WARDEN, 1, [initial])],
    )


def _author_opening_manifest():
    region = unreal.Name("ErebusStreetFront")
    return _configure_common(
        definition=_create_or_load("DA_Encounter_ErebusOpening"),
        encounter_id="Erebus_Opening",
        stage=unreal.AHChapterStage.EREBUS_OPENING,
        objective_id="Ch01_ReachDefensiveLine",
        budget=6.0,
        starting_credits=6.0,
        active_cap=5,
        mobile_cap=4,
        total_cap=5,
        seed=41017,
        regions=[_region(region, (200.0, 0.0, 120.0), (650.0, 1100.0, 260.0), unreal.AHEncounterDirection.EAST)],
        phases=[_phase("StreetContact", unreal.AHEncounterPhaseTrigger.IMMEDIATE, [_slot(PILGRIM, 3, [region])], [region], 2)],
        pool=[_pool(PILGRIM, 1.0, 1.0, maximum=5)],
    )


def _author_cathedral_manifest():
    region = unreal.Name("CathedralOuterSteps")
    return _configure_common(
        definition=_create_or_load("DA_Encounter_CathedralApproach"),
        encounter_id="Erebus_CathedralApproach",
        stage=unreal.AHChapterStage.CATHEDRAL_APPROACH,
        objective_id="Ch01_ReachCathedralApproach",
        budget=12.0,
        starting_credits=8.0,
        active_cap=7,
        mobile_cap=5,
        total_cap=8,
        seed=91273,
        regions=[_region(region, (15100.0, 0.0, 890.0), (350.0, 750.0, 220.0), unreal.AHEncounterDirection.EAST)],
        phases=[_phase("OuterSteps", unreal.AHEncounterPhaseTrigger.IMMEDIATE, [_slot(PILGRIM, 4, [region]), _slot(WARDEN, 1, [region])], [region], 2)],
        pool=[_pool(PILGRIM, 1.0, 1.0, maximum=7), _pool(WARDEN, 0.4, 4.0, maximum=1)],
    )


def main():
    if unreal.load_asset("/Game/Ashes/Data/Enemies/DA_Enemy_Pilgrim.DA_Enemy_Pilgrim") is None:
        raise RuntimeError("Run Scripts/AuthorEnemyDefinitions.py before authoring encounters")
    if unreal.load_asset("/Game/Ashes/Data/Enemies/DA_Enemy_Warden.DA_Enemy_Warden") is None:
        raise RuntimeError("Run Scripts/AuthorEnemyDefinitions.py before authoring encounters")

    unreal.EditorAssetLibrary.make_directory(ENCOUNTER_PATH)
    _author_opening_manifest()
    _author_defensive_line()
    _author_cathedral_manifest()
    unreal.log(
        "Authored finite encounter definitions: Erebus_Opening, Erebus_DefensiveLine, "
        "Erebus_CathedralApproach"
    )


if __name__ == "__main__":
    main()
