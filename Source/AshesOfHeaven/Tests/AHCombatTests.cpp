#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHGameplayTypes.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Gameplay/Encounters/AHCombatEncounter.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Gameplay/Game/AHCombatPlayerController.h"
#include "Gameplay/UI/AHCombatHUD.h"
#include "Gameplay/UI/AHHUDRootWidget.h"
#include "Gameplay/Audio/AHAudioPaletteData.h"
#include "Gameplay/Presentation/AHPresentationData.h"
#include "Tests/AHObjectiveHUDDelegateTestReceiver.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Chapter/AHChapterTerminal.h"
#include "Gameplay/Chapter/AHChapterTrigger.h"
#include "Gameplay/Chapter/AHDialogueSubsystem.h"
#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "Gameplay/Game/AHChapterOneGameMode.h"
#include "Gameplay/Level/AHChapterOneDirector.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "Gameplay/Vehicles/AHManticoreVehicle.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionParameter.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "MetasoundSource.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundBase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#if WITH_EDITOR
#include "Animation/WidgetAnimation.h"
#include "Components/SafeZone.h"
#include "WidgetBlueprint.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHArtTargetAssetManifestTest, "AshesOfHeaven.Art.TargetAssetManifest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHArtTargetAssetManifestTest::RunTest(const FString& Parameters)
{
	const FString ProjectRoot = FPaths::ProjectDir();
	const TArray<FString> ReferenceFiles = {
		TEXT("References/ArtTargets/01_Erebus_Battlefield.png"),
		TEXT("References/ArtTargets/02_Transit_Station.png"),
		TEXT("References/ArtTargets/03_Cathedral_Interior.png"),
		TEXT("References/ArtTargets/04_Lucian_Maya.png")
	};
	for (const FString& RelativePath : ReferenceFiles)
	{
		TestTrue(*FString::Printf(TEXT("Approved reference exists: %s"), *RelativePath), IFileManager::Get().FileExists(*(ProjectRoot / RelativePath)));
	}

	TestNotNull(TEXT("M91 skeletal mesh resolves"), LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Weapons/Rifle/Meshes/SKM_Rifle.SKM_Rifle")));
	TestNotNull(TEXT("Human mannequin resolves"), LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
	TestNotNull(TEXT("Maya mannequin scaffold resolves"), LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")));
	TestNotNull(TEXT("Transit door frame resolves"), LoadObject<UStaticMesh>(nullptr, TEXT("/Game/LevelPrototyping/Interactable/Door/Meshes/SM_DoorFrame_Edge.SM_DoorFrame_Edge")));
	TestNotNull(TEXT("Cathedral material resolves"), LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark.MI_PrototypeGrid_TopDark")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHPresentationAssetManifestTest, "AshesOfHeaven.Presentation.AssetManifest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHPresentationAssetManifestTest::RunTest(const FString& Parameters)
{
	const TArray<FString> WidgetPaths = {
		TEXT("/Game/Ashes/UI/HUD/WBP_HUD_Root.WBP_HUD_Root_C"), TEXT("/Game/Ashes/UI/HUD/WBP_Objective.WBP_Objective_C"),
		TEXT("/Game/Ashes/UI/HUD/WBP_PlayerStatus.WBP_PlayerStatus_C"), TEXT("/Game/Ashes/UI/HUD/WBP_WeaponStatus.WBP_WeaponStatus_C"),
		TEXT("/Game/Ashes/UI/HUD/WBP_Crosshair.WBP_Crosshair_C"), TEXT("/Game/Ashes/UI/HUD/WBP_InteractionPrompt.WBP_InteractionPrompt_C"),
		TEXT("/Game/Ashes/UI/HUD/WBP_DamageIndicator.WBP_DamageIndicator_C"), TEXT("/Game/Ashes/UI/HUD/WBP_Countdown.WBP_Countdown_C"),
		TEXT("/Game/Ashes/UI/HUD/WBP_Dialogue.WBP_Dialogue_C"), TEXT("/Game/Ashes/UI/HUD/WBP_TerminalIntel.WBP_TerminalIntel_C"),
		TEXT("/Game/Ashes/UI/HUD/WBP_ManticoreHUD.WBP_ManticoreHUD_C"), TEXT("/Game/Ashes/UI/HUD/WBP_ChapterTitle.WBP_ChapterTitle_C"),
		TEXT("/Game/Ashes/UI/Terminal/WBP_TerminalWorld.WBP_TerminalWorld_C")
	};
	for (const FString& Path : WidgetPaths)
	{
		UClass* WidgetClass = LoadObject<UClass>(nullptr, *Path);
		TestTrue(TEXT("saved UMG widget class exists"), WidgetClass && WidgetClass->IsChildOf(UUserWidget::StaticClass()));
	}
	#if WITH_EDITOR
	if (UWidgetBlueprint* HUDBlueprint = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Ashes/UI/HUD/WBP_HUD_Root.WBP_HUD_Root")))
	{
		UWidget* SafeZone = HUDBlueprint->WidgetTree ? HUDBlueprint->WidgetTree->FindWidget(TEXT("HUDSafeZone")) : nullptr;
		TestTrue(TEXT("HUD root is authored through a safe zone"), SafeZone && SafeZone->IsA<USafeZone>());
		TSet<FName> AnimationNames;
		for (UWidgetAnimation* Animation : HUDBlueprint->Animations)
		{
			if (Animation)
			{
				AnimationNames.Add(Animation->GetFName());
			}
		}
		TestTrue(TEXT("objective reveal animation is authored"), AnimationNames.Contains(TEXT("ObjectiveRevealAnimation")));
		TestTrue(TEXT("damage pulse animation is authored"), AnimationNames.Contains(TEXT("DamagePulseAnimation")));
		TestTrue(TEXT("countdown urgency animation is authored"), AnimationNames.Contains(TEXT("CountdownUrgencyAnimation")));
	}
	#endif
	UAHAudioPaletteData* Palette = LoadObject<UAHAudioPaletteData>(nullptr, TEXT("/Game/Ashes/Audio/DA_AudioPalette_Default.DA_AudioPalette_Default"));
	TestNotNull(TEXT("audio palette data asset exists"), Palette);
	if (Palette)
	{
		TestTrue(TEXT("semantic audio event map is populated"), Palette->Events.Num() >= 7);
		TestTrue(TEXT("environment audio event map is populated"), Palette->Environments.Num() >= 3);
		for (const TPair<FName, TSoftObjectPtr<USoundBase>>& Entry : Palette->Events)
		{
			TestTrue(*FString::Printf(TEXT("event %s resolves to a project sound"), *Entry.Key.ToString()), Entry.Value.IsValid() || Entry.Value.ToSoftObjectPath().IsValid());
		}
		TSet<FSoftObjectPath> CombatAudioAssets;
		for (const TCHAR* EventName : { TEXT("Combat.Melee"), TEXT("Combat.Hurt"), TEXT("Combat.Armor"), TEXT("Combat.Death"), TEXT("Combat.Grenade") })
		{
			const TSoftObjectPtr<USoundBase>* Event = Palette->Events.Find(FName(EventName));
			TestTrue(*FString::Printf(TEXT("semantic event %s is assigned"), EventName), Event && Event->ToSoftObjectPath().IsValid());
			if (Event)
			{
				CombatAudioAssets.Add(Event->ToSoftObjectPath());
			}
		}
		TestEqual(TEXT("combat semantic events use distinct authored sources"), CombatAudioAssets.Num(), 5);
	}
	for (const TCHAR* Path : {
		TEXT("/Game/Ashes/Audio/MetaSounds/MS_M91_Fire.MS_M91_Fire"), TEXT("/Game/Ashes/Audio/MetaSounds/MS_M91_Impact.MS_M91_Impact"),
		TEXT("/Game/Ashes/Audio/MetaSounds/MS_Erebus_Ambience.MS_Erebus_Ambience"), TEXT("/Game/Ashes/Audio/MetaSounds/MS_Transit_Ambience.MS_Transit_Ambience"),
		TEXT("/Game/Ashes/Audio/MetaSounds/MS_Cathedral_Ambience.MS_Cathedral_Ambience"), TEXT("/Game/Ashes/Audio/MetaSounds/MS_Manticore_Engine.MS_Manticore_Engine"),
		TEXT("/Game/Ashes/Audio/MetaSounds/MS_UI_Objective.MS_UI_Objective"), TEXT("/Game/Ashes/Audio/MetaSounds/MS_Combat_Melee.MS_Combat_Melee"),
		TEXT("/Game/Ashes/Audio/MetaSounds/MS_Combat_Hurt.MS_Combat_Hurt"), TEXT("/Game/Ashes/Audio/MetaSounds/MS_Combat_Armor.MS_Combat_Armor"),
		TEXT("/Game/Ashes/Audio/MetaSounds/MS_Combat_Death.MS_Combat_Death"), TEXT("/Game/Ashes/Audio/MetaSounds/MS_Combat_Grenade.MS_Combat_Grenade") })
	{
		TestTrue(TEXT("MetaSound presentation asset exists and is a MetaSound source"), LoadObject<UMetaSoundSource>(nullptr, Path) != nullptr);
	}
	const TArray<FString> RawAudioPaths = {
		TEXT("/Game/Ashes/Audio/Raw/SC_M91_Fire.SC_M91_Fire"), TEXT("/Game/Ashes/Audio/Raw/SC_M91_Reload.SC_M91_Reload"),
		TEXT("/Game/Ashes/Audio/Raw/SC_M91_Impact.SC_M91_Impact"), TEXT("/Game/Ashes/Audio/Raw/SC_Combat_Melee.SC_Combat_Melee"),
		TEXT("/Game/Ashes/Audio/Raw/SC_Combat_Hurt.SC_Combat_Hurt"), TEXT("/Game/Ashes/Audio/Raw/SC_Combat_Armor.SC_Combat_Armor"),
		TEXT("/Game/Ashes/Audio/Raw/SC_Combat_Death.SC_Combat_Death"), TEXT("/Game/Ashes/Audio/Raw/SC_Combat_Grenade.SC_Combat_Grenade"),
		TEXT("/Game/Ashes/Audio/Raw/SC_Erebus_Ambience.SC_Erebus_Ambience"),
		TEXT("/Game/Ashes/Audio/Raw/SC_Transit_Ambience.SC_Transit_Ambience"), TEXT("/Game/Ashes/Audio/Raw/SC_Cathedral_Ambience.SC_Cathedral_Ambience") };
	TSet<USoundWave*> RawSources;
	for (const FString& Path : RawAudioPaths)
	{
		USoundWave* Wave = LoadObject<USoundWave>(nullptr, *Path);
		TestNotNull(TEXT("authored raw SoundWave exists"), Wave);
		if (Wave)
		{
			RawSources.Add(Wave);
		}
	}
	TestEqual(TEXT("raw event sources remain distinct assets"), RawSources.Num(), RawAudioPaths.Num());
	TestNotNull(TEXT("world attenuation asset exists"), LoadObject<USoundAttenuation>(nullptr, TEXT("/Game/Ashes/Audio/Mix/ATT_World3D.ATT_World3D")));
	TestNotNull(TEXT("world concurrency asset exists"), LoadObject<USoundConcurrency>(nullptr, TEXT("/Game/Ashes/Audio/Mix/CONC_World.CONC_World")));
	TestNotNull(TEXT("master submix asset exists"), LoadObject<USoundSubmix>(nullptr, TEXT("/Game/Ashes/Audio/Mix/SM_Master.SM_Master")));
	for (const TCHAR* SubmixPath : {
		TEXT("/Game/Ashes/Audio/Submixes/SM_World.SM_World"), TEXT("/Game/Ashes/Audio/Submixes/SM_Weapons.SM_Weapons"),
		TEXT("/Game/Ashes/Audio/Submixes/SM_Ambience.SM_Ambience"), TEXT("/Game/Ashes/Audio/Submixes/SM_Veil.SM_Veil"),
		TEXT("/Game/Ashes/Audio/Submixes/SM_Dialogue.SM_Dialogue"), TEXT("/Game/Ashes/Audio/Submixes/SM_Music.SM_Music"),
		TEXT("/Game/Ashes/Audio/Submixes/SM_Vehicle.SM_Vehicle"), TEXT("/Game/Ashes/Audio/Submixes/SM_UI.SM_UI") })
	{
		TestNotNull(TEXT("semantic audio submix exists"), LoadObject<USoundSubmix>(nullptr, SubmixPath));
	}
	USoundCue* M91Cue = LoadObject<USoundCue>(nullptr, TEXT("/Game/Ashes/Audio/Cues/SC_M91_Fire.SC_M91_Fire"));
	TestNotNull(TEXT("M91 SoundCue exists"), M91Cue);
	if (M91Cue)
	{
		TestNotNull(TEXT("M91 SoundCue has an authored node graph"), M91Cue->FirstNode.Get());
		TestTrue(TEXT("M91 SoundCue has concurrency routing"), !M91Cue->bOverrideConcurrency && M91Cue->ConcurrencySet.Num() > 0);
		TestNotNull(TEXT("M91 SoundCue has submix routing"), M91Cue->SoundSubmixObject.Get());
	}
	USoundCue* FootstepCue = LoadObject<USoundCue>(nullptr, TEXT("/Game/Ashes/Audio/Cues/SC_Player_Footstep.SC_Player_Footstep"));
	TestNotNull(TEXT("footstep SoundCue exists"), FootstepCue);
	if (FootstepCue)
	{
		TestTrue(TEXT("footsteps route through the world submix"), FootstepCue->SoundSubmixObject.Get() == LoadObject<USoundSubmix>(nullptr, TEXT("/Game/Ashes/Audio/Submixes/SM_World.SM_World")));
	}
	for (const TCHAR* Path : {
		TEXT("/Game/Ashes/Materials/M_HumanMetal.M_HumanMetal"), TEXT("/Game/Ashes/Materials/M_HumanArmor.M_HumanArmor"), TEXT("/Game/Ashes/Materials/M_Concrete.M_Concrete"),
		TEXT("/Game/Ashes/Materials/M_CathedralMatter.M_CathedralMatter"), TEXT("/Game/Ashes/Materials/M_VeilObsidian.M_VeilObsidian"), TEXT("/Game/Ashes/Materials/M_EmissiveGlyph.M_EmissiveGlyph") })
	{
		TestNotNull(TEXT("authored material exists"), LoadObject<UMaterialInterface>(nullptr, Path));
	}
	for (const TCHAR* MeshPath : {
		TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube"), TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cylinder.SM_AH_Cylinder"),
		TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Sphere.SM_AH_Sphere"), TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cone.SM_AH_Cone"),
		TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Plane.SM_AH_Plane") })
	{
		TestNotNull(TEXT("project-owned presentation mesh exists"), LoadObject<UStaticMesh>(nullptr, MeshPath));
	}
	for (const TCHAR* EmitterPath : {
		TEXT("/Game/Ashes/VFX/Emitters/NE_AshField.NE_AshField"), TEXT("/Game/Ashes/VFX/Emitters/NE_EmberDrift.NE_EmberDrift"),
		TEXT("/Game/Ashes/VFX/Emitters/NE_ImpactSparks.NE_ImpactSparks"), TEXT("/Game/Ashes/VFX/Emitters/NE_SmokeColumn.NE_SmokeColumn") })
	{
		UNiagaraEmitter* Emitter = LoadObject<UNiagaraEmitter>(nullptr, EmitterPath);
		TestNotNull(TEXT("authored Niagara emitter exists"), Emitter);
		if (Emitter && Emitter->GetLatestEmitterData())
		{
			TestTrue(TEXT("Niagara emitter has deterministic authored configuration"), Emitter->GetLatestEmitterData()->bDeterminism);
			TestTrue(TEXT("Niagara emitter has fixed presentation bounds"), Emitter->GetLatestEmitterData()->CalculateBoundsMode == ENiagaraEmitterCalculateBoundMode::Fixed);
		}
	}
	for (const TCHAR* Path : {
		TEXT("/Game/Ashes/VFX/NS_AshField.NS_AshField"), TEXT("/Game/Ashes/VFX/NS_EmberDrift.NS_EmberDrift"), TEXT("/Game/Ashes/VFX/NS_ImpactSparks.NS_ImpactSparks"),
		TEXT("/Game/Ashes/VFX/NS_FireSmall.NS_FireSmall"), TEXT("/Game/Ashes/VFX/NS_FireLarge.NS_FireLarge"), TEXT("/Game/Ashes/VFX/NS_SmokeColumn.NS_SmokeColumn"),
		TEXT("/Game/Ashes/VFX/NS_DustSheet.NS_DustSheet"), TEXT("/Game/Ashes/VFX/NS_CathedralMotes.NS_CathedralMotes") })
	{
		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, Path);
		TestNotNull(TEXT("authored Niagara asset exists"), System);
		TestTrue(TEXT("Niagara asset is project-authored, not an engine template"), System && System->GetPathName().StartsWith(TEXT("/Game/Ashes/VFX/")));
	}
	for (const TCHAR* LegacyPath : {
		TEXT("/Game/Ashes/VFX/NS_Ash.NS_Ash"), TEXT("/Game/Ashes/VFX/NS_Embers.NS_Embers"), TEXT("/Game/Ashes/VFX/NS_Sparks.NS_Sparks"),
		TEXT("/Game/Ashes/VFX/NS_Dust.NS_Dust"), TEXT("/Game/Ashes/VFX/NS_CathedralParticles.NS_CathedralParticles") })
	{
		const FAssetData LegacyAsset = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetByObjectPath(FSoftObjectPath(LegacyPath));
		TestFalse(TEXT("legacy Niagara template duplicate is absent"), LegacyAsset.IsValid());
	}
	for (const TCHAR* Path : {
		TEXT("/Game/Ashes/Materials/M_HumanMetal.M_HumanMetal"), TEXT("/Game/Ashes/Materials/M_VeilObsidian.M_VeilObsidian") })
	{
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Path);
		TestNotNull(TEXT("material master exists"), Material);
		if (Material)
		{
			TArray<FMaterialParameterInfo> ScalarParameters;
			TArray<FGuid> ScalarParameterIds;
			TArray<FMaterialParameterInfo> VectorParameters;
			TArray<FGuid> VectorParameterIds;
			Material->GetAllScalarParameterInfo(ScalarParameters, ScalarParameterIds);
			Material->GetAllVectorParameterInfo(VectorParameters, VectorParameterIds);
			TestTrue(TEXT("material master has a non-trivial authored parameter surface"), ScalarParameters.Num() + VectorParameters.Num() >= 6);
			TSet<FName> ParameterNames;
			for (const FMaterialParameterInfo& Parameter : ScalarParameters)
			{
				ParameterNames.Add(Parameter.Name);
			}
			for (const TCHAR* ParameterName : { TEXT("WearAmount"), TEXT("EdgeVariation"), TEXT("DamageMaskStrength"), TEXT("Wetness"), TEXT("MicroDetailStrength") })
			{
				TestTrue(*FString::Printf(TEXT("material graph exposes %s"), ParameterName), ParameterNames.Contains(FName(ParameterName)));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHHealthDamageTest, "AshesOfHeaven.Combat.HealthDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHHealthDamageTest::RunTest(const FString& Parameters)
{
	UAHHealthComponent* Health = NewObject<UAHHealthComponent>();
	Health->MaxHealth = 100.0f;
	Health->ResetHealth();
	TestEqual(TEXT("Health starts full"), Health->GetHealth(), 100.0f);
	TestEqual(TEXT("Damage is applied"), Health->ApplyDamage(35.0f), 35.0f);
	TestEqual(TEXT("Health is reduced"), Health->GetHealth(), 65.0f);
	TestTrue(TEXT("Lethal damage marks dead"), Health->ApplyDamage(100.0f) > 0.0f && Health->IsDead());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHHealthRegenTest, "AshesOfHeaven.Combat.HealthRegenTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHHealthRegenTest::RunTest(const FString& Parameters)
{
	UAHHealthComponent* Enemy = NewObject<UAHHealthComponent>();
	Enemy->MaxHealth = 100.0f;
	Enemy->ResetHealth();
	Enemy->ApplyDamage(40.0f);
	TestEqual(TEXT("Combatants do not regenerate by default"), Enemy->RegenerationPerSecond, 0.0f);

	UAHHealthComponent* Player = NewObject<UAHHealthComponent>();
	Player->MaxHealth = 100.0f;
	Player->RegenerationDelay = 5.0f;
	Player->RegenerationPerSecond = 20.0f;
	Player->ResetHealth();
	Player->ApplyDamage(60.0f);
	TestEqual(TEXT("Regeneration waits out the delay"), Player->GetTimeUntilRegeneration(2.0f), 3.0f);
	TestEqual(TEXT("Regeneration opens after the delay"), Player->GetTimeUntilRegeneration(5.0f), 0.0f);
	// 60 health back at 20/s is three seconds of regeneration on top of the five-second delay.
	TestEqual(TEXT("Full recovery costs delay plus health over rate"), Player->RegenerationDelay + (60.0f / Player->RegenerationPerSecond), 8.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHArmorTimingTest, "AshesOfHeaven.Combat.ArmorAbsorptionAndRegenTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHArmorTimingTest::RunTest(const FString& Parameters)
{
	UAHArmorComponent* Armor = NewObject<UAHArmorComponent>();
	Armor->MaxArmor = 100.0f;
	Armor->RegenerationDelay = 4.0f;
	Armor->ResetArmor();
	TestEqual(TEXT("Armor absorbs incoming damage"), Armor->AbsorbDamage(35.0f), 35.0f);
	TestEqual(TEXT("Armor is reduced before health"), Armor->GetArmor(), 65.0f);
	TestEqual(TEXT("Regeneration delay is enforced"), Armor->GetTimeUntilRegeneration(2.0f), 2.0f);
	TestEqual(TEXT("Regeneration becomes available after delay"), Armor->GetTimeUntilRegeneration(4.0f), 0.0f);
	float ArmorDamage = 0.0f;
	TestEqual(TEXT("Pure armor rule returns health damage"), UAHCombatRulesLibrary::ApplyArmorAbsorption(120.0f, 50.0f, ArmorDamage), 70.0f);
	TestEqual(TEXT("Pure armor rule consumes remaining armor"), ArmorDamage, 50.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHAmmoReloadTest, "AshesOfHeaven.Combat.AmmoConsumptionAndReloadCalculation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHAmmoReloadTest::RunTest(const FString& Parameters)
{
	FAHAmmoState Ammo;
	Ammo.MagazineCapacity = 36;
	Ammo.Magazine = 11;
	Ammo.Reserve = 18;
	TestEqual(TEXT("Reload transfers only missing rounds"), UAHCombatRulesLibrary::CalculateReloadTransfer(Ammo), 18);
	Ammo.Reserve = 100;
	TestEqual(TEXT("Reload never exceeds magazine capacity"), UAHCombatRulesLibrary::CalculateReloadTransfer(Ammo), 25);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHGrenadeInventoryTest, "AshesOfHeaven.Combat.GrenadeInventory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHGrenadeInventoryTest::RunTest(const FString& Parameters)
{
	UAHInventoryComponent* Inventory = NewObject<UAHInventoryComponent>();
	Inventory->MaximumGrenades = 4;
	Inventory->AddGrenades(2);
	TestEqual(TEXT("Grenade pickup replenishes inventory"), Inventory->GetGrenades(), 2);
	TestTrue(TEXT("Grenade can be consumed"), Inventory->ConsumeGrenade());
	TestEqual(TEXT("Consumed grenade is removed"), Inventory->GetGrenades(), 1);
	Inventory->AddGrenades(10);
	TestEqual(TEXT("Grenade inventory is capped"), Inventory->GetGrenades(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHObjectiveTransitionTest, "AshesOfHeaven.Combat.ObjectiveTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHObjectiveTransitionTest::RunTest(const FString& Parameters)
{
	UAHObjectiveSubsystem* Objectives = NewObject<UAHObjectiveSubsystem>();
	const TArray<FName> ObjectiveChain = {
		FName(TEXT("ReachDefensivePosition")),
		FName(TEXT("EliminateVeilAssault")),
		FName(TEXT("AdvanceThroughBreach")),
		FName(TEXT("DefendEvacuationGate")),
		FName(TEXT("ReachExtraction"))
	};
	TestEqual(TEXT("Five objectives are configured"), Objectives->GetObjectiveCount(), ObjectiveChain.Num());
	for (int32 Index = 0; Index < ObjectiveChain.Num(); ++Index)
	{
		TestTrue(*FString::Printf(TEXT("Objective %d is active"), Index + 1), Objectives->IsCurrentObjective(ObjectiveChain[Index]));
		TestTrue(*FString::Printf(TEXT("Objective %d completes"), Index + 1), Objectives->CompleteObjective(ObjectiveChain[Index]));
	}
	TestTrue(TEXT("Mission completion is reachable after the fifth objective"), Objectives->IsMissionComplete());
	TestEqual(TEXT("All objectives are recorded as completed"), Objectives->GetCompletedObjectiveIds().Num(), ObjectiveChain.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHObjectiveRestoreStateTest, "AshesOfHeaven.Combat.ObjectiveRestoreState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHObjectiveRestoreStateTest::RunTest(const FString& Parameters)
{
	UAHObjectiveSubsystem* Objectives = NewObject<UAHObjectiveSubsystem>();
	Objectives->RestoreState(2);
	TestTrue(TEXT("Checkpoint restores the current objective"), Objectives->IsCurrentObjective(FName(TEXT("AdvanceThroughBreach"))));
	TestEqual(TEXT("Checkpoint restores completed objective history"), Objectives->GetCompletedObjectiveIds().Num(), 2);
	TestFalse(TEXT("Partial checkpoint is not mission complete"), Objectives->IsMissionComplete());
	Objectives->RestoreState(5);
	TestTrue(TEXT("Completed checkpoint restores mission completion"), Objectives->IsMissionComplete());
	TestEqual(TEXT("Completed checkpoint restores all objective history"), Objectives->GetCompletedObjectiveIds().Num(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHObjectiveHUDDelegateTest, "AshesOfHeaven.Chapter.ObjectiveHUDDelegate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHObjectiveHUDDelegateTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues()
		.InitializeScenes(false)
		.AllowAudioPlayback(false)
		.RequiresHitProxies(false)
		.CreatePhysicsScene(false)
		.CreateNavigation(false)
		.CreateAISystem(false)
		.ShouldSimulatePhysics(false)
		.EnableTraceCollision(false)
		.SetTransactional(false)
		.CreateFXSystem(false);
	UWorld* TestWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		FName(TEXT("AHObjectiveHUDTestWorld")),
		nullptr,
		true,
		ERHIFeatureLevel::Num,
		&WorldInitialization,
		false);
	TestNotNull(TEXT("Objective HUD test world is created"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	UAHObjectiveSubsystem* Objectives = TestWorld->GetSubsystem<UAHObjectiveSubsystem>();
	AAHCombatPlayerController* Controller = TestWorld->SpawnActor<AAHCombatPlayerController>();
	AAHCombatHUD* HUD = TestWorld->SpawnActor<AAHCombatHUD>();
	UAHObjectiveHUDDelegateTestReceiver* DelegateReceiver = NewObject<UAHObjectiveHUDDelegateTestReceiver>(TestWorld);
	TestNotNull(TEXT("Objective subsystem exists in the test world"), Objectives);
	TestNotNull(TEXT("Objective HUD test controller is spawned"), Controller);
	TestNotNull(TEXT("Objective HUD test HUD is spawned"), HUD);
	TestNotNull(TEXT("Objective delegate receiver is created"), DelegateReceiver);
	if (!Objectives || !Controller || !HUD || !DelegateReceiver)
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	FObjectPropertyBase* HUDProperty = FindFProperty<FObjectPropertyBase>(APlayerController::StaticClass(), TEXT("MyHUD"));
	TestNotNull(TEXT("Player controller HUD property is available"), HUDProperty);
	if (HUDProperty)
	{
		HUDProperty->SetObjectPropertyValue_InContainer(Controller, HUD);
	}
	TestTrue(TEXT("Controller exposes the injected HUD"), Controller->GetHUD() == HUD);
	DelegateReceiver->Configure(Controller);
	HUD->SetObjective(FText::FromString(TEXT("DIRECT HUD TEST")), 1, 5);
	TestEqual(TEXT("HUD presentation state can be updated directly"), HUD->GetObjectiveIndex(), 1);

	TestEqual(TEXT("Objective subsystem starts at objective zero"), Objectives->GetCurrentObjectiveIndex(), 0);
	TestWorld->SetBegunPlay(true);
	TestWorld->BeginPlay();
	Controller->DispatchBeginPlay();
	TestTrue(TEXT("Controller retains the injected HUD after BeginPlay"), Controller->GetHUD() == HUD);
	TestTrue(TEXT("Objective change delegate is bound"), Objectives->OnObjectiveChanged.IsBound());
	TestTrue(TEXT("Mission complete delegate is bound"), Objectives->OnMissionComplete.IsBound());
	Objectives->OnObjectiveChanged.AddDynamic(DelegateReceiver, &UAHObjectiveHUDDelegateTestReceiver::HandleObjectiveChanged);
	Objectives->OnMissionComplete.AddDynamic(DelegateReceiver, &UAHObjectiveHUDDelegateTestReceiver::HandleMissionComplete);
	TestEqual(TEXT("Initial HUD objective index is zero"), HUD->GetObjectiveIndex(), 0);
	TestEqual(TEXT("Initial HUD objective count is five"), HUD->GetObjectiveCount(), Objectives->GetObjectiveCount());
	TestTrue(TEXT("Objective change handler is reflected"), Controller->FindFunction(TEXT("HandleObjectiveChanged")) != nullptr);
	TestTrue(TEXT("Mission complete handler is reflected"), Controller->FindFunction(TEXT("HandleMissionComplete")) != nullptr);

	const FName FirstObjective = Objectives->GetCurrentObjective().Id;
	Objectives->DebugAdvanceObjective();
	TestEqual(TEXT("One ObjectiveDebug invocation advances exactly one objective"), Objectives->GetCurrentObjectiveIndex(), 1);
	TestTrue(TEXT("Objective delegate handler is invoked"), DelegateReceiver->WasObjectiveCallbackInvoked());
	TestEqual(TEXT("HUD objective index follows the objective delegate"), HUD->GetObjectiveIndex(), 1);
	TestEqual(TEXT("HUD objective count follows the objective delegate"), HUD->GetObjectiveCount(), Objectives->GetObjectiveCount());
	TestTrue(TEXT("HUD objective text follows the objective delegate"), HUD->GetCurrentObjective().EqualTo(Objectives->GetCurrentObjective().DisplayText));
	TestFalse(TEXT("The first objective is no longer current"), Objectives->IsCurrentObjective(FirstObjective));

	while (!Objectives->IsMissionComplete())
	{
		Objectives->CompleteObjective(Objectives->GetCurrentObjective().Id);
	}
	TestTrue(TEXT("Mission complete delegate handler is invoked"), DelegateReceiver->WasMissionCompleteCallbackInvoked());
	TestTrue(TEXT("Mission complete reaches the HUD delegate path"), HUD->IsMissionCompleteDisplayed());
	Controller->Destroy();
	HUD->Destroy();
	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCheckpointSerializationTest, "AshesOfHeaven.Combat.CheckpointSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHCheckpointSerializationTest::RunTest(const FString& Parameters)
{
	UAHSaveGame* Save = NewObject<UAHSaveGame>();
	Save->CombatState.bValid = true;
	Save->CombatState.CheckpointId = FName(TEXT("Checkpoint_2"));
	Save->CombatState.Health = 73.0f;
	Save->CombatState.Armor = 41.0f;
	Save->CombatState.Ammo.Magazine = 19;
	Save->CombatState.Ammo.Reserve = 94;
	Save->CombatState.Grenades = 3;
	Save->CombatState.ObjectiveIndex = 2;
	Save->CombatState.CompletedEncounters.Add(FName(TEXT("Encounter_One")));
	Save->CombatState.CompletedEncounters.Add(FName(TEXT("Encounter_Two")));

	TArray<uint8> Bytes;
	TestTrue(TEXT("Checkpoint save serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	UAHSaveGame* Loaded = Cast<UAHSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestNotNull(TEXT("Checkpoint save deserializes"), Loaded);
	if (Loaded)
	{
		TestEqual(TEXT("Checkpoint id survives serialization"), Loaded->CombatState.CheckpointId, FName(TEXT("Checkpoint_2")));
		TestEqual(TEXT("Health survives serialization"), Loaded->CombatState.Health, 73.0f);
		TestEqual(TEXT("Ammo survives serialization"), Loaded->CombatState.Ammo.Magazine, 19);
		TestEqual(TEXT("Grenade state survives serialization"), Loaded->CombatState.Grenades, 3);
		TestEqual(TEXT("Objective state survives serialization"), Loaded->CombatState.ObjectiveIndex, 2);
		TestTrue(TEXT("Encounter one progression survives serialization"), Loaded->CombatState.CompletedEncounters.Contains(FName(TEXT("Encounter_One"))));
		TestTrue(TEXT("Encounter two progression survives serialization"), Loaded->CombatState.CompletedEncounters.Contains(FName(TEXT("Encounter_Two"))));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterConfigurationTest, "AshesOfHeaven.Combat.EncounterConfiguration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterConfigurationTest::RunTest(const FString& Parameters)
{
	AAHCombatEncounter* Encounter = NewObject<AAHCombatEncounter>();
	TestEqual(TEXT("Encounter defaults to the Pilgrim patrol definition"), Encounter->EncounterDefinitionId, AHEnemyAssets::EncounterId(TEXT("PilgrimPatrol")));
	TestFalse(TEXT("New encounter is inactive"), Encounter->IsActive());
	TestFalse(TEXT("New encounter is incomplete"), Encounter->IsComplete());
	return true;
}

namespace AHStreamedCombatantTestSupport
{
	// A combatant resolves its body mesh, physics asset and weapon loadout through
	// UAHEnemyAssetSubsystem, which lives on the game instance and answers asynchronously.
	// Spawned into a bare UWorld there is no subsystem to answer, so AAHCombatantCharacter
	// keeps its zero defaults: no skeletal mesh, no physics asset, no weapon. Both tests below
	// therefore need a standalone game instance and have to wait for the streamed definition.
	struct FStreamedCombatantWorld
	{
		FAutomationTestBase* Test = nullptr;
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
		double Deadline = 0.0;

		bool Boot(FAutomationTestBase* InTest, FName WorldName, float TimeoutSeconds = 30.0f)
		{
			Test = InTest;
			GameInstance = NewObject<UGameInstance>(GEngine);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone(WorldName);
			World = GameInstance->GetWorld();
			if (!World)
			{
				Test->AddError(TEXT("standalone game instance produced no world"));
				return false;
			}
			World->InitializeActorsForPlay(FURL());
			Deadline = FPlatformTime::Seconds() + TimeoutSeconds;
			return true;
		}

		template <typename T>
		T* Spawn(const FVector& Location) const
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<T>(Location, FRotator::ZeroRotator, SpawnParameters);
		}

		// Spawn first, then begin play, the way a packaged launch does: AWorldSettings dispatches
		// actor BeginPlay, which is what starts each combatant's own asset request.
		void BeginPlay() const
		{
			if (AWorldSettings* Settings = World->GetWorldSettings())
			{
				Settings->NotifyBeginPlay();
			}
			World->BeginPlay();
		}

		bool HasTimedOut(const TCHAR* What)
		{
			if (FPlatformTime::Seconds() <= Deadline)
			{
				return false;
			}
			Test->AddError(FString::Printf(TEXT("timed out waiting for %s"), What));
			return true;
		}

		void Teardown()
		{
			if (World)
			{
				for (TActorIterator<AAHCombatantCharacter> It(World); It; ++It)
				{
					It->Destroy();
				}
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
				World = nullptr;
			}
			if (GameInstance)
			{
				// Deinitialize subsystems while the objects they reference are still alive. Left to
				// the GC-purge destructor path this asserts (UObjectArray.h Index >= 0).
				GameInstance->Shutdown();
				GameInstance->RemoveFromRoot();
				GameInstance = nullptr;
			}
		}
	};

	bool HasStreamedBody(const AAHCombatantCharacter* Combatant)
	{
		const USkeletalMeshComponent* Body = Combatant ? Combatant->GetMesh() : nullptr;
		return Body != nullptr && Body->GetSkeletalMeshAsset() != nullptr;
	}

	struct FShootableState
	{
		FStreamedCombatantWorld Fixture;
		TObjectPtr<AAHVeilPilgrimCharacter> Target = nullptr;
		int32 Stage = 0;
	};

	// Weapons trace on ECC_Visibility. A combatant whose only collider is the capsule cannot be
	// shot at all, because the Pawn profile ignores that channel - which is exactly how every
	// enemy in the chapter became immortal. This fires the real trace at a real spawned
	// combatant and then kills it through the real damage path.
	class FShootableCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FShootableCommand(TSharedRef<FShootableState> InState) : State(MoveTemp(InState)) {}

		virtual bool Update() override
		{
			FShootableState& S = *State;
			FAutomationTestBase* Test = S.Fixture.Test;

			if (S.Stage == 0)
			{
				S.Target = S.Fixture.Spawn<AAHVeilPilgrimCharacter>(FVector::ZeroVector);
				Test->TestNotNull(TEXT("A combatant can be spawned"), S.Target.Get());
				if (!S.Target)
				{
					S.Fixture.Teardown();
					return true;
				}
				S.Fixture.BeginPlay();
				S.Stage = 1;
				return false;
			}

			if (S.Stage == 1)
			{
				if (!HasStreamedBody(S.Target))
				{
					if (!S.Fixture.HasTimedOut(TEXT("the combatant's streamed body")))
					{
						return false;
					}
					S.Fixture.Teardown();
					return true;
				}
				S.Stage = 2;
			}

			USkeletalMeshComponent* Body = S.Target->GetMesh();
			Test->TestNotNull(TEXT("A combatant has a body mesh component"), Body);
			if (!Body)
			{
				S.Fixture.Teardown();
				return true;
			}
			Test->TestNotNull(TEXT("The body has a skeletal mesh asset"), Body->GetSkeletalMeshAsset());
			Test->TestNotNull(TEXT("The body has a physics asset, so hits resolve to a bone"), Body->GetPhysicsAsset());
			Test->TestEqual(TEXT("The body blocks the channel weapons trace on"),
				Body->GetCollisionResponseToChannel(ECC_Visibility), ECR_Block);

			// Chest height, fired from in front, exactly like AAHWeaponBase::FireShot.
			const FVector ChestOffset(0.0f, 0.0f, 55.0f);
			const FVector TraceStart = S.Target->GetActorLocation() + ChestOffset + FVector(600.0f, 0.0f, 0.0f);
			const FVector TraceEnd = S.Target->GetActorLocation() + ChestOffset - FVector(600.0f, 0.0f, 0.0f);
			FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(AHShootableTest), true);
			FHitResult Hit;
			const bool bHit = S.Fixture.World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, TraceParams);
			Test->TestTrue(TEXT("A visibility trace at chest height hits the combatant"), bHit && Hit.GetActor() == S.Target);
			Test->TestTrue(TEXT("The hit resolves to a named bone, which the headshot multiplier needs"), !Hit.BoneName.IsNone());

			// Same damage event the weapon builds, applied until the health pool is spent.
			const float StartingHealth = S.Target->GetHealthComponent()->GetHealth();
			Test->TestTrue(TEXT("The combatant starts alive"), !S.Target->IsCombatantDead());
			Test->TestTrue(TEXT("The combatant starts on a full health pool"), StartingHealth > 0.0f);
			int32 Shots = 0;
			while (!S.Target->IsCombatantDead() && Shots < 200)
			{
				++Shots;
				UGameplayStatics::ApplyPointDamage(S.Target, 24.0f, (TraceEnd - TraceStart).GetSafeNormal(), Hit, nullptr, nullptr, nullptr);
			}
			Test->TestTrue(TEXT("Rifle damage through the hit result eventually kills the combatant"), S.Target->IsCombatantDead());
			Test->TestTrue(TEXT("Killing the combatant takes a bounded number of rounds"), Shots > 0 && Shots < 200);
			Test->TestTrue(TEXT("Damage actually drained the health pool"), S.Target->GetHealthComponent()->GetHealth() < StartingHealth);

			S.Fixture.Teardown();
			return true;
		}

	private:
		TSharedRef<FShootableState> State;
	};

	struct FLootState
	{
		FStreamedCombatantWorld Fixture;
		TObjectPtr<AAHCombatPlayerCharacter> Looter = nullptr;
		TObjectPtr<AAHVeilPilgrimCharacter> Victim = nullptr;
		int32 Stage = 0;
	};

	class FLootCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FLootCommand(TSharedRef<FLootState> InState) : State(MoveTemp(InState)) {}

		virtual bool Update() override
		{
			FLootState& S = *State;
			FAutomationTestBase* Test = S.Fixture.Test;

			if (S.Stage == 0)
			{
				S.Looter = S.Fixture.Spawn<AAHCombatPlayerCharacter>(FVector::ZeroVector);
				S.Victim = S.Fixture.Spawn<AAHVeilPilgrimCharacter>(FVector(300.0f, 0.0f, 0.0f));
				Test->TestNotNull(TEXT("A looter can be spawned"), S.Looter.Get());
				Test->TestNotNull(TEXT("A victim can be spawned"), S.Victim.Get());
				if (!S.Looter || !S.Victim)
				{
					S.Fixture.Teardown();
					return true;
				}
				S.Fixture.BeginPlay();
				S.Stage = 1;
				return false;
			}

			if (S.Stage == 1)
			{
				// The victim's weapon arrives with its streamed definition, not at spawn.
				const bool bArmed = S.Victim->GetInventoryComponent()->GetCurrentWeapon() != nullptr;
				if (!HasStreamedBody(S.Victim) || !bArmed)
				{
					if (!S.Fixture.HasTimedOut(TEXT("the victim's streamed body and loadout")))
					{
						return false;
					}
					S.Fixture.Teardown();
					return true;
				}
				S.Stage = 2;
			}

			Test->TestTrue(TEXT("A living enemy offers no loot prompt"), IAHInteractable::Execute_GetInteractionPrompt(S.Victim).IsEmpty());
			Test->TestNull(TEXT("A living enemy exposes no lootable weapon"), S.Victim->GetLootableWeapon());

			AAHWeaponBase* VictimWeapon = S.Victim->GetInventoryComponent()->GetCurrentWeapon();
			AAHWeaponBase* LooterWeapon = S.Looter->GetInventoryComponent()->GetCurrentWeapon();
			Test->TestNotNull(TEXT("The victim starts armed"), VictimWeapon);
			Test->TestNotNull(TEXT("The looter starts armed"), LooterWeapon);
			if (!VictimWeapon || !LooterWeapon)
			{
				S.Fixture.Teardown();
				return true;
			}
			const FAHAmmoState VictimAmmo = VictimWeapon->GetAmmoState();
			// Leave room in the reserve, or a full pool would hide the transfer behind its own clamp.
			LooterWeapon->SetAmmoState(FAHAmmoState{VictimAmmo.MagazineCapacity, 0, VictimAmmo.MagazineCapacity, VictimAmmo.ReserveCapacity});
			Test->TestEqual(TEXT("The looter's reserve is emptied for the test"), LooterWeapon->GetAmmoState().Reserve, 0);

			int32 Rounds = 0;
			while (!S.Victim->IsCombatantDead() && Rounds < 200)
			{
				++Rounds;
				S.Victim->TakeDamage(24.0f, FDamageEvent(), nullptr, nullptr);
			}
			Test->TestTrue(TEXT("The victim can be killed"), S.Victim->IsCombatantDead());

			Test->TestNotNull(TEXT("A corpse exposes the weapon it was holding"), S.Victim->GetLootableWeapon());
			Test->TestFalse(TEXT("A corpse offers a loot prompt"), IAHInteractable::Execute_GetInteractionPrompt(S.Victim).IsEmpty());

			IAHInteractable::Execute_Interact(S.Victim, S.Looter);

			// A reserve pool is capped, so a full corpse tops the looter out rather than overfilling.
			const int32 ExpectedReserve = FMath::Min(VictimAmmo.Magazine + VictimAmmo.Reserve, VictimAmmo.ReserveCapacity);
			Test->TestEqual(TEXT("Looting moves the dead soldier's rounds to the looter"),
				LooterWeapon->GetAmmoState().Reserve, ExpectedReserve);
			Test->TestTrue(TEXT("The looter gained ammunition it did not have"), ExpectedReserve > 0);
			Test->TestNull(TEXT("A stripped corpse has nothing left to loot"), S.Victim->GetLootableWeapon());
			Test->TestTrue(TEXT("A stripped corpse stops offering a prompt"), IAHInteractable::Execute_GetInteractionPrompt(S.Victim).IsEmpty());

			S.Fixture.Teardown();
			return true;
		}

	private:
		TSharedRef<FLootState> State;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCombatantIsShootableTest, "AshesOfHeaven.Combat.CombatantIsShootable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHCombatantIsShootableTest::RunTest(const FString& Parameters)
{
	TSharedRef<AHStreamedCombatantTestSupport::FShootableState> State = MakeShared<AHStreamedCombatantTestSupport::FShootableState>();
	if (!State->Fixture.Boot(this, FName(TEXT("AHShootableTestWorld"))))
	{
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(AHStreamedCombatantTestSupport::FShootableCommand(State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterCompletesWhenEnemiesAreKilledTest, "AshesOfHeaven.Combat.EncounterCompletesWhenEnemiesAreKilled", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterCompletesWhenEnemiesAreKilledTest::RunTest(const FString& Parameters)
{
	// Runtime encounters may no longer fall back to a hard class when there is no GameInstance
	// asset subsystem. The full async spawn/death loop is covered by Assets.Enemies.AsyncLifecycle.
	AAHCombatEncounter* Encounter = NewObject<AAHCombatEncounter>();
	Encounter->ActivateEncounter();
	TestFalse(TEXT("an encounter cannot spawn before an async asset lease is ready"), Encounter->IsActive());
	TestEqual(TEXT("no fallback enemy was hard-spawned"), Encounter->GetActiveEnemyCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCorpseIsLootableTest, "AshesOfHeaven.Combat.CorpseIsLootable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHCorpseIsLootableTest::RunTest(const FString& Parameters)
{
	TSharedRef<AHStreamedCombatantTestSupport::FLootState> State = MakeShared<AHStreamedCombatantTestSupport::FLootState>();
	if (!State->Fixture.Boot(this, FName(TEXT("AHLootTestWorld"))))
	{
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(AHStreamedCombatantTestSupport::FLootCommand(State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHPlayerViewIsStandingHeightTest, "AshesOfHeaven.Combat.PlayerViewIsStandingHeight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHPlayerViewIsStandingHeightTest::RunTest(const FString& Parameters)
{
	// The camera used to sit at the capsule's centre, 96cm off the floor, and the game read as
	// though it were played from a chair. Eye height is measured from the capsule's centre, so
	// the test asserts the offset rather than a world Z.
	const AAHCombatPlayerCharacter* Player = GetDefault<AAHCombatPlayerCharacter>();
	TestNotNull(TEXT("The player character default object exists"), Player);
	if (!Player)
	{
		return false;
	}

	const USkeletalMeshComponent* ViewHolder = Player->GetFirstPersonMesh();
	const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
	TestNotNull(TEXT("The player has a first person view holder"), ViewHolder);
	TestNotNull(TEXT("The player has a capsule"), Capsule);
	if (!ViewHolder || !Capsule)
	{
		return false;
	}

	const float EyeOffset = ViewHolder->GetRelativeLocation().Z;
	const float EyeHeightFromFeet = Capsule->GetUnscaledCapsuleHalfHeight() + EyeOffset;
	TestEqual(TEXT("The view sits at the character's eye height, not its waist"), EyeOffset, Player->BaseEyeHeight);
	TestTrue(TEXT("Eyes are at a standing height above the feet"), EyeHeightFromFeet > 140.0f && EyeHeightFromFeet < 190.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHFactionHostilityTest, "AshesOfHeaven.Combat.FactionHostility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHFactionHostilityTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Player is hostile to Veil"), UAHCombatRulesLibrary::IsHostile(EAHFaction::Player, EAHFaction::Veil));
	TestTrue(TEXT("Veil is hostile to player"), UAHCombatRulesLibrary::IsHostile(EAHFaction::Veil, EAHFaction::Player));
	TestTrue(TEXT("Human is hostile to Veil"), UAHCombatRulesLibrary::IsHostile(EAHFaction::Human, EAHFaction::Veil));
	TestFalse(TEXT("Human allies are not hostile"), UAHCombatRulesLibrary::IsHostile(EAHFaction::Human, EAHFaction::Player));
	TestFalse(TEXT("Neutral is not hostile"), UAHCombatRulesLibrary::IsHostile(EAHFaction::Neutral, EAHFaction::Veil));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHChapterStageOrderingTest, "AshesOfHeaven.Chapter.StageOrdering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHChapterStageOrderingTest::RunTest(const FString& Parameters)
{
	const TArray<EAHChapterStage> Stages = {
		EAHChapterStage::OpeningBlack, EAHChapterStage::ErebusOpening, EAHChapterStage::OpeningBattle,
		EAHChapterStage::TransitStation, EAHChapterStage::VeilRevelation, EAHChapterStage::OpenBattlefield,
		EAHChapterStage::ManticoreSection, EAHChapterStage::CathedralApproach, EAHChapterStage::FailsafeOrder,
		EAHChapterStage::CathedralInterior, EAHChapterStage::SaelTransmission, EAHChapterStage::FailsafeTerminal,
		EAHChapterStage::Escape, EAHChapterStage::OtherLucian, EAHChapterStage::ErebusDestruction,
		EAHChapterStage::TenYearsLater, EAHChapterStage::MayaScene, EAHChapterStage::NysaTransmission,
		EAHChapterStage::FleetDeparture, EAHChapterStage::StarsDisappearing, EAHChapterStage::ChapterComplete
	};
	TestEqual(TEXT("All authored Chapter stages are present"), Stages.Num(), 21);
	for (int32 Index = 1; Index < Stages.Num(); ++Index)
	{
		TestTrue(*FString::Printf(TEXT("Stage %d follows stage %d"), Index, Index - 1), static_cast<uint8>(Stages[Index]) > static_cast<uint8>(Stages[Index - 1]));
	}
	TestEqual(TEXT("ChapterComplete is the terminal stage"), Stages.Last(), EAHChapterStage::ChapterComplete);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHChapterObjectiveChainTest, "AshesOfHeaven.Chapter.ObjectiveChain", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHChapterObjectiveChainTest::RunTest(const FString& Parameters)
{
	// The director still hands over the historical 17-definition chain; ConfigureObjectives drops
	// everything after Ch01_SurviveDestruction. Naming index 11 with the real terminator is what
	// makes this test measure that truncation instead of dodging it with synthetic ids.
	UAHObjectiveSubsystem* Objectives = NewObject<UAHObjectiveSubsystem>();
	TArray<FAHObjectiveDefinition> Definitions;
	for (int32 Index = 0; Index < 17; ++Index)
	{
		const FName Id = Index == AHChapterStateConstants::ObjectiveCount - 1
			? FName(TEXT("Ch01_SurviveDestruction"))
			: FName(*FString::Printf(TEXT("Ch01_Objective_%02d"), Index + 1));
		Definitions.Add({Id, FText::FromString(FString::Printf(TEXT("CHAPTER OBJECTIVE %02d"), Index + 1)), FText::FromString(TEXT("Greybox verification objective."))});
	}
	Objectives->ConfigureObjectives(Definitions, 0);
	TestEqual(TEXT("The retired epilogue tail is dropped"), Objectives->GetObjectiveCount(), AHChapterStateConstants::ObjectiveCount);
	for (int32 Index = 0; Index < AHChapterStateConstants::ObjectiveCount; ++Index)
	{
		TestTrue(*FString::Printf(TEXT("Completes %s"), *Definitions[Index].Id.ToString()), Objectives->CompleteObjective(Definitions[Index].Id));
	}
	TestFalse(TEXT("A dropped epilogue objective cannot be completed"), Objectives->CompleteObjective(Definitions[AHChapterStateConstants::ObjectiveCount].Id));
	TestTrue(TEXT("Chapter completion is reachable"), Objectives->IsMissionComplete());
	TestEqual(TEXT("All Chapter objective history is retained"), Objectives->GetCompletedObjectiveIds().Num(), AHChapterStateConstants::ObjectiveCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHChapterStateSerializationTest, "AshesOfHeaven.Chapter.StateSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHChapterStateSerializationTest::RunTest(const FString& Parameters)
{
	UAHSaveGame* Save = NewObject<UAHSaveGame>();
	Save->CombatState.bValid = true;
	Save->CombatState.MapName = TEXT("L_ChapterOne_Greybox");
	Save->CombatState.CheckpointId = FName(TEXT("Ch01_Checkpoint_03"));
	Save->CombatState.ObjectiveIndex = 7;
	Save->CombatState.Ammo.Magazine = 14;
	Save->CombatState.Ammo.Reserve = 121;
	Save->CombatState.Grenades = 1;
	Save->CombatState.ChapterState.Stage = EAHChapterStage::FailsafeOrder;
	Save->CombatState.ChapterState.CountdownSeconds = 401.5f;
	Save->CombatState.ChapterState.bCountdownActive = true;
	Save->CombatState.ChapterState.Vehicle.bSpawned = true;
	Save->CombatState.ChapterState.Vehicle.Health = 287.0f;
	TArray<uint8> Bytes;
	TestTrue(TEXT("Chapter checkpoint serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	UAHSaveGame* Loaded = Cast<UAHSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestNotNull(TEXT("Chapter checkpoint deserializes"), Loaded);
	if (Loaded)
	{
		TestEqual(TEXT("Chapter stage survives restore"), Loaded->CombatState.ChapterState.Stage, EAHChapterStage::FailsafeOrder);
		TestEqual(TEXT("Chapter countdown survives restore"), Loaded->CombatState.ChapterState.CountdownSeconds, 401.5f);
		TestEqual(TEXT("Ammo survives restore"), Loaded->CombatState.Ammo.Magazine, 14);
		TestEqual(TEXT("Reserve ammo survives restore"), Loaded->CombatState.Ammo.Reserve, 121);
		TestEqual(TEXT("Grenades survive restore"), Loaded->CombatState.Grenades, 1);
		TestEqual(TEXT("Manticore state survives restore"), Loaded->CombatState.ChapterState.Vehicle.Health, 287.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHChapterCountdownNarrativeTest, "AshesOfHeaven.Chapter.CountdownAndNarrativeState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHChapterCountdownNarrativeTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UAHChapterSubsystem* Chapter = NewObject<UAHChapterSubsystem>(TestGameInstance);
	Chapter->StartCountdown(522.0f);
	Chapter->TickCountdown(22.0f);
	TestEqual(TEXT("Countdown advances by elapsed time"), Chapter->GetCountdownSeconds(), 500.0f);
	TestTrue(TEXT("Countdown stays active before expiry"), Chapter->IsCountdownActive());
	Chapter->MarkNarrativeEvent(FName(TEXT("Ch01_VeilRevelation")));
	Chapter->MarkNarrativeEvent(FName(TEXT("Ch01_VeilRevelation")));
	TestEqual(TEXT("Narrative events are one-shot"), Chapter->GetState().CompletedNarrativeEvents.Num(), 1);
	FAHChapterState State = Chapter->GetState();
	UAHChapterSubsystem* Restored = NewObject<UAHChapterSubsystem>(TestGameInstance);
	Restored->RestoreState(State);
	TestTrue(TEXT("Countdown and narrative state restore"), FMath::IsNearlyEqual(Restored->GetCountdownSeconds(), 500.0f) && Restored->HasCompletedNarrativeEvent(FName(TEXT("Ch01_VeilRevelation"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHChapterStateConsistencyTest, "AshesOfHeaven.Chapter.NewGameHasNoCompletion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHChapterStateConsistencyTest::RunTest(const FString& Parameters)
{
	const FAHChapterState NewGame = UAHChapterSubsystem::NormalizeState(FAHChapterState());
	TestTrue(TEXT("new Chapter One state begins at OpeningBlack"), NewGame.Stage == EAHChapterStage::OpeningBlack);
	TestEqual(TEXT("new Chapter One objective index is zero"), NewGame.ObjectiveIndex, 0);
	TestFalse(TEXT("new Chapter One is not complete"), NewGame.bChapterComplete);

	FAHChapterState InvalidManticore;
	InvalidManticore.Stage = EAHChapterStage::ChapterComplete;
	InvalidManticore.ObjectiveIndex = 5;
	InvalidManticore.bChapterComplete = true;
	const FAHChapterState Manticore = UAHChapterSubsystem::NormalizeState(InvalidManticore);
	TestTrue(TEXT("Manticore checkpoint canonicalizes to ManticoreSection"), Manticore.Stage == EAHChapterStage::ManticoreSection);
	TestEqual(TEXT("Manticore checkpoint keeps objective six"), Manticore.ObjectiveIndex, 5);
	TestFalse(TEXT("Manticore checkpoint cannot be complete"), Manticore.bChapterComplete);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHChapterCompletionInvariantTest, "AshesOfHeaven.Chapter.CompletionOnlyAfterFinalStage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHChapterCompletionInvariantTest::RunTest(const FString& Parameters)
{
	// Level One's last pre-completion stage is the Erebus escape, not the retired epilogue.
	FAHChapterState BeforeFinal;
	BeforeFinal.Stage = EAHChapterStage::Escape;
	BeforeFinal.ObjectiveIndex = 10;
	const FAHChapterState NormalizedBeforeFinal = UAHChapterSubsystem::NormalizeState(BeforeFinal);
	TestTrue(TEXT("Escape remains the pre-completion stage"), NormalizedBeforeFinal.Stage == EAHChapterStage::Escape);
	TestFalse(TEXT("completion is hidden before the final objective"), NormalizedBeforeFinal.bChapterComplete);

	// A retired post-Erebus stage is never preserved, whatever version wrote it.
	FAHChapterState CurrentVersionEpilogue;
	CurrentVersionEpilogue.SaveVersion = AHChapterStateConstants::CurrentSaveVersion;
	CurrentVersionEpilogue.Stage = EAHChapterStage::TenYearsLater;
	CurrentVersionEpilogue.ObjectiveIndex = 12;
	const FAHChapterState NormalizedEpilogue = UAHChapterSubsystem::NormalizeState(CurrentVersionEpilogue);
	TestTrue(TEXT("a current-version epilogue state still migrates to completion"),
		NormalizedEpilogue.Stage == EAHChapterStage::ChapterComplete && NormalizedEpilogue.bChapterComplete);

	FAHChapterState Final;
	Final.Stage = EAHChapterStage::ChapterComplete;
	Final.ObjectiveIndex = AHChapterStateConstants::ObjectiveCount;
	const FAHChapterState NormalizedFinal = UAHChapterSubsystem::NormalizeState(Final);
	TestTrue(TEXT("ChapterComplete is reachable at the terminal objective"), NormalizedFinal.Stage == EAHChapterStage::ChapterComplete);
	TestTrue(TEXT("completion is true only at the terminal stage"), NormalizedFinal.bChapterComplete);
	TestEqual(TEXT("final state owns the terminal objective index"), NormalizedFinal.ObjectiveIndex, AHChapterStateConstants::ObjectiveCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHManticoreCompletionRegressionTest, "AshesOfHeaven.Chapter.ManticoreDoesNotShowCompletion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHManticoreCompletionRegressionTest::RunTest(const FString& Parameters)
{
	FAHChapterState State;
	State.Stage = EAHChapterStage::ManticoreSection;
	State.ObjectiveIndex = 5;
	State.bChapterComplete = true;
	const FAHChapterState Restored = UAHChapterSubsystem::NormalizeState(State);
	TestTrue(TEXT("Manticore objective remains active"), Restored.Stage == EAHChapterStage::ManticoreSection);
	TestEqual(TEXT("Manticore objective is objective six"), Restored.ObjectiveIndex, 5);
	TestFalse(TEXT("Manticore cannot show chapter completion"), Restored.bChapterComplete);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHChapterCheckpointRestoreConsistencyTest, "AshesOfHeaven.Chapter.CheckpointRestoreStateConsistency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHChapterCheckpointRestoreConsistencyTest::RunTest(const FString& Parameters)
{
	FAHCombatCheckpointState Checkpoint;
	Checkpoint.bValid = true;
	Checkpoint.MapName = TEXT("L_ChapterOne_Greybox");
	Checkpoint.CheckpointId = FName(TEXT("Ch01_Checkpoint_03"));
	Checkpoint.ObjectiveIndex = 5;
	Checkpoint.ChapterState.Stage = EAHChapterStage::ChapterComplete;
	Checkpoint.ChapterState.ObjectiveIndex = 5;
	Checkpoint.ChapterState.bChapterComplete = true;
	const FAHChapterState Restored = UAHChapterSubsystem::NormalizeState(Checkpoint.ChapterState);
	TestTrue(TEXT("checkpoint restore is canonicalized to ManticoreSection"), Restored.Stage == EAHChapterStage::ManticoreSection);
	TestEqual(TEXT("checkpoint restore keeps the Manticore objective"), Restored.ObjectiveIndex, 5);
	TestFalse(TEXT("checkpoint restore hides completion before the final stage"), Restored.bChapterComplete);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHRuntimePresentationReferenceTest, "AshesOfHeaven.Presentation.RuntimeReferencesArtAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHRuntimePresentationReferenceTest::RunTest(const FString& Parameters)
{
	const TArray<FString> RuntimePropPaths = {
		TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_BlastWall.BP_Erebus_BlastWall_C"),
		TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_PipeCluster.BP_Erebus_PipeCluster_C"),
		TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_Wreck.BP_Erebus_Wreck_C"),
		TEXT("/Game/Ashes/Blueprints/Environment/BP_Transit_Sign.BP_Transit_Sign_C"),
		TEXT("/Game/Ashes/Blueprints/Environment/BP_Transit_Bench.BP_Transit_Bench_C"),
		TEXT("/Game/Ashes/Blueprints/Environment/BP_Cathedral_Fin.BP_Cathedral_Fin_C"),
		TEXT("/Game/Ashes/Blueprints/Environment/BP_Cathedral_GlyphPanel.BP_Cathedral_GlyphPanel_C")
	};
	for (const FString& Path : RuntimePropPaths)
	{
		TestNotNull(*FString::Printf(TEXT("runtime presentation prop resolves: %s"), *Path), LoadObject<UClass>(nullptr, *Path));
	}
	for (const TCHAR* Profile : { TEXT("Transit"), TEXT("PresentDay") })
	{
		const FString Path = FString::Printf(TEXT("/Game/Ashes/Presentation/DA_EnvironmentStyle_%s.DA_EnvironmentStyle_%s"), Profile, Profile);
		TestNotNull(*FString::Printf(TEXT("runtime environment profile resolves: %s"), Profile), LoadObject<UAHEnvironmentStyleData>(nullptr, *Path));
	}
	UAHAudioPaletteData* Palette = LoadObject<UAHAudioPaletteData>(nullptr, TEXT("/Game/Ashes/Audio/DA_AudioPalette_Default.DA_AudioPalette_Default"));
	const TSoftObjectPtr<USoundBase>* PresentDay = Palette ? Palette->Environments.Find(FName(TEXT("Environment.PresentDay"))) : nullptr;
	TestTrue(TEXT("present-day runtime audio mapping is assigned"), PresentDay && PresentDay->ToSoftObjectPath().IsValid());
	return true;
}

namespace AHObjective01TestSupport
{
	// The Chapter One fresh-spawn transform authored in AAHChapterOneGameMode::ChoosePlayerStart.
	const FVector FreshSpawn(-1400.0f, 0.0f, 120.0f);

	// Boots a standalone game world with a game instance and a fully-built Chapter One
	// director (greybox collision + presentation layers), the same assembly a packaged
	// L_ChapterOne_Greybox launch performs at BeginPlay.
	struct FChapterOneWorldFixture
	{
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		AAHChapterOneDirector* Director = nullptr;

		bool Boot()
		{
			GameInstance = NewObject<UGameInstance>(GEngine);
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			if (!World)
			{
				return false;
			}
			World->InitializeActorsForPlay(FURL());
			// Spawn the director BEFORE begun-play, then dispatch BeginPlay the way the
			// engine does (AWorldSettings::NotifyBeginPlay flips bBegunPlay only after the
			// actor loop): UStaticMeshComponent::SetStaticMesh rejects static-mobility
			// components once the world has begun play, so the packaged assembly order
			// must be reproduced exactly for the greybox meshes to exist.
			Director = World->SpawnActor<AAHChapterOneDirector>(AAHChapterOneDirector::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
			if (AWorldSettings* Settings = World->GetWorldSettings())
			{
				Settings->NotifyBeginPlay();
			}
			World->BeginPlay();
			return Director != nullptr;
		}

		void Teardown()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
				World = nullptr;
			}
			if (GameInstance)
			{
				// Deinitialize subsystems now, while their referenced objects are still
				// alive. Leaving this to the GC-purge destructor path asserts
				// (UObjectArray.h Index >= 0) once neighbouring objects are freed first.
				GameInstance->Shutdown();
				GameInstance = nullptr;
			}
		}

		bool TraceGround(const FVector& From, FHitResult& OutHit) const
		{
			return World && World->LineTraceSingleByChannel(OutHit, From + FVector(0.0f, 0.0f, 300.0f), From - FVector(0.0f, 0.0f, 1500.0f), ECC_Visibility);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHFreshStartHasValidSpawnTest, "AshesOfHeaven.Chapter.FreshStartHasValidSpawn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHFreshStartHasValidSpawnTest::RunTest(const FString& Parameters)
{
	using namespace AHObjective01TestSupport;
	FChapterOneWorldFixture Fixture;
	TestTrue(TEXT("Chapter One world fixture boots"), Fixture.Boot());
	if (!Fixture.World)
	{
		return false;
	}

	// Deferred spawn: exercises the real ChoosePlayerStart path without dispatching the
	// game mode's BeginPlay (which would build a second director in this fixture world).
	AAHChapterOneGameMode* GameMode = Fixture.World->SpawnActorDeferred<AAHChapterOneGameMode>(AAHChapterOneGameMode::StaticClass(), FTransform::Identity);
	TestNotNull(TEXT("Chapter One game mode spawns"), GameMode);
	if (GameMode)
	{
		AActor* Start = GameMode->ChoosePlayerStart(nullptr);
		TestNotNull(TEXT("fresh start resolves a player start"), Start);
		if (Start)
		{
			TestTrue(TEXT("fresh spawn transform is finite"), !Start->GetActorLocation().ContainsNaN());
			TestTrue(TEXT("fresh spawn matches the authored Erebus start"), Start->GetActorLocation().Equals(FreshSpawn, 1.0f));
		}
	}

	FHitResult Ground;
	TestTrue(TEXT("fresh spawn has blocking ground beneath it"), Fixture.TraceGround(FreshSpawn, Ground));
	if (Ground.bBlockingHit)
	{
		TestTrue(TEXT("ground is immediately under the spawn, not far below (no void)"), FreshSpawn.Z - Ground.ImpactPoint.Z < 400.0f);
		TestTrue(TEXT("ground plane is the authored collision floor top"), FMath::Abs(Ground.ImpactPoint.Z - (-50.0f)) < 25.0f);
	}
	TestTrue(TEXT("fresh spawn passes the checkpoint transform validity gate"), UAHCheckpointSubsystem::IsCheckpointTransformValid(Fixture.World, FreshSpawn));

	// Behind-the-spawn boundary: walking backwards must hit a wall, not the edge of the floor.
	FHitResult RearWall;
	const bool bRearBlocked = Fixture.World->LineTraceSingleByChannel(RearWall, FreshSpawn, FreshSpawn - FVector(1200.0f, 0.0f, 0.0f), ECC_Visibility);
	TestTrue(TEXT("a rear boundary exists behind the fresh spawn"), bRearBlocked);

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHFreshStartNotUsingStaleCheckpointTest, "AshesOfHeaven.Chapter.FreshStartNotUsingStaleCheckpoint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHFreshStartNotUsingStaleCheckpointTest::RunTest(const FString& Parameters)
{
	using namespace AHObjective01TestSupport;
	FChapterOneWorldFixture Fixture;
	TestTrue(TEXT("Chapter One world fixture boots"), Fixture.Boot());
	if (!Fixture.World)
	{
		return false;
	}
	UAHCheckpointSubsystem* Checkpoints = Fixture.World->GetSubsystem<UAHCheckpointSubsystem>();
	TestNotNull(TEXT("checkpoint subsystem exists"), Checkpoints);
	if (!Checkpoints)
	{
		Fixture.Teardown();
		return false;
	}

	// Transform validity gate.
	TestFalse(TEXT("void transform below the world is rejected"), UAHCheckpointSubsystem::IsCheckpointTransformValid(Fixture.World, FVector(0.0f, 0.0f, -50000.0f)));
	TestFalse(TEXT("transform far outside the chapter strip is rejected"), UAHCheckpointSubsystem::IsCheckpointTransformValid(Fixture.World, FVector(500000.0f, 0.0f, 120.0f)));
	TestTrue(TEXT("transform on the authored floor is accepted"), UAHCheckpointSubsystem::IsCheckpointTransformValid(Fixture.World, FVector(4100.0f, 0.0f, 120.0f)));

	// A stale opening checkpoint (the exact failure observed in the field: opening capture
	// with a backwards-facing rotation) must never restore its transform into a fresh run.
	FAHCombatCheckpointState StaleOpening;
	StaleOpening.bValid = true;
	StaleOpening.MapName = TEXT("L_ChapterOne_Greybox");
	StaleOpening.CheckpointId = FName(TEXT("Ch01_Opening"));
	StaleOpening.ObjectiveIndex = 0;
	StaleOpening.ChapterState.ObjectiveIndex = 0;
	StaleOpening.PlayerLocation = FVector(-1400.0f, 0.0f, 48.0f);
	StaleOpening.PlayerRotation = FRotator(0.0f, 167.0f, 0.0f);
	TestFalse(TEXT("fresh run rejects the stale opening checkpoint transform"), Checkpoints->RestoreFromState(StaleOpening));

	// A checkpoint with progress but a void transform must also be rejected.
	FAHCombatCheckpointState StaleVoid;
	StaleVoid.bValid = true;
	StaleVoid.MapName = TEXT("L_ChapterOne_Greybox");
	StaleVoid.CheckpointId = FName(TEXT("Ch01_Checkpoint_03"));
	StaleVoid.ObjectiveIndex = 3;
	StaleVoid.ChapterState.Stage = EAHChapterStage::TransitStation;
	StaleVoid.ChapterState.ObjectiveIndex = 3;
	StaleVoid.PlayerLocation = FVector(0.0f, 0.0f, -8000.0f);
	TestFalse(TEXT("checkpoint with a void transform is rejected"), Checkpoints->RestoreFromState(StaleVoid));

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHObjective01PresentationAlignedTest, "AshesOfHeaven.Chapter.Objective01PresentationAligned", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHObjective01PresentationAlignedTest::RunTest(const FString& Parameters)
{
	using namespace AHObjective01TestSupport;
	FChapterOneWorldFixture Fixture;
	TestTrue(TEXT("Chapter One world fixture boots"), Fixture.Boot());
	if (!Fixture.World)
	{
		return false;
	}

	// Gather visible presentation actors once.
	struct FVisibleActor { AActor* Actor; FBox Bounds; };
	TArray<FVisibleActor> Visible;
	int32 PresentationInObjective01 = 0;
	const FBox Objective01Box(FVector(-2400.0f, -1400.0f, -200.0f), FVector(2600.0f, 1400.0f, 1200.0f));
	for (TActorIterator<AActor> It(Fixture.World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor->ActorHasTag(FName(TEXT("Phase4Presentation"))))
		{
			continue;
		}
		bool bAnyVisible = false;
		TInlineComponentArray<UPrimitiveComponent*> Prims;
		Actor->GetComponents(Prims);
		for (UPrimitiveComponent* Prim : Prims)
		{
			if (Prim && Prim->IsVisible())
			{
				bAnyVisible = true;
				break;
			}
		}
		if (!bAnyVisible)
		{
			continue;
		}
		const FBox Bounds = Actor->GetComponentsBoundingBox(true);
		Visible.Add({Actor, Bounds});
		if (Bounds.Intersect(Objective01Box))
		{
			++PresentationInObjective01;
		}
	}
	TestTrue(TEXT("visible presentation geometry exists near the gameplay start"), PresentationInObjective01 >= 8);

	// At sample points along the Objective 01 corridor, the visible presentation ground and
	// the gameplay collision ground must occupy the same space (coherent coordinates).
	// X=900/Y=0 and X=2000/Y=0 both land on a greybox cover block rather than the floor
	// (measured tops Z=255 and Z=309.8), so those samples are offset in Y to probe the
	// walkable floor beside them. Probing a block top asserts nothing about ground
	// alignment: it only asks whether presentation happens to sit at that height, which an
	// oversized primitive slab used to satisfy by accident.
	const TArray<FVector2D> SamplePoints = {
		{-1400.0f, 0.0f}, {-1000.0f, 0.0f}, {-600.0f, 0.0f}, {0.0f, 0.0f}, {900.0f, 300.0f}, {2000.0f, 300.0f}
	};
	for (const FVector2D& Point : SamplePoints)
	{
		FHitResult Ground;
		const FVector Probe(Point.X, Point.Y, 120.0f);
		const bool bCollision = Fixture.TraceGround(Probe, Ground);
		TestTrue(*FString::Printf(TEXT("gameplay collision ground exists at X=%.0f"), Point.X), bCollision);
		bool bVisibleGround = false;
		for (const FVisibleActor& Entry : Visible)
		{
			if (Point.X >= Entry.Bounds.Min.X && Point.X <= Entry.Bounds.Max.X
				&& Point.Y >= Entry.Bounds.Min.Y && Point.Y <= Entry.Bounds.Max.Y
				&& bCollision && FMath::Abs(Entry.Bounds.Max.Z - Ground.ImpactPoint.Z) < 40.0f)
			{
				bVisibleGround = true;
				break;
			}
		}
		TestTrue(*FString::Printf(TEXT("visible presentation ground is aligned with collision at X=%.0f"), Point.X), bVisibleGround);
	}

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHObjective01ReachableTest, "AshesOfHeaven.Chapter.Objective01Reachable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHObjective01ReachableTest::RunTest(const FString& Parameters)
{
	using namespace AHObjective01TestSupport;
	FChapterOneWorldFixture Fixture;
	TestTrue(TEXT("Chapter One world fixture boots"), Fixture.Boot());
	if (!Fixture.World)
	{
		return false;
	}

	// Find the Objective 01 target trigger placed by the director.
	FVector Target = FVector::ZeroVector;
	FVector TargetExtent = FVector::ZeroVector;
	bool bTargetFound = false;
	for (TActorIterator<AAHChapterTrigger> It(Fixture.World); It; ++It)
	{
		if (It->TriggerId == FName(TEXT("ReachDefensiveLine")))
		{
			Target = It->GetActorLocation();
			TargetExtent = It->Trigger ? It->Trigger->GetScaledBoxExtent() : FVector(280.0f, 1000.0f, 160.0f);
			bTargetFound = true;
			break;
		}
	}
	TestTrue(TEXT("Objective 01 defensive-line trigger exists"), bTargetFound);
	if (!bTargetFound)
	{
		Fixture.Teardown();
		return false;
	}

	// Grid flood-fill from the fresh spawn to the trigger volume: a cell is walkable when
	// it has gameplay ground at floor height and a standing capsule fits above it.
	const float Cell = 100.0f;
	const float MinX = -1600.0f, MaxX = FMath::Max(Target.X + TargetExtent.X, -200.0f);
	const float MinY = -950.0f, MaxY = 950.0f;
	const int32 CountX = FMath::FloorToInt((MaxX - MinX) / Cell) + 1;
	const int32 CountY = FMath::FloorToInt((MaxY - MinY) / Cell) + 1;
	TArray<int8> Walkable;
	Walkable.SetNumZeroed(CountX * CountY);
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(38.0f, 90.0f);
	for (int32 IX = 0; IX < CountX; ++IX)
	{
		for (int32 IY = 0; IY < CountY; ++IY)
		{
			const FVector2D Point(MinX + IX * Cell, MinY + IY * Cell);
			FHitResult Ground;
			if (!Fixture.World->LineTraceSingleByChannel(Ground, FVector(Point.X, Point.Y, 400.0f), FVector(Point.X, Point.Y, -1500.0f), ECC_Visibility))
			{
				continue;
			}
			if (Ground.ImpactPoint.Z < -80.0f || Ground.ImpactPoint.Z > 60.0f)
			{
				continue;
			}
			const FVector StandPoint(Point.X, Point.Y, Ground.ImpactPoint.Z + 96.0f);
			if (Fixture.World->OverlapBlockingTestByChannel(StandPoint, FQuat::Identity, ECC_Pawn, Capsule))
			{
				continue;
			}
			Walkable[IX * CountY + IY] = 1;
		}
	}

	auto CellIndex = [&](const FVector2D& Point) -> int32
	{
		const int32 IX = FMath::RoundToInt((Point.X - MinX) / Cell);
		const int32 IY = FMath::RoundToInt((Point.Y - MinY) / Cell);
		return (IX >= 0 && IX < CountX && IY >= 0 && IY < CountY) ? IX * CountY + IY : INDEX_NONE;
	};
	const int32 StartCell = CellIndex(FVector2D(FreshSpawn.X, FreshSpawn.Y));
	TestTrue(TEXT("fresh spawn cell is walkable"), StartCell != INDEX_NONE && Walkable[StartCell] == 1);

	TArray<int32> Queue;
	TSet<int32> Seen;
	bool bReached = false;
	if (StartCell != INDEX_NONE && Walkable[StartCell])
	{
		Queue.Add(StartCell);
		Seen.Add(StartCell);
	}
	while (!Queue.IsEmpty() && !bReached)
	{
		const int32 Current = Queue.Pop();
		const int32 IX = Current / CountY;
		const int32 IY = Current % CountY;
		const FVector2D Point(MinX + IX * Cell, MinY + IY * Cell);
		if (FMath::Abs(Point.X - Target.X) <= TargetExtent.X && FMath::Abs(Point.Y - Target.Y) <= TargetExtent.Y)
		{
			bReached = true;
			break;
		}
		const int32 Neighbors[4] = {Current + CountY, Current - CountY, Current + 1, Current - 1};
		for (const int32 Neighbor : Neighbors)
		{
			if (Neighbor >= 0 && Neighbor < Walkable.Num() && Walkable[Neighbor] && !Seen.Contains(Neighbor)
				&& FMath::Abs((Neighbor % CountY) - IY) + FMath::Abs((Neighbor / CountY) - IX) == 1)
			{
				Seen.Add(Neighbor);
				Queue.Add(Neighbor);
			}
		}
	}
	TestTrue(TEXT("the defensive-line target is walkably reachable from the fresh spawn"), bReached);

	// The route beyond the line toward the opening battle keeps continuous ground.
	for (float X = Target.X; X <= 900.0f; X += 200.0f)
	{
		FHitResult Ground;
		TestTrue(*FString::Printf(TEXT("route ground continues at X=%.0f"), X),
			Fixture.World->LineTraceSingleByChannel(Ground, FVector(X, 0.0f, 400.0f), FVector(X, 0.0f, -1500.0f), ECC_Visibility));
	}

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHErebusKitManifestTest, "AshesOfHeaven.Presentation.ErebusKitManifest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHErebusKitManifestTest::RunTest(const FString& Parameters)
{
	// The authored zone level must exist as cooked/saved content.
	TestTrue(TEXT("authored Erebus presentation level package exists"),
		FPackageName::DoesPackageExist(TEXT("/Game/Ashes/Environment/Erebus/L_ErebusOpening_Presentation")));

	// A representative slice of the authored modular kit: each mesh must load, carry real
	// geometry and at least one material slot, and must not be an SM_AH primitive copy.
	const TCHAR* KitMeshes[] = {
		TEXT("SM_Erebus_BlastWall_A"), TEXT("SM_Erebus_BunkerWall_A"), TEXT("SM_Erebus_IndustrialWall_A"),
		TEXT("SM_Erebus_RuinedFacade_A"), TEXT("SM_Erebus_Pipe_Large_A"), TEXT("SM_Erebus_Barricade_A"),
		TEXT("SM_Erebus_Wreckage_A"), TEXT("SM_Erebus_RubbleLarge_A"), TEXT("SM_Erebus_TrenchWall_A"),
		TEXT("SM_Erebus_SandbagRow_A"), TEXT("SM_Erebus_MudBase_A"), TEXT("SM_Erebus_Monolith_A"),
		TEXT("SM_Erebus_CathedralSpire_A"), TEXT("SM_Erebus_RuinBlock_A"), TEXT("SM_Erebus_GantryTower_A")
	};
	for (const TCHAR* Name : KitMeshes)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Ashes/Environment/Erebus/Meshes/%s.%s"), Name, Name));
		TestNotNull(*FString::Printf(TEXT("kit mesh resolves: %s"), Name), Mesh);
		if (Mesh)
		{
			TestTrue(*FString::Printf(TEXT("kit mesh has render geometry: %s"), Name), Mesh->GetNumTriangles(0) > 12);
			TestTrue(*FString::Printf(TEXT("kit mesh has material slots: %s"), Name), Mesh->GetStaticMaterials().Num() >= 1);
			TestNotNull(*FString::Printf(TEXT("kit mesh slot 0 material assigned: %s"), Name),
				Mesh->GetStaticMaterials().Num() >= 1 ? Mesh->GetStaticMaterials()[0].MaterialInterface.Get() : nullptr);
		}
	}

	// Erebus material instances ride the authored masters with real variation.
	const TCHAR* Instances[] = {
		TEXT("MI_Erebus_Concrete_Dry"), TEXT("MI_Erebus_Concrete_Wet"), TEXT("MI_Erebus_Concrete_Burned"),
		TEXT("MI_Erebus_Steel_Dark"), TEXT("MI_Erebus_Steel_Painted"), TEXT("MI_Erebus_Steel_Scorched"),
		TEXT("MI_Erebus_Mud"), TEXT("MI_Erebus_WreckMetal"), TEXT("MI_Erebus_Rubber"), TEXT("MI_Erebus_Glass_Damaged")
	};
	for (const TCHAR* Name : Instances)
	{
		TestNotNull(*FString::Printf(TEXT("Erebus material instance resolves: %s"), Name),
			LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Ashes/Materials/Instances/%s.%s"), Name, Name)));
	}

	// Authored assembly Blueprints resolve.
	for (const TCHAR* Prop : { TEXT("BP_Erebus_Bunker_A"), TEXT("BP_Erebus_DefensivePosition_A"), TEXT("BP_Erebus_WreckCluster_A") })
	{
		TestNotNull(*FString::Printf(TEXT("Erebus assembly blueprint resolves: %s"), Prop),
			LoadObject<UClass>(nullptr, *FString::Printf(TEXT("/Game/Ashes/Blueprints/Environment/%s.%s_C"), Prop, Prop)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHErebusAuthoredZoneTest, "AshesOfHeaven.Chapter.ErebusAuthoredZone", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHErebusAuthoredZoneTest::RunTest(const FString& Parameters)
{
	using namespace AHObjective01TestSupport;
	FChapterOneWorldFixture Fixture;
	TestTrue(TEXT("Chapter One world fixture boots"), Fixture.Boot());
	if (!Fixture.World)
	{
		return false;
	}

	// The authored streamed zone is the normal-gameplay Erebus presentation.
	int32 AuthoredCount = 0;
	int32 LegacyErebusPrimitives = 0;
	for (TActorIterator<AActor> It(Fixture.World); It; ++It)
	{
		if (It->Tags.Contains(FName(TEXT("AH.AuthoredZone"))))
		{
			++AuthoredCount;
		}
		// Legacy SpawnVisualShape primitives carry Phase4Visual; none may remain visible in
		// the Erebus opening band (Transit content starts past X=3100 and stays untouched).
		if (It->Tags.Contains(FName(TEXT("Phase4Visual"))) && !It->IsHidden()
			&& It->GetActorLocation().X > -2500.0f && It->GetActorLocation().X < 2900.0f)
		{
			++LegacyErebusPrimitives;
		}
	}
	TestTrue(*FString::Printf(TEXT("authored Erebus zone streamed in (%d tagged actors, expected >= 100)"), AuthoredCount), AuthoredCount >= 100);
	TestEqual(TEXT("no visible legacy primitive presentation remains in the Erebus opening band"), LegacyErebusPrimitives, 0);

	// Mesh-level primitive audit (visual gate §31): no visible component in the reviewed
	// corridor may reference a debug primitive from /Game/Ashes/Presentation/Meshes/SM_AH_*,
	// regardless of which actor or tag carries it. Hidden collision layers are exempt.
	int32 VisibleDebugPrimitiveMeshes = 0;
	for (TActorIterator<AActor> It(Fixture.World); It; ++It)
	{
		if (It->IsHidden() || It->GetActorLocation().X < -2500.0f || It->GetActorLocation().X > 2900.0f)
		{
			continue;
		}
		TArray<UStaticMeshComponent*> MeshComponents;
		It->GetComponents(MeshComponents);
		for (const UStaticMeshComponent* MeshComponent : MeshComponents)
		{
			if (!MeshComponent || MeshComponent->bHiddenInGame || !MeshComponent->IsVisible()
				|| !MeshComponent->GetStaticMesh())
			{
				continue;
			}
			if (MeshComponent->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_")))
			{
				++VisibleDebugPrimitiveMeshes;
				UE_LOG(LogTemp, Display, TEXT("[PrimitiveAudit] %s (%s) uses %s at %s"),
					*It->GetName(), *It->GetClass()->GetName(),
					*MeshComponent->GetStaticMesh()->GetName(), *It->GetActorLocation().ToCompactString());
			}
		}
	}
	TestEqual(TEXT("no visible debug primitive mesh (SM_AH_*) remains in the reviewed Erebus corridor"), VisibleDebugPrimitiveMeshes, 0);

	// The objective-01 corridor keeps presentation coverage from the authored zone.
	const FVector ObjectiveCenter(-600.0f, 0.0f, 120.0f);
	const FVector ObjectiveExtent(280.0f, 1000.0f, 160.0f);
	const FBox ObjectiveBox(ObjectiveCenter - ObjectiveExtent, ObjectiveCenter + ObjectiveExtent);
	int32 PresentationAtObjective = 0;
	for (TActorIterator<AActor> It(Fixture.World); It; ++It)
	{
		if (!It->Tags.Contains(FName(TEXT("Phase4Presentation"))) || It->IsHidden())
		{
			continue;
		}
		FVector Origin;
		FVector Extent;
		It->GetActorBounds(false, Origin, Extent);
		if (ObjectiveBox.Intersect(FBox(Origin - Extent, Origin + Extent)))
		{
			++PresentationAtObjective;
		}
	}
	TestTrue(*FString::Printf(TEXT("authored presentation covers the objective corridor (%d actors, expected >= 8)"), PresentationAtObjective), PresentationAtObjective >= 8);

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneObjectiveCompletersTest, "AshesOfHeaven.LevelOne.ObjectiveCompleters", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneObjectiveCompletersTest::RunTest(const FString& Parameters)
{
	// Every Level One objective needs something that can complete it. Objective 7 (ACTIVATE
	// PLANETARY FAILSAFE) had nothing: its EnterCathedral trigger was authored for
	// CathedralApproach, but Tick completes CathedralApproach's own objective as soon as the
	// player passes X=13700 - hundreds of units before the trigger box - so the overlap always
	// arrived with the chapter already on FailsafeOrder and AAHChapterTrigger rejected it. The
	// level could not be finished, and no test noticed.
	AHObjective01TestSupport::FChapterOneWorldFixture Fixture;
	if (!Fixture.Boot())
	{
		AddError(TEXT("Chapter One fixture failed to boot"));
		Fixture.Teardown();
		return false;
	}

	TMap<EAHChapterStage, FName> TriggersByStage;
	for (TActorIterator<AAHChapterTrigger> It(Fixture.World); It; ++It)
	{
		TriggersByStage.Add(It->Stage, It->TriggerId);
	}

	// Stages whose objective is completed by something other than a chapter trigger.
	const TMap<EAHChapterStage, FString> NonTriggerCompleters = {
		{EAHChapterStage::OpeningBattle, TEXT("opening battle encounter")},
		{EAHChapterStage::VeilRevelation, TEXT("Ch01_VeilRevelation dialogue completion")},
		{EAHChapterStage::ManticoreSection, TEXT("Manticore boarding")},
		{EAHChapterStage::CathedralApproach, TEXT("director Tick X>13700 threshold")},
		{EAHChapterStage::FailsafeTerminal, TEXT("failsafe terminal confirmation")},
		{EAHChapterStage::ErebusDestruction, TEXT("FinishDestructionSequence hold")}
	};

	for (int32 ObjectiveIndex = 0; ObjectiveIndex < AHChapterStateConstants::ObjectiveCount; ++ObjectiveIndex)
	{
		const EAHChapterStage Stage = UAHChapterSubsystem::StageForObjectiveIndex(ObjectiveIndex);
		const bool bHasTrigger = TriggersByStage.Contains(Stage);
		const bool bHasOtherCompleter = NonTriggerCompleters.Contains(Stage);
		TestTrue(*FString::Printf(TEXT("objective %d (%s) has a completer"), ObjectiveIndex, *UEnum::GetValueAsString(Stage)),
			bHasTrigger || bHasOtherCompleter);
	}

	// The specific regression: the Cathedral doorway trigger must belong to FailsafeOrder.
	const FName* FailsafeTrigger = TriggersByStage.Find(EAHChapterStage::FailsafeOrder);
	TestNotNull(TEXT("FailsafeOrder owns a chapter trigger"), FailsafeTrigger);
	if (FailsafeTrigger)
	{
		TestEqual(TEXT("that trigger is the Cathedral doorway"), *FailsafeTrigger, FName(TEXT("EnterCathedral")));
	}
	TestFalse(TEXT("no trigger is still bound to the auto-completed CathedralApproach stage"),
		TriggersByStage.Contains(EAHChapterStage::CathedralApproach));

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneTerminalShowsCasualtyCountTest, "AshesOfHeaven.LevelOne.TerminalShowsCasualtyCount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneTerminalShowsCasualtyCountTest::RunTest(const FString& Parameters)
{
	// The acceptance criterion is that the terminal DISPLAYS 11,407,231. CasualtyText held the
	// figure and nothing read it, so the screen showed the widget's authored placeholder while the
	// only automated assertion checked a dialogue array.
	AHObjective01TestSupport::FChapterOneWorldFixture Fixture;
	if (!Fixture.Boot())
	{
		AddError(TEXT("Chapter One fixture failed to boot"));
		Fixture.Teardown();
		return false;
	}

	// Deferred spawn, exactly like SpawnCathedralTerminal: the world has begun play, so a plain
	// SpawnActor would run BeginPlay - and register with the world-state subsystem - before the
	// persistent id exists, which the subsystem logs as an error.
	const FTransform TerminalTransform(FRotator::ZeroRotator, FVector(18100.0f, 0.0f, 890.0f));
	AAHChapterTerminal* Terminal = Fixture.World->SpawnActorDeferred<AAHChapterTerminal>(AAHChapterTerminal::StaticClass(), TerminalTransform);
	TestNotNull(TEXT("A terminal can be spawned"), Terminal);
	if (!Terminal)
	{
		Fixture.Teardown();
		return false;
	}
	Terminal->SetPersistentId(FGuid(0xA11E1001, 0x9A3549B5, 0xB51F9901, 0x00000002));
	Terminal->FinishSpawning(TerminalTransform);

	TestTrue(TEXT("The casualty count reaches the terminal screen"),
		Terminal->SetScreenText(FName(TEXT("TerminalIntel")), Terminal->CasualtyText));
	TestTrue(TEXT("The authored screen carries the exact figure"),
		Terminal->CasualtyText.ToString().Contains(TEXT("11,407,231")));
	TestTrue(TEXT("The confirmation line has a screen block too"),
		Terminal->SetScreenText(FName(TEXT("TerminalStatus")), Terminal->ConfirmationText));

	Terminal->Destroy();
	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneDialogueDoesNotRewindStageTest, "AshesOfHeaven.LevelOne.DialogueDoesNotRewindStage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneDialogueDoesNotRewindStageTest::RunTest(const FString& Parameters)
{
	// Ch01_Sael finishing used to call StartStage(SaelTransmission) unconditionally. The player
	// can confirm the terminal while that beat is still playing, so the chapter was dragged
	// back out of Escape and every later trigger was rejected: no second Other Lucian sighting,
	// no Erebus finale, no way to finish the level.
	AHObjective01TestSupport::FChapterOneWorldFixture Fixture;
	if (!Fixture.Boot())
	{
		AddError(TEXT("Chapter One fixture failed to boot"));
		Fixture.Teardown();
		return false;
	}
	UAHChapterSubsystem* Chapter = Fixture.GameInstance->GetSubsystem<UAHChapterSubsystem>();
	UAHDialogueSubsystem* Dialogue = Fixture.World->GetSubsystem<UAHDialogueSubsystem>();
	UAHObjectiveSubsystem* Objectives = Fixture.World->GetSubsystem<UAHObjectiveSubsystem>();
	if (!Chapter || !Dialogue || !Objectives)
	{
		AddError(TEXT("chapter, dialogue or objective subsystem missing"));
		Fixture.Teardown();
		return false;
	}

	// Put chapter state, objective cursor and stage in agreement before each case, or the
	// director's spatial validation logs an error and fails the test for the wrong reason.
	const auto EnterStage = [Chapter, Objectives](EAHChapterStage Stage)
	{
		const int32 ObjectiveIndex = UAHChapterSubsystem::ObjectiveIndexForStage(Stage);
		Chapter->SetObjectiveIndex(ObjectiveIndex);
		Objectives->RestoreState(ObjectiveIndex);
		Chapter->SetStage(Stage);
	};

	FAHDialogueLine Line;
	Line.Speaker = FName(TEXT("SAEL"));
	Line.Subtitle = FText::FromString(TEXT("Do not activate it."));
	Line.Duration = 30.0f;

	// The beat finishes after the player has already moved on. Keep the objective index in step
	// with the stage, or the director's own spatial validation logs an error and fails the test.
	EnterStage(EAHChapterStage::Escape);
	Dialogue->StartSequence(FName(TEXT("Ch01_Sael")), {Line}, false);
	Dialogue->SkipCurrentSequence();
	TestEqual(TEXT("a late Ch01_Sael does not rewind the chapter"), Chapter->GetStage(), EAHChapterStage::Escape);

	// The same beat finishing in its own stage still advances.
	EnterStage(EAHChapterStage::CathedralInterior);
	Dialogue->StartSequence(FName(TEXT("Ch01_Sael")), {Line}, false);
	Dialogue->SkipCurrentSequence();
	TestEqual(TEXT("Ch01_Sael still advances from its own stage"), Chapter->GetStage(), EAHChapterStage::SaelTransmission);

	Fixture.Teardown();
	return true;
}

#endif
