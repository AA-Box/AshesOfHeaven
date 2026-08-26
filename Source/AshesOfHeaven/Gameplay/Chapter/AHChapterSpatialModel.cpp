#include "Gameplay/Chapter/AHChapterTypes.h"

namespace
{
	FAHStageSpatialDefinition Stage(
		EAHChapterStage StageId,
		const TCHAR* Zone,
		const FVector& SafeLocation,
		const FVector& Anchor,
		const FVector& BoundsMin,
		const FVector& BoundsMax,
		float FloorZ,
		const TCHAR* Profile,
		const TCHAR* Objective,
		const TCHAR* TargetId,
		const FVector& TargetLocation,
		const TCHAR* Checkpoint)
	{
		FAHStageSpatialDefinition Definition;
		Definition.Stage = StageId;
		Definition.ZoneId = FName(Zone);
		Definition.SafePlayerLocation = SafeLocation;
		Definition.StageAnchor = Anchor;
		Definition.ExpectedBoundsMin = BoundsMin;
		Definition.ExpectedBoundsMax = BoundsMax;
		Definition.MaxDistanceFromAnchor = FMath::Max(1200.0f, FVector::Dist2D(BoundsMin, BoundsMax) * 0.75f);
		Definition.GameplayFloorZ = FloorZ;
		Definition.EnvironmentProfile = FName(Profile);
		Definition.ObjectiveId = Objective ? FName(Objective) : NAME_None;
		Definition.ObjectiveTargetId = TargetId ? FName(TargetId) : NAME_None;
		Definition.ObjectiveTargetLocation = TargetLocation;
		Definition.CheckpointId = Checkpoint ? FName(Checkpoint) : NAME_None;
		return Definition;
	}

