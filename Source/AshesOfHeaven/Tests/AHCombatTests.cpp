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
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Gameplay/Game/AHCombatPlayerController.h"
#include "Gameplay/UI/AHCombatHUD.h"
#include "Gameplay/UI/AHHUDRootWidget.h"
#include "Gameplay/Audio/AHAudioPaletteData.h"
#include "Tests/AHObjectiveHUDDelegateTestReceiver.h"
#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
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
#include "MetasoundSource.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundBase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "Blueprint/UserWidget.h"

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
	}
	for (const TCHAR* Path : {
		TEXT("/Game/Ashes/Audio/MetaSounds/MS_M91_Fire.MS_M91_Fire"), TEXT("/Game/Ashes/Audio/MetaSounds/MS_M91_Impact.MS_M91_Impact"),
		TEXT("/Game/Ashes/Audio/MetaSounds/MS_Erebus_Ambience.MS_Erebus_Ambience"), TEXT("/Game/Ashes/Audio/MetaSounds/MS_Transit_Ambience.MS_Transit_Ambience"),
		TEXT("/Game/Ashes/Audio/MetaSounds/MS_Cathedral_Ambience.MS_Cathedral_Ambience"), TEXT("/Game/Ashes/Audio/MetaSounds/MS_Manticore_Engine.MS_Manticore_Engine"),
		TEXT("/Game/Ashes/Audio/MetaSounds/MS_UI_Objective.MS_UI_Objective") })
	{
		TestTrue(TEXT("MetaSound presentation asset exists and is a MetaSound source"), LoadObject<UMetaSoundSource>(nullptr, Path) != nullptr);
	}
	const TArray<FString> RawAudioPaths = {
		TEXT("/Game/Ashes/Audio/Raw/SC_M91_Fire.SC_M91_Fire"), TEXT("/Game/Ashes/Audio/Raw/SC_M91_Reload.SC_M91_Reload"),
		TEXT("/Game/Ashes/Audio/Raw/SC_M91_Impact.SC_M91_Impact"), TEXT("/Game/Ashes/Audio/Raw/SC_Erebus_Ambience.SC_Erebus_Ambience"),
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
	USoundCue* M91Cue = LoadObject<USoundCue>(nullptr, TEXT("/Game/Ashes/Audio/Cues/SC_M91_Fire.SC_M91_Fire"));
	TestNotNull(TEXT("M91 SoundCue exists"), M91Cue);
	if (M91Cue)
	{
		TestNotNull(TEXT("M91 SoundCue has an authored node graph"), M91Cue->FirstNode.Get());
		TestTrue(TEXT("M91 SoundCue has concurrency routing"), !M91Cue->bOverrideConcurrency && M91Cue->ConcurrencySet.Num() > 0);
		TestNotNull(TEXT("M91 SoundCue has submix routing"), M91Cue->SoundSubmixObject.Get());
	}
	for (const TCHAR* Path : {
		TEXT("/Game/Ashes/Materials/M_HumanMetal.M_HumanMetal"), TEXT("/Game/Ashes/Materials/M_HumanArmor.M_HumanArmor"), TEXT("/Game/Ashes/Materials/M_Concrete.M_Concrete"),
		TEXT("/Game/Ashes/Materials/M_CathedralMatter.M_CathedralMatter"), TEXT("/Game/Ashes/Materials/M_VeilObsidian.M_VeilObsidian"), TEXT("/Game/Ashes/Materials/M_EmissiveGlyph.M_EmissiveGlyph") })
	{
		TestNotNull(TEXT("authored material exists"), LoadObject<UMaterialInterface>(nullptr, Path));
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
	TestTrue(TEXT("Encounter enemies default to Veil Pilgrims"), Encounter->EnemyClass == AAHVeilPilgrimCharacter::StaticClass());
	TestFalse(TEXT("New encounter is inactive"), Encounter->IsActive());
	TestFalse(TEXT("New encounter is incomplete"), Encounter->IsComplete());
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
	UAHObjectiveSubsystem* Objectives = NewObject<UAHObjectiveSubsystem>();
	TArray<FAHObjectiveDefinition> Definitions;
	for (int32 Index = 0; Index < 17; ++Index)
	{
		Definitions.Add({FName(*FString::Printf(TEXT("Ch01_Objective_%02d"), Index + 1)), FText::FromString(FString::Printf(TEXT("CHAPTER OBJECTIVE %02d"), Index + 1)), FText::FromString(TEXT("Greybox verification objective."))});
	}
	Objectives->ConfigureObjectives(Definitions, 0);
	TestEqual(TEXT("Chapter objective chain contains seventeen objectives"), Objectives->GetObjectiveCount(), 17);
	for (const FAHObjectiveDefinition& Definition : Definitions)
	{
		TestTrue(*FString::Printf(TEXT("Completes %s"), *Definition.Id.ToString()), Objectives->CompleteObjective(Definition.Id));
	}
	TestTrue(TEXT("Chapter completion is reachable"), Objectives->IsMissionComplete());
	TestEqual(TEXT("All Chapter objective history is retained"), Objectives->GetCompletedObjectiveIds().Num(), 17);
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

#endif
