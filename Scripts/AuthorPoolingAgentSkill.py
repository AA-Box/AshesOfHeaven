import json

import unreal


ASSET_FOLDER = "/Game/Ashes/Skills/"
ASSET_NAME = "AHPoolingLifecycle"
ASSET_PATH = f"{ASSET_FOLDER}{ASSET_NAME}"
DESCRIPTION = (
    "Defines Ashes of Heaven's measurement gate and strict reset contract for pooling transient gameplay objects. "
    "Apply when adding, changing, profiling, or debugging pooled projectiles and effects."
)
INSTRUCTIONS = """Measure representative spawn/destroy churn before registering any concrete class. Leave negligible-frequency objects unpooled.

Pooling is concrete-class opt-in. A derived class that adds state must explicitly opt in again and own a complete reset contract. On release, clear per-activation delegates, timers, latent work, ownership, instigation, collision, velocity, damage state, Niagara and audio state, material overrides, transforms, tags, references, and lifespan. On acquire, restore the class defaults before applying shot-specific owner, instigator, transform, direction, and damage.

Never simulate activation by invoking BeginPlay or EndPlay. A pooled actor receives each Unreal lifecycle callback once; repeated activations use only the pool acquire/release contract.

Prefer Unreal's native Niagara component pooling for one-shot effects. Do not wrap Niagara systems in transient actors unless measurements and a complete actor reset contract justify it.

Pool exhaustion must grow only to the performance-profile hard limit, then use a normal spawn fallback for critical gameplay objects. Checkpoint restores and world transitions must clear active transient objects.

After changing a poolable type, run the lifecycle stress, exhaustion, simultaneous-impact, checkpoint-reset, and world-transition coverage. Compare measured construction count, elapsed time, and memory behavior before claiming an improvement."""

TOOLSET_NAME = "ToolsetRegistry.AgentSkillToolset"


def call_agent_skill_tool(tool_name: str, payload: dict) -> object:
    result = unreal.ToolsetRegistry.execute_tool(
        TOOLSET_NAME,
        tool_name,
        json.dumps(payload),
    )
    if not result.is_complete:
        raise RuntimeError(f"{TOOLSET_NAME}.{tool_name} did not complete synchronously")
    if result.error:
        raise RuntimeError(f"{TOOLSET_NAME}.{tool_name}: {result.error}")
    response = json.loads(result.value)
    return response["returnValue"]


def main() -> None:
    # AICallable-only functions intentionally are not exposed as ordinary Python methods.
    # Invoke the registered toolset surface so this follows the same path as unreal-mcp.
    summaries = call_agent_skill_tool("ListSkills", {})
    unreal.log(f"AH_POOL_SKILL_LIST count={len(summaries)} paths={list(summaries.keys())}")

    existing_path = next(
        (path for path in summaries if path.endswith(f"/{ASSET_NAME}.{ASSET_NAME}_C")),
        None,
    )
    details = {"instructions": INSTRUCTIONS}

    if existing_path:
        prior = call_agent_skill_tool("GetSkills", {"skillPaths": [existing_path]})
        unreal.log(f"AH_POOL_SKILL_EXISTING {prior}")
        updated = call_agent_skill_tool(
            "UpdateSkill",
            {"skillPath": existing_path, "description": DESCRIPTION, "details": details},
        )
        if not updated:
            raise RuntimeError(f"Unable to update {existing_path}")
        skill_path = existing_path
    else:
        skill_path = call_agent_skill_tool(
            "CreateSkill",
            {
                "folderPath": ASSET_FOLDER,
                "assetName": ASSET_NAME,
                "description": DESCRIPTION,
                "details": details,
            },
        )
        if not skill_path:
            raise RuntimeError("Unable to create pooling Agent Skill")

    if not unreal.EditorAssetLibrary.save_asset(ASSET_PATH, only_if_is_dirty=False):
        raise RuntimeError(f"Unable to save {ASSET_PATH}")

    verified = call_agent_skill_tool("GetSkills", {"skillPaths": [skill_path]})
    if skill_path not in verified:
        raise RuntimeError(f"Unable to read back {skill_path}")
    unreal.log(f"AH_POOL_SKILL_VERIFIED path={skill_path} details={verified[skill_path]}")


main()