	const TArray<FAHStageSpatialDefinition>& Definitions()
	{
		static const TArray<FAHStageSpatialDefinition> Values = {
			Stage(EAHChapterStage::OpeningBlack, TEXT("Opening"), FVector(-1400.0f, 0.0f, 120.0f), FVector(0.0f, 0.0f, -50.0f), FVector(-2500.0f, -1500.0f, -200.0f), FVector(1200.0f, 1500.0f, 500.0f), -50.0f, TEXT("Erebus"), nullptr, nullptr, FVector::ZeroVector, TEXT("Ch01_Opening")),
			Stage(EAHChapterStage::ErebusOpening, TEXT("ErebusLine"), FVector(-1400.0f, 0.0f, 120.0f), FVector(0.0f, 0.0f, -50.0f), FVector(-2500.0f, -1500.0f, -200.0f), FVector(1800.0f, 1500.0f, 500.0f), -50.0f, TEXT("Erebus"), TEXT("Ch01_ReachDefensiveLine"), TEXT("ReachDefensiveLine"), FVector(-600.0f, 0.0f, 120.0f), TEXT("Ch01_Opening")),
			Stage(EAHChapterStage::OpeningBattle, TEXT("ErebusLine"), FVector(1050.0f, 0.0f, 120.0f), FVector(900.0f, 0.0f, -50.0f), FVector(-400.0f, -1500.0f, -200.0f), FVector(3000.0f, 1500.0f, 500.0f), -50.0f, TEXT("Erebus"), TEXT("Ch01_SurviveOpeningBattle"), nullptr, FVector(1050.0f, 0.0f, 120.0f), TEXT("Ch01_Erebus_Battle")),
			Stage(EAHChapterStage::TransitStation, TEXT("Transit"), FVector(3150.0f, -100.0f, 120.0f), FVector(3500.0f, 0.0f, -50.0f), FVector(2500.0f, -1500.0f, -200.0f), FVector(5200.0f, 1500.0f, 500.0f), -50.0f, TEXT("Transit"), TEXT("Ch01_ReachTransitStation"), TEXT("ReachTransitStation"), FVector(3500.0f, 0.0f, 120.0f), TEXT("Ch01_Transit_Entrance")),
			Stage(EAHChapterStage::VeilRevelation, TEXT("Transit"), FVector(5000.0f, 0.0f, 120.0f), FVector(4700.0f, 0.0f, -50.0f), FVector(3800.0f, -1500.0f, -200.0f), FVector(5600.0f, 1500.0f, 500.0f), -50.0f, TEXT("Transit"), TEXT("Ch01_SurviveVeilRevelation"), nullptr, FVector(5000.0f, 0.0f, 120.0f), TEXT("Ch01_Transit_Entrance")),
			Stage(EAHChapterStage::OpenBattlefield, TEXT("Battlefield"), FVector(6500.0f, 0.0f, 120.0f), FVector(7200.0f, 0.0f, -50.0f), FVector(5200.0f, -2000.0f, -200.0f), FVector(11500.0f, 2000.0f, 600.0f), -50.0f, TEXT("Erebus"), TEXT("Ch01_CrossBattlefield"), TEXT("CrossBattlefield"), FVector(7200.0f, 0.0f, 120.0f), TEXT("Ch01_Battlefield")),
			Stage(EAHChapterStage::ManticoreSection, TEXT("Manticore"), FVector(7900.0f, -280.0f, 120.0f), FVector(8300.0f, -280.0f, -50.0f), FVector(7300.0f, -1500.0f, -200.0f), FVector(12000.0f, 1500.0f, 600.0f), -50.0f, TEXT("Erebus"), TEXT("Ch01_EnterManticore"), TEXT("EnterManticore"), FVector(8300.0f, -280.0f, -12.0f), TEXT("Ch01_Manticore")),
			Stage(EAHChapterStage::CathedralApproach, TEXT("CathedralApproach"), FVector(14000.0f, 0.0f, 890.0f), FVector(14500.0f, 0.0f, 790.0f), FVector(13000.0f, -900.0f, 650.0f), FVector(15500.0f, 900.0f, 1150.0f), 790.0f, TEXT("Cathedral"), TEXT("Ch01_ReachCathedralApproach"), nullptr, FVector(14700.0f, 0.0f, 890.0f), TEXT("Ch01_Cathedral_Approach")),
			Stage(EAHChapterStage::FailsafeOrder, TEXT("CathedralInterior"), FVector(15100.0f, -100.0f, 890.0f), FVector(15100.0f, 0.0f, 790.0f), FVector(14500.0f, -900.0f, 650.0f), FVector(16500.0f, 900.0f, 1150.0f), 790.0f, TEXT("Cathedral"), TEXT("Ch01_ActivatePlanetaryFailsafe"), TEXT("EnterCathedral"), FVector(14700.0f, 0.0f, 890.0f), TEXT("Ch01_Cathedral_Approach")),
			Stage(EAHChapterStage::CathedralInterior, TEXT("CathedralInterior"), FVector(16400.0f, 0.0f, 890.0f), FVector(16800.0f, 0.0f, 790.0f), FVector(15000.0f, -900.0f, 650.0f), FVector(19000.0f, 900.0f, 1150.0f), 790.0f, TEXT("Cathedral"), TEXT("Ch01_ReachTerminal"), TEXT("ReachTerminal"), FVector(18100.0f, 0.0f, 890.0f), TEXT("Ch01_Cathedral_Interior")),
			Stage(EAHChapterStage::SaelTransmission, TEXT("CathedralInterior"), FVector(17000.0f, 0.0f, 890.0f), FVector(17600.0f, 0.0f, 790.0f), FVector(15000.0f, -900.0f, 650.0f), FVector(19000.0f, 900.0f, 1150.0f), 790.0f, TEXT("Cathedral"), TEXT("Ch01_ReachTerminal"), nullptr, FVector(17600.0f, 0.0f, 890.0f), TEXT("Ch01_Cathedral_Interior")),
			Stage(EAHChapterStage::FailsafeTerminal, TEXT("CathedralInterior"), FVector(18100.0f, 0.0f, 890.0f), FVector(18100.0f, 0.0f, 790.0f), FVector(17500.0f, -900.0f, 650.0f), FVector(19800.0f, 900.0f, 1150.0f), 790.0f, TEXT("Cathedral"), TEXT("Ch01_ConfirmFailsafe"), TEXT("ReachTerminal"), FVector(18100.0f, 0.0f, 890.0f), TEXT("Ch01_FailsafeTerminal")),
			Stage(EAHChapterStage::Escape, TEXT("CathedralEscape"), FVector(20100.0f, 0.0f, 890.0f), FVector(21000.0f, 0.0f, 790.0f), FVector(19200.0f, -1400.0f, 650.0f), FVector(25000.0f, 1400.0f, 1150.0f), 790.0f, TEXT("Cathedral"), TEXT("Ch01_EscapeCathedral"), TEXT("EscapeCathedral"), FVector(24400.0f, 0.0f, 890.0f), TEXT("Ch01_Escape")),
			Stage(EAHChapterStage::OtherLucian, TEXT("CathedralEscape"), FVector(21900.0f, 0.0f, 890.0f), FVector(21900.0f, 0.0f, 790.0f), FVector(19200.0f, -1400.0f, 650.0f), FVector(25000.0f, 1400.0f, 1150.0f), 790.0f, TEXT("Cathedral"), TEXT("Ch01_EscapeCathedral"), TEXT("OtherLucian"), FVector(21900.0f, 500.0f, 890.0f), TEXT("Ch01_Escape")),
			Stage(EAHChapterStage::ErebusDestruction, TEXT("CathedralEscape"), FVector(24000.0f, 0.0f, 890.0f), FVector(24000.0f, 0.0f, 790.0f), FVector(22000.0f, -1400.0f, 650.0f), FVector(25000.0f, 1400.0f, 1150.0f), 790.0f, TEXT("Cathedral"), TEXT("Ch01_SurviveDestruction"), nullptr, FVector(24000.0f, 0.0f, 890.0f), TEXT("Ch01_Escape")),
			Stage(EAHChapterStage::TenYearsLater, TEXT("PresentDay"), FVector(29200.0f, 0.0f, 150.0f), FVector(30000.0f, 0.0f, -40.0f), FVector(28500.0f, -1500.0f, -200.0f), FVector(31500.0f, 1500.0f, 600.0f), -40.0f, TEXT("PresentDay"), TEXT("Ch01_MeetMaya"), nullptr, FVector(29780.0f, -240.0f, 120.0f), TEXT("Ch01_PresentDay")),
			Stage(EAHChapterStage::MayaScene, TEXT("PresentDay"), FVector(29700.0f, 0.0f, 150.0f), FVector(30000.0f, 0.0f, -40.0f), FVector(28500.0f, -1500.0f, -200.0f), FVector(31500.0f, 1500.0f, 600.0f), -40.0f, TEXT("PresentDay"), TEXT("Ch01_ReceiveNysa"), nullptr, FVector(30000.0f, 250.0f, 120.0f), TEXT("Ch01_PresentDay")),
			Stage(EAHChapterStage::NysaTransmission, TEXT("PresentDay"), FVector(30000.0f, 0.0f, 150.0f), FVector(30000.0f, 0.0f, -40.0f), FVector(28500.0f, -1500.0f, -200.0f), FVector(31500.0f, 1500.0f, 600.0f), -40.0f, TEXT("PresentDay"), TEXT("Ch01_FleetDeparture"), nullptr, FVector(30000.0f, 0.0f, 150.0f), TEXT("Ch01_PresentDay")),
			Stage(EAHChapterStage::FleetDeparture, TEXT("PresentDay"), FVector(30200.0f, 0.0f, 150.0f), FVector(30000.0f, 0.0f, -40.0f), FVector(28500.0f, -1500.0f, -200.0f), FVector(31500.0f, 1500.0f, 600.0f), -40.0f, TEXT("PresentDay"), TEXT("Ch01_SeeDisappearingStars"), nullptr, FVector(30200.0f, 0.0f, 150.0f), TEXT("Ch01_PresentDay")),
			Stage(EAHChapterStage::StarsDisappearing, TEXT("PresentDay"), FVector(30200.0f, 0.0f, 150.0f), FVector(30000.0f, 0.0f, -40.0f), FVector(28500.0f, -1500.0f, -200.0f), FVector(31500.0f, 1500.0f, 600.0f), -40.0f, TEXT("PresentDay"), TEXT("Ch01_TitleReveal"), nullptr, FVector(30200.0f, 0.0f, 150.0f), TEXT("Ch01_PresentDay")),
			Stage(EAHChapterStage::ChapterComplete, TEXT("PresentDay"), FVector(30200.0f, 0.0f, 150.0f), FVector(30000.0f, 0.0f, -40.0f), FVector(28500.0f, -1500.0f, -200.0f), FVector(31500.0f, 1500.0f, 600.0f), -40.0f, TEXT("PresentDay"), nullptr, nullptr, FVector(30200.0f, 0.0f, 150.0f), TEXT("Ch01_PresentDay"))
		};
		return Values;
	}

	const TArray<FAHCheckpointSpatialDefinition>& Checkpoints()
	{
		static const TArray<FAHCheckpointSpatialDefinition> Values = {
			{FName(TEXT("Ch01_Opening")), EAHChapterStage::OpeningBlack, FName(TEXT("Opening")), FVector(-1400.0f, 0.0f, 120.0f), FRotator::ZeroRotator},
			{FName(TEXT("Ch01_Erebus_Battle")), EAHChapterStage::OpeningBattle, FName(TEXT("ErebusLine")), FVector(1600.0f, 0.0f, 120.0f), FRotator::ZeroRotator},
			{FName(TEXT("Ch01_Transit_Entrance")), EAHChapterStage::TransitStation, FName(TEXT("Transit")), FVector(3500.0f, 0.0f, 120.0f), FRotator::ZeroRotator},
			{FName(TEXT("Ch01_Battlefield")), EAHChapterStage::OpenBattlefield, FName(TEXT("Battlefield")), FVector(6600.0f, 0.0f, 120.0f), FRotator::ZeroRotator},
			{FName(TEXT("Ch01_Manticore")), EAHChapterStage::ManticoreSection, FName(TEXT("Manticore")), FVector(8300.0f, -280.0f, 120.0f), FRotator::ZeroRotator},
			{FName(TEXT("Ch01_Cathedral_Approach")), EAHChapterStage::CathedralApproach, FName(TEXT("CathedralApproach")), FVector(14500.0f, 0.0f, 890.0f), FRotator::ZeroRotator},
			{FName(TEXT("Ch01_Cathedral_Interior")), EAHChapterStage::CathedralInterior, FName(TEXT("CathedralInterior")), FVector(16500.0f, 0.0f, 890.0f), FRotator::ZeroRotator},
			{FName(TEXT("Ch01_FailsafeTerminal")), EAHChapterStage::FailsafeTerminal, FName(TEXT("CathedralInterior")), FVector(18100.0f, 0.0f, 890.0f), FRotator::ZeroRotator},
			{FName(TEXT("Ch01_Escape")), EAHChapterStage::Escape, FName(TEXT("CathedralEscape")), FVector(21500.0f, 0.0f, 890.0f), FRotator::ZeroRotator},
			{FName(TEXT("Ch01_PresentDay")), EAHChapterStage::TenYearsLater, FName(TEXT("PresentDay")), FVector(29200.0f, 0.0f, 150.0f), FRotator::ZeroRotator}
		};
		return Values;
	}
}

namespace AHChapterSpatial
{
	const FAHStageSpatialDefinition& GetStageDefinition(EAHChapterStage StageId)
	{
		for (const FAHStageSpatialDefinition& Definition : Definitions())
		{
			if (Definition.Stage == StageId)
			{
				return Definition;
			}
		}
		return Definitions()[0];
	}

	const TArray<FAHStageSpatialDefinition>& GetStageDefinitions()
	{
		return Definitions();
	}

	const TArray<FAHCheckpointSpatialDefinition>& GetCheckpointDefinitions()
	{
		return Checkpoints();
	}

	const FAHCheckpointSpatialDefinition* FindCheckpointDefinition(FName CheckpointId)
	{
		for (const FAHCheckpointSpatialDefinition& Definition : Checkpoints())
		{
			if (Definition.CheckpointId == CheckpointId)
			{
				return &Definition;
			}
		}
		const FString LegacyId = CheckpointId.ToString();
		if (LegacyId.StartsWith(TEXT("Ch01_Checkpoint_")))
		{
			int32 LegacyIndex = 0;
			if (LexTryParseString(LegacyIndex, *LegacyId.RightChop(16)))
			{
				const int32 Index = FMath::Clamp(LegacyIndex - 1, 0, Checkpoints().Num() - 1);
				return &Checkpoints()[Index];
			}
		}
		return nullptr;
	}
}
