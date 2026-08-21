#include "Player/HSPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/HSGameplayTags.h"
#include "Core/HSLogCategories.h"
#include "Equipment/HSPlayerEquipmentComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Exploration/HSExplorationStateComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Herbalism/HSPlantEffectComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"
#include "Hunting/HSAnimalIntelComponent.h"
#include "Hunting/HSHuntingComponent.h"
#include "Interaction/HSInteractionSensorComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "NPC/HSNPCActor.h"
#include "NPC/HSNPCServiceComponent.h"
#include "Persistence/HSSettlementPersistenceComponent.h"
#include "Player/HSPlayerHealthComponent.h"
#include "Progression/HSPlayerProgressComponent.h"
#include "Resources/HSResourceCarryComponent.h"
#include "Resources/HSResourcePickupActor.h"
#include "Settlement/HSBuildPlacementComponent.h"
#include "Settlement/HSConstructionComponent.h"
#include "Settlement/HSSettlementPlanComponent.h"
#include "Settlement/HSSettlementPopulationComponent.h"
#include "Settlement/HSSettlementUnlockComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "World/HSGameState.h"
#include "World/HSWorldCleanupSubsystem.h"
#include "World/HSWorldTimeComponent.h"

namespace
{
	enum class EHSBuildMenuEntryKind : uint8
	{
		Schema,
		TemporaryShelter
	};

	struct FHSBuildMenuEntryDefinition
	{
		FGameplayTag BuildingTag;
		FText Label;
		FText Category;
		FText Description;
		float RequiredDays = 0.0f;
		EHSBuildMenuEntryKind Kind = EHSBuildMenuEntryKind::Schema;
		FGameplayTag PrerequisiteCompletedBuilding;
		bool bRequiresSettlementClaim = true;
		bool bRequiresBasicHouse = false;
		bool bRequiresUnlock = false;
	};

	const TArray<FHSBuildMenuEntryDefinition>& GetBuildMenuEntries()
	{
		static const TArray<FHSBuildMenuEntryDefinition> Entries = {
			{ TAG_HS_Building_BasicHouse, NSLOCTEXT("HSBuildMenu", "BasicHouse", "Basic House"), NSLOCTEXT("HSBuildMenu", "CategoryHouse", "House"), NSLOCTEXT("HSBuildMenu", "BasicHouseDesc", "Permanent first home. Builder completes it in one day."), 1.0f },
			{ TAG_HS_Building_BasicHouseMinorUpgrade, NSLOCTEXT("HSBuildMenu", "MinorUpgrade", "Minor House Upgrade"), NSLOCTEXT("HSBuildMenu", "CategoryUpgrade", "Upgrade"), NSLOCTEXT("HSBuildMenu", "MinorUpgradeDesc", "Small improvement to the house. Builder time: quarter day."), 0.25f, EHSBuildMenuEntryKind::Schema, TAG_HS_Building_BasicHouse },
			{ FGameplayTag(), NSLOCTEXT("HSBuildMenu", "BasicCampsite", "Basic Campsite"), NSLOCTEXT("HSBuildMenu", "CategoryTemporary", "Temporary"), NSLOCTEXT("HSBuildMenu", "BasicCampsiteDesc", "One-use shelter marker for sleep and time skip."), 0.0f, EHSBuildMenuEntryKind::TemporaryShelter, FGameplayTag(), false },
			{ TAG_HS_Building_BasicHouseMidUpgrade, NSLOCTEXT("HSBuildMenu", "MidUpgrade", "Mid House Upgrade"), NSLOCTEXT("HSBuildMenu", "CategoryUpgrade", "Upgrade"), NSLOCTEXT("HSBuildMenu", "MidUpgradeDesc", "Meaningful house improvement. Builder time: half day."), 0.5f, EHSBuildMenuEntryKind::Schema, TAG_HS_Building_BasicHouseMinorUpgrade },
			{ TAG_HS_Building_BasicHouseMajorUpgrade, NSLOCTEXT("HSBuildMenu", "MajorUpgrade", "Major House Upgrade"), NSLOCTEXT("HSBuildMenu", "CategoryUpgrade", "Upgrade"), NSLOCTEXT("HSBuildMenu", "MajorUpgradeDesc", "Large house improvement. Builder time: one day."), 1.0f, EHSBuildMenuEntryKind::Schema, TAG_HS_Building_BasicHouseMidUpgrade },
			{ TAG_HS_Building_StorageHut, NSLOCTEXT("HSBuildMenu", "StorageHut", "Storage Hut"), NSLOCTEXT("HSBuildMenu", "CategorySettlement", "Settlement"), NSLOCTEXT("HSBuildMenu", "StorageHutDesc", "Support building for future storage and supplies."), 0.5f, EHSBuildMenuEntryKind::Schema, FGameplayTag(), true, true },
			{ TAG_HS_Building_Workshop, NSLOCTEXT("HSBuildMenu", "Workshop", "Workshop"), NSLOCTEXT("HSBuildMenu", "CategorySettlement", "Settlement"), NSLOCTEXT("HSBuildMenu", "WorkshopDesc", "Builder work area for later crafting and upgrades."), 1.0f, EHSBuildMenuEntryKind::Schema, FGameplayTag(), true, true },
			{ TAG_HS_Building_WatchPost, NSLOCTEXT("HSBuildMenu", "WatchPost", "Watch Post"), NSLOCTEXT("HSBuildMenu", "CategoryDefense", "Defense"), NSLOCTEXT("HSBuildMenu", "WatchPostDesc", "Soldier lookout foundation for safer settlement expansion."), 0.5f, EHSBuildMenuEntryKind::Schema, FGameplayTag(), true, true },
			{ TAG_HS_Building_HerbalHut, NSLOCTEXT("HSBuildMenu", "HerbalHut", "Herbal Hut"), NSLOCTEXT("HSBuildMenu", "CategorySettlement", "Settlement"), NSLOCTEXT("HSBuildMenu", "HerbalHutDesc", "Herbalist workplace for plant discovery and remedies."), 0.5f, EHSBuildMenuEntryKind::Schema, FGameplayTag(), true, true },
			{ TAG_HS_Building_BasicDryingRack, NSLOCTEXT("HSBuildMenu", "DryingRack", "Drying Rack"), NSLOCTEXT("HSBuildMenu", "CategorySettlement", "Settlement"), NSLOCTEXT("HSBuildMenu", "DryingRackDesc", "Small processing schema unlocked from settlement crafting."), 0.25f, EHSBuildMenuEntryKind::Schema, FGameplayTag(), true, true, true }
		};
		return Entries;
	}

	const FHSBuildMenuEntryDefinition* GetBuildMenuEntry(int32 EntryIndex)
	{
		const TArray<FHSBuildMenuEntryDefinition>& Entries = GetBuildMenuEntries();
		return Entries.IsValidIndex(EntryIndex) ? &Entries[EntryIndex] : nullptr;
	}
}

AHSPlayerCharacter::AHSPlayerCharacter()
	: PlayerStance(EHSPlayerStance::Standing)
	, StandingWalkSpeed(350.0f)
	, CrouchWalkSpeed(190.0f)
	, ProneMoveSpeed(85.0f)
	, StandingCapsuleHalfHeight(88.0f)
	, CrouchCapsuleHalfHeight(48.0f)
	, ProneCapsuleHalfHeight(36.0f)
	, StandingCameraHeight(64.0f)
	, CrouchCameraHeight(40.0f)
	, ProneCameraHeight(24.0f)
	, bPauseMenuOpen(false)
	, bPauseOptionsOpen(false)
	, bInventoryOpen(false)
	, bBuildMenuOpen(false)
	, SelectedInventoryIndex(0)
	, SelectedBuildMenuIndex(0)
	, GraphicsQualityPreset(0)
	, MouseSensitivityPreset(1)
	, MasterVolumePreset(2)
	, SelectedManualSaveSlotIndex(0)
	, bPerformanceOverlayEnabled(false)
	, bEmergencyLowEndModeEnabled(false)
	, MenuFeedbackDuration(4.0f)
	, MenuFeedbackExpireTime(0.0)
	, bAutosaveSuppressed(false)
{
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = true;

	GetCapsuleComponent()->SetCapsuleHalfHeight(StandingCapsuleHalfHeight);
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = StandingWalkSpeed;
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchWalkSpeed;
	GetCharacterMovement()->JumpZVelocity = 420.0f;
	GetCharacterMovement()->AirControl = 0.25f;
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, StandingCameraHeight));
	FirstPersonCamera->bUsePawnControlRotation = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	BowGripMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BowGripMesh"));
	BowGripMesh->SetupAttachment(FirstPersonCamera);
	BowGripMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BowGripMesh->SetRelativeLocation(FVector(48.0f, -26.0f, -24.0f));
	BowGripMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	BowGripMesh->SetRelativeScale3D(FVector(0.035f, 0.035f, 0.34f));
	BowGripMesh->SetHiddenInGame(true);
	if (CylinderMesh.Succeeded())
	{
		BowGripMesh->SetStaticMesh(CylinderMesh.Object);
	}

	BowUpperLimbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BowUpperLimbMesh"));
	BowUpperLimbMesh->SetupAttachment(FirstPersonCamera);
	BowUpperLimbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BowUpperLimbMesh->SetRelativeLocation(FVector(50.0f, -27.0f, -4.0f));
	BowUpperLimbMesh->SetRelativeRotation(FRotator(-23.0f, 0.0f, 0.0f));
	BowUpperLimbMesh->SetRelativeScale3D(FVector(0.028f, 0.028f, 0.44f));
	BowUpperLimbMesh->SetHiddenInGame(true);
	if (CylinderMesh.Succeeded())
	{
		BowUpperLimbMesh->SetStaticMesh(CylinderMesh.Object);
	}

	BowLowerLimbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BowLowerLimbMesh"));
	BowLowerLimbMesh->SetupAttachment(FirstPersonCamera);
	BowLowerLimbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BowLowerLimbMesh->SetRelativeLocation(FVector(50.0f, -27.0f, -45.0f));
	BowLowerLimbMesh->SetRelativeRotation(FRotator(23.0f, 0.0f, 0.0f));
	BowLowerLimbMesh->SetRelativeScale3D(FVector(0.028f, 0.028f, 0.44f));
	BowLowerLimbMesh->SetHiddenInGame(true);
	if (CylinderMesh.Succeeded())
	{
		BowLowerLimbMesh->SetStaticMesh(CylinderMesh.Object);
	}

	BowStringMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BowStringMesh"));
	BowStringMesh->SetupAttachment(FirstPersonCamera);
	BowStringMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BowStringMesh->SetRelativeLocation(FVector(46.0f, -18.0f, -24.0f));
	BowStringMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	BowStringMesh->SetRelativeScale3D(FVector(0.008f, 0.008f, 0.84f));
	BowStringMesh->SetHiddenInGame(true);
	if (CylinderMesh.Succeeded())
	{
		BowStringMesh->SetStaticMesh(CylinderMesh.Object);
	}

	InteractionSensor = CreateDefaultSubobject<UHSInteractionSensorComponent>(TEXT("InteractionSensor"));
	AnimalIntelComponent = CreateDefaultSubobject<UHSAnimalIntelComponent>(TEXT("AnimalIntelComponent"));
	ExplorationStateComponent = CreateDefaultSubobject<UHSExplorationStateComponent>(TEXT("ExplorationStateComponent"));
	BuildPlacementComponent = CreateDefaultSubobject<UHSBuildPlacementComponent>(TEXT("BuildPlacementComponent"));
	PlantEffectComponent = CreateDefaultSubobject<UHSPlantEffectComponent>(TEXT("PlantEffectComponent"));
	HuntingComponent = CreateDefaultSubobject<UHSHuntingComponent>(TEXT("HuntingComponent"));
	ConstructionComponent = CreateDefaultSubobject<UHSConstructionComponent>(TEXT("ConstructionComponent"));
	SettlementPersistenceComponent = CreateDefaultSubobject<UHSSettlementPersistenceComponent>(TEXT("SettlementPersistenceComponent"));
	PlayerHealthComponent = CreateDefaultSubobject<UHSPlayerHealthComponent>(TEXT("PlayerHealthComponent"));
	PlayerProgressComponent = CreateDefaultSubobject<UHSPlayerProgressComponent>(TEXT("PlayerProgressComponent"));
	ResourceCarryComponent = CreateDefaultSubobject<UHSResourceCarryComponent>(TEXT("ResourceCarryComponent"));
	SettlementPlanComponent = CreateDefaultSubobject<UHSSettlementPlanComponent>(TEXT("SettlementPlanComponent"));
	SettlementUnlockComponent = CreateDefaultSubobject<UHSSettlementUnlockComponent>(TEXT("SettlementUnlockComponent"));
	SettlementPopulationComponent = CreateDefaultSubobject<UHSSettlementPopulationComponent>(TEXT("SettlementPopulationComponent"));
	PlayerEquipmentComponent = CreateDefaultSubobject<UHSPlayerEquipmentComponent>(TEXT("PlayerEquipmentComponent"));
	ResourcePickupActorClass = AHSResourcePickupActor::StaticClass();

	PlayerMappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("PlayerMappingContext"));

	MoveAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveAction"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	PlayerMappingContext->MapKey(MoveAction, EKeys::W).Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(PlayerMappingContext));
	FEnhancedActionKeyMapping& BackwardMapping = PlayerMappingContext->MapKey(MoveAction, EKeys::S);
	BackwardMapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(PlayerMappingContext));
	BackwardMapping.Modifiers.Add(NewObject<UInputModifierNegate>(PlayerMappingContext));
	FEnhancedActionKeyMapping& LeftMapping = PlayerMappingContext->MapKey(MoveAction, EKeys::A);
	LeftMapping.Modifiers.Add(NewObject<UInputModifierNegate>(PlayerMappingContext));
	PlayerMappingContext->MapKey(MoveAction, EKeys::D);
	PlayerMappingContext->MapKey(MoveAction, EKeys::Gamepad_Left2D);

	LookAction = CreateDefaultSubobject<UInputAction>(TEXT("LookAction"));
	LookAction->ValueType = EInputActionValueType::Axis2D;
	PlayerMappingContext->MapKey(LookAction, EKeys::Mouse2D);
	PlayerMappingContext->MapKey(LookAction, EKeys::Gamepad_Right2D);

	InteractAction = CreateDefaultSubobject<UInputAction>(TEXT("InteractAction"));
	InteractAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(InteractAction, EKeys::E);
	PlayerMappingContext->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Bottom);

	HuntAction = CreateDefaultSubobject<UInputAction>(TEXT("HuntAction"));
	HuntAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(HuntAction, EKeys::LeftMouseButton);
	PlayerMappingContext->MapKey(HuntAction, EKeys::Gamepad_RightTrigger);

	JumpAction = CreateDefaultSubobject<UInputAction>(TEXT("JumpAction"));
	JumpAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	PlayerMappingContext->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Top);

	CrouchAction = CreateDefaultSubobject<UInputAction>(TEXT("CrouchAction"));
	CrouchAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(CrouchAction, EKeys::LeftControl);
	PlayerMappingContext->MapKey(CrouchAction, EKeys::Gamepad_LeftThumbstick);

	ProneAction = CreateDefaultSubobject<UInputAction>(TEXT("ProneAction"));
	ProneAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(ProneAction, EKeys::Z);
	PlayerMappingContext->MapKey(ProneAction, EKeys::Gamepad_FaceButton_Right);

	BuildPlacementAction = CreateDefaultSubobject<UInputAction>(TEXT("BuildPlacementAction"));
	BuildPlacementAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(BuildPlacementAction, EKeys::B);
	PlayerMappingContext->MapKey(BuildPlacementAction, EKeys::Gamepad_Special_Right);

	InventoryAction = CreateDefaultSubobject<UInputAction>(TEXT("InventoryAction"));
	InventoryAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(InventoryAction, EKeys::I);
	PlayerMappingContext->MapKey(InventoryAction, EKeys::Tab);

	RotateBuildLeftAction = CreateDefaultSubobject<UInputAction>(TEXT("RotateBuildLeftAction"));
	RotateBuildLeftAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(RotateBuildLeftAction, EKeys::Q);

	RotateBuildRightAction = CreateDefaultSubobject<UInputAction>(TEXT("RotateBuildRightAction"));
	RotateBuildRightAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(RotateBuildRightAction, EKeys::R);

	EscapeAction = CreateDefaultSubobject<UInputAction>(TEXT("EscapeAction"));
	EscapeAction->ValueType = EInputActionValueType::Boolean;
	EscapeAction->bTriggerWhenPaused = true;
	PlayerMappingContext->MapKey(EscapeAction, EKeys::Escape);

	PauseMenuResumeAction = CreateDefaultSubobject<UInputAction>(TEXT("PauseMenuResumeAction"));
	PauseMenuResumeAction->ValueType = EInputActionValueType::Boolean;
	PauseMenuResumeAction->bTriggerWhenPaused = true;
	PlayerMappingContext->MapKey(PauseMenuResumeAction, EKeys::One);

	PauseMenuSaveAction = CreateDefaultSubobject<UInputAction>(TEXT("PauseMenuSaveAction"));
	PauseMenuSaveAction->ValueType = EInputActionValueType::Boolean;
	PauseMenuSaveAction->bTriggerWhenPaused = true;
	PlayerMappingContext->MapKey(PauseMenuSaveAction, EKeys::Two);

	PauseMenuLoadAction = CreateDefaultSubobject<UInputAction>(TEXT("PauseMenuLoadAction"));
	PauseMenuLoadAction->ValueType = EInputActionValueType::Boolean;
	PauseMenuLoadAction->bTriggerWhenPaused = true;
	PlayerMappingContext->MapKey(PauseMenuLoadAction, EKeys::Three);

	PauseMenuOptionsAction = CreateDefaultSubobject<UInputAction>(TEXT("PauseMenuOptionsAction"));
	PauseMenuOptionsAction->ValueType = EInputActionValueType::Boolean;
	PauseMenuOptionsAction->bTriggerWhenPaused = true;
	PlayerMappingContext->MapKey(PauseMenuOptionsAction, EKeys::Four);

	PauseMenuExitAction = CreateDefaultSubobject<UInputAction>(TEXT("PauseMenuExitAction"));
	PauseMenuExitAction->ValueType = EInputActionValueType::Boolean;
	PauseMenuExitAction->bTriggerWhenPaused = true;
	PlayerMappingContext->MapKey(PauseMenuExitAction, EKeys::Five);

	MenuUtilityAction = CreateDefaultSubobject<UInputAction>(TEXT("MenuUtilityAction"));
	MenuUtilityAction->ValueType = EInputActionValueType::Boolean;
	MenuUtilityAction->bTriggerWhenPaused = true;
	PlayerMappingContext->MapKey(MenuUtilityAction, EKeys::Six);

	MenuSensitivityAction = CreateDefaultSubobject<UInputAction>(TEXT("MenuSensitivityAction"));
	MenuSensitivityAction->ValueType = EInputActionValueType::Boolean;
	MenuSensitivityAction->bTriggerWhenPaused = true;
	PlayerMappingContext->MapKey(MenuSensitivityAction, EKeys::Seven);

	MenuVolumeAction = CreateDefaultSubobject<UInputAction>(TEXT("MenuVolumeAction"));
	MenuVolumeAction->ValueType = EInputActionValueType::Boolean;
	MenuVolumeAction->bTriggerWhenPaused = true;
	PlayerMappingContext->MapKey(MenuVolumeAction, EKeys::Eight);

	MenuSaveSlotAction = CreateDefaultSubobject<UInputAction>(TEXT("MenuSaveSlotAction"));
	MenuSaveSlotAction->ValueType = EInputActionValueType::Boolean;
	MenuSaveSlotAction->bTriggerWhenPaused = true;
	PlayerMappingContext->MapKey(MenuSaveSlotAction, EKeys::Nine);

	PerformanceOverlayAction = CreateDefaultSubobject<UInputAction>(TEXT("PerformanceOverlayAction"));
	PerformanceOverlayAction->ValueType = EInputActionValueType::Boolean;
	PerformanceOverlayAction->bTriggerWhenPaused = true;
	PlayerMappingContext->MapKey(PerformanceOverlayAction, EKeys::F10);

	EmergencyLowEndModeAction = CreateDefaultSubobject<UInputAction>(TEXT("EmergencyLowEndModeAction"));
	EmergencyLowEndModeAction->ValueType = EInputActionValueType::Boolean;
	EmergencyLowEndModeAction->bTriggerWhenPaused = true;
	PlayerMappingContext->MapKey(EmergencyLowEndModeAction, EKeys::F8);

	EquipmentAction = CreateDefaultSubobject<UInputAction>(TEXT("EquipmentAction"));
	EquipmentAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(EquipmentAction, EKeys::G);

	SaveAction = CreateDefaultSubobject<UInputAction>(TEXT("SaveAction"));
	SaveAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(SaveAction, EKeys::F5);

	LoadAction = CreateDefaultSubobject<UInputAction>(TEXT("LoadAction"));
	LoadAction->ValueType = EInputActionValueType::Boolean;
	PlayerMappingContext->MapKey(LoadAction, EKeys::F9);
}

void AHSPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (PlantEffectComponent)
	{
		PlantEffectComponent->OnPlantEffectApplied.AddUniqueDynamic(this, &AHSPlayerCharacter::HandlePlantEffectMovementChanged);
		PlantEffectComponent->OnPlantEffectExpired.AddUniqueDynamic(this, &AHSPlayerCharacter::HandlePlantEffectMovementChanged);
	}

	if (PlayerEquipmentComponent)
	{
		PlayerEquipmentComponent->OnBowEquippedChanged.AddUniqueDynamic(this, &AHSPlayerCharacter::HandleBowEquippedChanged);
	}

	RefreshBowVisuals();
	if (SettlementPopulationComponent)
	{
		SettlementPopulationComponent->RecruitNPC(TEXT("Hunter"), TAG_HS_NPC_Profession_Hunter);
		SettlementPopulationComponent->RecruitNPC(TEXT("Builder"), TAG_HS_NPC_Profession_Builder);
		SettlementPopulationComponent->RecruitNPC(TEXT("Herbalist"), TAG_HS_NPC_Profession_Herbalist);
		SettlementPopulationComponent->RecruitNPC(TEXT("Soldier"), TAG_HS_NPC_Profession_Soldier);
	}
	BindAutosaveSources();
}

void AHSPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	ApplyInputMappingContext();
}

void AHSPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHSPlayerCharacter::Move);
	EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHSPlayerCharacter::Look);
	EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::Interact);
	EnhancedInputComponent->BindAction(HuntAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::Hunt);
	EnhancedInputComponent->BindAction(HuntAction, ETriggerEvent::Completed, this, &AHSPlayerCharacter::ReleaseHunt);
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::StartJump);
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AHSPlayerCharacter::StopJump);
	EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::ToggleCrouchStance);
	EnhancedInputComponent->BindAction(ProneAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::ToggleProneStance);
	EnhancedInputComponent->BindAction(BuildPlacementAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::ToggleBuildPlacement);
	EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::ToggleInventory);
	EnhancedInputComponent->BindAction(RotateBuildLeftAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::RotateBuildPlacementLeft);
	EnhancedInputComponent->BindAction(RotateBuildRightAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::RotateBuildPlacementRight);
	EnhancedInputComponent->BindAction(EscapeAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::HandleEscapeAction);
	EnhancedInputComponent->BindAction(PauseMenuResumeAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::HandlePauseMenuResume);
	EnhancedInputComponent->BindAction(PauseMenuSaveAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::HandlePauseMenuSave);
	EnhancedInputComponent->BindAction(PauseMenuLoadAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::HandlePauseMenuLoad);
	EnhancedInputComponent->BindAction(PauseMenuOptionsAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::HandlePauseMenuOptions);
	EnhancedInputComponent->BindAction(PauseMenuExitAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::HandlePauseMenuExit);
	EnhancedInputComponent->BindAction(MenuUtilityAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::HandleMenuUtilityAction);
	EnhancedInputComponent->BindAction(MenuSensitivityAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::HandleMenuSensitivityAction);
	EnhancedInputComponent->BindAction(MenuVolumeAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::HandleMenuVolumeAction);
	EnhancedInputComponent->BindAction(MenuSaveSlotAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::HandleMenuSaveSlotAction);
	EnhancedInputComponent->BindAction(PerformanceOverlayAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::HandlePerformanceOverlayAction);
	EnhancedInputComponent->BindAction(EmergencyLowEndModeAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::HandleEmergencyLowEndModeAction);
	EnhancedInputComponent->BindAction(EquipmentAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::HandleEquipmentAction);
	EnhancedInputComponent->BindAction(SaveAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::SaveSettlementState);
	EnhancedInputComponent->BindAction(LoadAction, ETriggerEvent::Started, this, &AHSPlayerCharacter::LoadSettlementState);
}

void AHSPlayerCharacter::Move(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen || bInventoryOpen || bBuildMenuOpen)
	{
		return;
	}

	if (PlayerStance == EHSPlayerStance::Prone && GetCharacterMovement()->IsFalling())
	{
		return;
	}

	const FVector2D MovementInput = InputValue.Get<FVector2D>();
	const FRotator ControlRotation = Controller ? Controller->GetControlRotation() : FRotator::ZeroRotator;
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), MovementInput.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), MovementInput.X);
}

void AHSPlayerCharacter::Look(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen || bInventoryOpen || bBuildMenuOpen)
	{
		return;
	}

	const FVector2D LookInput = InputValue.Get<FVector2D>() * GetMouseSensitivityScale();
	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void AHSPlayerCharacter::Interact(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen || bInventoryOpen || bBuildMenuOpen)
	{
		return;
	}

	if (BuildPlacementComponent && BuildPlacementComponent->IsPlacementActive())
	{
		BuildPlacementComponent->ConfirmPlacement();
		return;
	}

	if (InteractionSensor)
	{
		InteractionSensor->TryInteract();
	}
	else
	{
		UE_LOG(LogHSInteraction, Warning, TEXT("%s has no interaction sensor."), *GetNameSafe(this));
	}
}

void AHSPlayerCharacter::Hunt(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen || bInventoryOpen || bBuildMenuOpen)
	{
		return;
	}

	if (BuildPlacementComponent && BuildPlacementComponent->IsPlacementActive())
	{
		BuildPlacementComponent->ConfirmPlacement();
		return;
	}

	if (HuntingComponent)
	{
		HuntingComponent->StartBowDraw();
	}
	else
	{
		UE_LOG(LogHSAI, Warning, TEXT("%s has no hunting component."), *GetNameSafe(this));
	}
}

void AHSPlayerCharacter::ReleaseHunt(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen || bInventoryOpen || bBuildMenuOpen)
	{
		return;
	}

	if (BuildPlacementComponent && BuildPlacementComponent->IsPlacementActive())
	{
		return;
	}

	if (HuntingComponent)
	{
		HuntingComponent->ReleaseBowShot();
	}
}

void AHSPlayerCharacter::StartJump(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen || bInventoryOpen || bBuildMenuOpen)
	{
		return;
	}

	if (PlayerStance == EHSPlayerStance::Prone)
	{
		SetPlayerStance(EHSPlayerStance::Standing);
	}

	Jump();
}

void AHSPlayerCharacter::StopJump(const FInputActionValue& InputValue)
{
	StopJumping();
}

void AHSPlayerCharacter::ToggleCrouchStance(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen || bInventoryOpen || bBuildMenuOpen)
	{
		return;
	}

	SetPlayerStance(PlayerStance == EHSPlayerStance::Crouching ? EHSPlayerStance::Standing : EHSPlayerStance::Crouching);
}

void AHSPlayerCharacter::ToggleProneStance(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen || bInventoryOpen || bBuildMenuOpen)
	{
		return;
	}

	SetPlayerStance(PlayerStance == EHSPlayerStance::Prone ? EHSPlayerStance::Standing : EHSPlayerStance::Prone);
}

void AHSPlayerCharacter::ToggleBuildPlacement(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen || bInventoryOpen)
	{
		return;
	}

	if (!BuildPlacementComponent)
	{
		return;
	}

	if (BuildPlacementComponent->IsPlacementActive())
	{
		BuildPlacementComponent->CancelPlacement();
	}
	else
	{
		ToggleBuildMenu();
	}
}

void AHSPlayerCharacter::RotateBuildPlacementLeft(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen)
	{
		return;
	}

	if (bBuildMenuOpen)
	{
		SelectBuildMenuDelta(-1);
		return;
	}

	if (bInventoryOpen)
	{
		SelectInventoryDelta(-1);
		return;
	}

	if ((!BuildPlacementComponent || !BuildPlacementComponent->IsPlacementActive()) && SelectFocusedNPCServiceDelta(-1))
	{
		return;
	}

	if (BuildPlacementComponent)
	{
		BuildPlacementComponent->RotatePlacement(-1.0f);
	}
}

void AHSPlayerCharacter::RotateBuildPlacementRight(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen)
	{
		return;
	}

	if (bBuildMenuOpen)
	{
		SelectBuildMenuDelta(1);
		return;
	}

	if (bInventoryOpen)
	{
		SelectInventoryDelta(1);
		return;
	}

	if ((!BuildPlacementComponent || !BuildPlacementComponent->IsPlacementActive()) && SelectFocusedNPCServiceDelta(1))
	{
		return;
	}

	if (BuildPlacementComponent)
	{
		BuildPlacementComponent->RotatePlacement(1.0f);
	}
}

void AHSPlayerCharacter::ToggleInventory(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen || (BuildPlacementComponent && BuildPlacementComponent->IsPlacementActive()))
	{
		return;
	}

	ToggleInventoryMenu();
}

void AHSPlayerCharacter::HandleEscapeAction(const FInputActionValue& InputValue)
{
	if (BuildPlacementComponent && BuildPlacementComponent->IsPlacementActive())
	{
		BuildPlacementComponent->CancelPlacement();
		return;
	}

	if (bInventoryOpen)
	{
		SetInventoryOpen(false);
		return;
	}

	if (bBuildMenuOpen)
	{
		SetBuildMenuOpen(false);
		return;
	}

	SetPauseMenuOpen(!bPauseMenuOpen);
}

void AHSPlayerCharacter::HandlePauseMenuResume(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen)
	{
		ClosePauseMenu();
		return;
	}

	if (bBuildMenuOpen)
	{
		StartBasicHouseFromBuildMenu();
		return;
	}

	if (bInventoryOpen)
	{
		InspectSelectedInventoryStack();
	}
}

void AHSPlayerCharacter::HandlePauseMenuSave(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen)
	{
		SaveFromPauseMenu();
		return;
	}

	if (bInventoryOpen)
	{
		DropSelectedInventoryStackQuantity(1);
		return;
	}

	if (bBuildMenuOpen)
	{
		StartBasicHouseMinorUpgradeFromBuildMenu();
		return;
	}
}

void AHSPlayerCharacter::HandlePauseMenuLoad(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen)
	{
		LoadFromPauseMenu();
		return;
	}

	if (bInventoryOpen)
	{
		DropSelectedInventoryStackQuantity(TNumericLimits<int32>::Max());
		return;
	}

	if (bBuildMenuOpen)
	{
		StartBasicCampsiteFromBuildMenu();
	}
}

void AHSPlayerCharacter::HandlePauseMenuOptions(const FInputActionValue& InputValue)
{
	if (bBuildMenuOpen)
	{
		StartBuildMenuEntry(3);
		return;
	}

	if (bInventoryOpen)
	{
		UseSelectedInventoryStack();
		return;
	}

	if (bPauseMenuOpen)
	{
		TogglePauseOptions();
	}
}

void AHSPlayerCharacter::HandleMenuUtilityAction(const FInputActionValue& InputValue)
{
	if (bBuildMenuOpen)
	{
		StartBuildMenuEntry(5);
		return;
	}

	if (bPauseMenuOpen && bPauseOptionsOpen)
	{
		CycleGraphicsQualityPreset();
	}
}

void AHSPlayerCharacter::HandleMenuSensitivityAction(const FInputActionValue& InputValue)
{
	if (bBuildMenuOpen)
	{
		StartBuildMenuEntry(6);
		return;
	}

	if (bPauseMenuOpen && bPauseOptionsOpen)
	{
		CycleMouseSensitivityPreset();
	}
}

void AHSPlayerCharacter::HandleMenuVolumeAction(const FInputActionValue& InputValue)
{
	if (bBuildMenuOpen)
	{
		StartBuildMenuEntry(7);
		return;
	}

	if (bPauseMenuOpen && bPauseOptionsOpen)
	{
		CycleMasterVolumePreset();
	}
}

void AHSPlayerCharacter::HandleMenuSaveSlotAction(const FInputActionValue& InputValue)
{
	if (bBuildMenuOpen)
	{
		StartBuildMenuEntry(8);
		return;
	}

	if (bPauseMenuOpen && bPauseOptionsOpen)
	{
		CycleManualSaveSlot();
	}
}

void AHSPlayerCharacter::HandlePerformanceOverlayAction(const FInputActionValue& InputValue)
{
	TogglePerformanceOverlay();
}

void AHSPlayerCharacter::HandleEmergencyLowEndModeAction(const FInputActionValue& InputValue)
{
	ToggleEmergencyLowEndMode();
}

void AHSPlayerCharacter::HandleEquipmentAction(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen || bInventoryOpen || bBuildMenuOpen)
	{
		return;
	}

	if (!PlayerEquipmentComponent)
	{
		SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "EquipmentUnavailable", "Equipment is unavailable."));
		return;
	}

	const EHSPlayerWeaponMode PreviousWeaponMode = PlayerEquipmentComponent->GetEquippedWeaponMode();
	if (!PlayerEquipmentComponent->CycleWeaponMode())
	{
		SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "WeaponCycleMissing", "No weapon available."));
		return;
	}

	const EHSPlayerWeaponMode NewWeaponMode = PlayerEquipmentComponent->GetEquippedWeaponMode();
	if (NewWeaponMode == PreviousWeaponMode)
	{
		SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "WeaponUnchanged", "Weapon unchanged."));
		return;
	}

	if (NewWeaponMode == EHSPlayerWeaponMode::Bow)
	{
		SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "BowEquipped", "Bow equipped."));
	}
	else if (NewWeaponMode == EHSPlayerWeaponMode::Spear)
	{
		SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "SpearEquipped", "Spear equipped."));
	}
	else
	{
		SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "WeaponStored", "Weapon stored."));
	}
}

void AHSPlayerCharacter::HandlePauseMenuExit(const FInputActionValue& InputValue)
{
	if (bBuildMenuOpen)
	{
		StartBuildMenuEntry(4);
		return;
	}

	if (bPauseMenuOpen)
	{
		ExitGameFromPauseMenu();
	}
}

void AHSPlayerCharacter::SaveSettlementState(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen)
	{
		return;
	}

	const bool bSaved = SaveFromPauseMenu();
	UE_LOG(LogHSSettlement, Log, TEXT("%s manual settlement save result: %s."), *GetNameSafe(this), bSaved ? TEXT("saved") : TEXT("failed"));
}

void AHSPlayerCharacter::LoadSettlementState(const FInputActionValue& InputValue)
{
	if (bPauseMenuOpen)
	{
		return;
	}

	const bool bLoaded = LoadFromPauseMenu();
	UE_LOG(LogHSSettlement, Log, TEXT("%s manual settlement load result: %s."), *GetNameSafe(this), bLoaded ? TEXT("loaded") : TEXT("failed"));
}

void AHSPlayerCharacter::OpenPauseMenu()
{
	SetPauseMenuOpen(true);
}

void AHSPlayerCharacter::ClosePauseMenu()
{
	SetPauseMenuOpen(false);
}

bool AHSPlayerCharacter::SaveFromPauseMenu()
{
	const bool bSaved = SettlementPersistenceComponent && SettlementPersistenceComponent->SaveToSlot(GetSelectedManualSaveSlotName());
	SetMenuFeedback(bSaved
		? FText::Format(NSLOCTEXT("HSPlayerCharacter", "PauseSaveSucceeded", "Saved {0}."), FText::FromString(GetSelectedManualSaveSlotLabel()))
		: NSLOCTEXT("HSPlayerCharacter", "PauseSaveFailed", "Save failed."));
	UE_LOG(LogHSSettlement, Log, TEXT("%s pause menu save result: %s."), *GetNameSafe(this), bSaved ? TEXT("saved") : TEXT("failed"));
	return bSaved;
}

bool AHSPlayerCharacter::LoadFromPauseMenu()
{
	bAutosaveSuppressed = true;
	const bool bLoaded = SettlementPersistenceComponent && SettlementPersistenceComponent->LoadFromSlot(GetSelectedManualSaveSlotName());
	bAutosaveSuppressed = false;
	UE_LOG(LogHSSettlement, Log, TEXT("%s pause menu load result: %s."), *GetNameSafe(this), bLoaded ? TEXT("loaded") : TEXT("failed"));
	SetMenuFeedback(bLoaded
		? FText::Format(NSLOCTEXT("HSPlayerCharacter", "PauseLoadSucceeded", "Loaded {0}."), FText::FromString(GetSelectedManualSaveSlotLabel()))
		: NSLOCTEXT("HSPlayerCharacter", "PauseLoadFailed", "Load failed."));
	if (bLoaded)
	{
		BindAutosaveSources();
	}
	return bLoaded;
}

bool AHSPlayerCharacter::AutosaveSettlementState(const FString& Reason)
{
	if (bAutosaveSuppressed || !SettlementPersistenceComponent)
	{
		return false;
	}

	const bool bSaved = SettlementPersistenceComponent->SaveToSlot(SettlementPersistenceComponent->GetAutosaveSlotName());
	UE_LOG(LogHSSettlement, Log, TEXT("%s autosave after %s: %s."), *GetNameSafe(this), *Reason, bSaved ? TEXT("saved") : TEXT("failed"));
	return bSaved;
}

bool AHSPlayerCharacter::HasActiveMenuFeedback() const
{
	return !LastMenuFeedback.IsEmpty() && FPlatformTime::Seconds() < MenuFeedbackExpireTime;
}

FString AHSPlayerCharacter::GetGraphicsQualityPresetName() const
{
	switch (GraphicsQualityPreset)
	{
	case 0:
		return TEXT("Low");
	case 1:
		return TEXT("Medium");
	case 2:
		return TEXT("High");
	default:
		return TEXT("Custom");
	}
}

FString AHSPlayerCharacter::GetMouseSensitivityPresetName() const
{
	switch (MouseSensitivityPreset)
	{
	case 0:
		return TEXT("Low");
	case 1:
		return TEXT("Medium");
	case 2:
		return TEXT("High");
	default:
		return TEXT("Custom");
	}
}

FString AHSPlayerCharacter::GetMasterVolumePresetName() const
{
	switch (MasterVolumePreset)
	{
	case 0:
		return TEXT("Low");
	case 1:
		return TEXT("Medium");
	case 2:
		return TEXT("High");
	default:
		return TEXT("Custom");
	}
}

FString AHSPlayerCharacter::GetSelectedManualSaveSlotName() const
{
	const FString BaseSlotName = SettlementPersistenceComponent ? SettlementPersistenceComponent->GetDefaultSlotName() : TEXT("HimalayanSettlement_Prototype");
	if (SelectedManualSaveSlotIndex <= 0)
	{
		return BaseSlotName;
	}

	return FString::Printf(TEXT("%s_%d"), *BaseSlotName, SelectedManualSaveSlotIndex + 1);
}

FString AHSPlayerCharacter::GetSelectedManualSaveSlotLabel() const
{
	return FString::Printf(TEXT("Slot %d"), SelectedManualSaveSlotIndex + 1);
}

void AHSPlayerCharacter::ToggleInventoryMenu()
{
	SetInventoryOpen(!bInventoryOpen);
}

void AHSPlayerCharacter::ToggleBuildMenu()
{
	SetBuildMenuOpen(!bBuildMenuOpen);
}

int32 AHSPlayerCharacter::GetBuildMenuEntryCount() const
{
	return GetBuildMenuEntries().Num();
}

FText AHSPlayerCharacter::GetBuildMenuEntryLabel(int32 EntryIndex) const
{
	const FHSBuildMenuEntryDefinition* Entry = GetBuildMenuEntry(EntryIndex);
	return Entry ? Entry->Label : FText::GetEmpty();
}

FText AHSPlayerCharacter::GetBuildMenuEntryCategory(int32 EntryIndex) const
{
	const FHSBuildMenuEntryDefinition* Entry = GetBuildMenuEntry(EntryIndex);
	return Entry ? Entry->Category : FText::GetEmpty();
}

FText AHSPlayerCharacter::GetBuildMenuEntryDescription(int32 EntryIndex) const
{
	const FHSBuildMenuEntryDefinition* Entry = GetBuildMenuEntry(EntryIndex);
	return Entry ? Entry->Description : FText::GetEmpty();
}

float AHSPlayerCharacter::GetBuildMenuEntryRequiredDays(int32 EntryIndex) const
{
	const FHSBuildMenuEntryDefinition* Entry = GetBuildMenuEntry(EntryIndex);
	return Entry ? Entry->RequiredDays : 0.0f;
}

FGameplayTag AHSPlayerCharacter::GetBuildMenuEntryTag(int32 EntryIndex) const
{
	const FHSBuildMenuEntryDefinition* Entry = GetBuildMenuEntry(EntryIndex);
	return Entry ? Entry->BuildingTag : FGameplayTag();
}

void AHSPlayerCharacter::StartBasicHouseFromBuildMenu()
{
	StartBuildMenuEntry(0);
}

void AHSPlayerCharacter::StartBasicCampsiteFromBuildMenu()
{
	StartBuildMenuEntry(2);
}

void AHSPlayerCharacter::StartBasicHouseMinorUpgradeFromBuildMenu()
{
	StartBuildMenuEntry(1);
}

void AHSPlayerCharacter::StartSelectedBuildMenuEntry()
{
	ClampSelectedBuildMenuIndex();
	StartBuildMenuEntry(SelectedBuildMenuIndex);
}

bool AHSPlayerCharacter::StartBuildMenuEntry(int32 EntryIndex)
{
	const FHSBuildMenuEntryDefinition* Entry = GetBuildMenuEntry(EntryIndex);
	if (!Entry)
	{
		SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "BuildMenuInvalidEntry", "No build item in that slot."));
		return false;
	}

	if (!BuildPlacementComponent)
	{
		SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "NoBuildPlacementComponent", "Build placement is unavailable."));
		return false;
	}

	FText BlockReason;
	if (!CanStartBuildMenuEntry(EntryIndex, &BlockReason))
	{
		SetMenuFeedback(BlockReason);
		return false;
	}

	SetBuildMenuOpen(false);
	const bool bStarted = Entry->Kind == EHSBuildMenuEntryKind::TemporaryShelter
		? BuildPlacementComponent->StartBasicCampsitePlacement()
		: BuildPlacementComponent->StartBuildingPlacement(Entry->BuildingTag, Entry->Label);
	if (!bStarted)
	{
		SetMenuFeedback(FText::Format(NSLOCTEXT("HSPlayerCharacter", "BuildPlacementStartFailedGeneric", "Could not start {0} placement."), Entry->Label));
	}
	return bStarted;
}

void AHSPlayerCharacter::SelectBuildMenuDelta(int32 Delta)
{
	SelectedBuildMenuIndex = FMath::Clamp(SelectedBuildMenuIndex + Delta, 0, FMath::Max(0, GetBuildMenuEntryCount() - 1));
	SetMenuFeedback(FText::Format(NSLOCTEXT("HSPlayerCharacter", "BuildMenuSelectedGeneric", "Selected {0}."), GetBuildMenuEntryLabel(SelectedBuildMenuIndex)));
}

bool AHSPlayerCharacter::CanStartBuildMenuEntry(int32 EntryIndex, FText* OutReason) const
{
	const FHSBuildMenuEntryDefinition* Entry = GetBuildMenuEntry(EntryIndex);
	if (!Entry)
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HSPlayerCharacter", "BuildMenuInvalidSelection", "No build item selected.");
		}
		return false;
	}

	if (BuildPlacementComponent && BuildPlacementComponent->IsPlacementActive())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HSPlayerCharacter", "BuildMenuPlacementAlreadyActive", "Finish or cancel the current placement first.");
		}
		return false;
	}

	if (Entry->Kind == EHSBuildMenuEntryKind::TemporaryShelter)
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HSPlayerCharacter", "BuildMenuCampsiteReady", "Ready: place a temporary campsite to sleep once.");
		}
		return true;
	}

	if (Entry->bRequiresSettlementClaim && (!SettlementPlanComponent || !SettlementPlanComponent->HasSettlementClaim()))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HSPlayerCharacter", "BuildMenuNeedsClaimGeneric", "Claim a settlement site before placing this schema.");
		}
		return false;
	}

	if (Entry->bRequiresBasicHouse && (!ConstructionComponent || !ConstructionComponent->HasCompletedBuilding(TAG_HS_Building_BasicHouse)))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HSPlayerCharacter", "BuildMenuNeedsBasicHouseGeneric", "Complete the Basic House before placing this schema.");
		}
		return false;
	}

	if (Entry->PrerequisiteCompletedBuilding.IsValid() && (!ConstructionComponent || !ConstructionComponent->HasCompletedBuilding(Entry->PrerequisiteCompletedBuilding)))
	{
		if (OutReason)
		{
			*OutReason = FText::Format(NSLOCTEXT("HSPlayerCharacter", "BuildMenuNeedsPrerequisiteGeneric", "Complete {0} first."), FText::FromString(Entry->PrerequisiteCompletedBuilding.ToString()));
		}
		return false;
	}

	if (Entry->bRequiresUnlock && (!SettlementUnlockComponent || !SettlementUnlockComponent->HasUnlock(Entry->BuildingTag)))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HSPlayerCharacter", "BuildMenuNeedsUnlockGeneric", "Unlock this schema through settlement progress first.");
		}
		return false;
	}

	if (SettlementPopulationComponent && ConstructionComponent && !SettlementPopulationComponent->CanSupportBuilding(Entry->BuildingTag, ConstructionComponent->GetCompletedBuildings(), OutReason))
	{
		return false;
	}

	if (ConstructionComponent && ConstructionComponent->HasCompletedBuilding(Entry->BuildingTag))
	{
		if (OutReason)
		{
			*OutReason = FText::Format(NSLOCTEXT("HSPlayerCharacter", "BuildMenuAlreadyCompleteGeneric", "{0} is already complete."), Entry->Label);
		}
		return false;
	}

	if (ConstructionComponent && ConstructionComponent->HasActiveJob())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("HSPlayerCharacter", "BuildMenuConstructionActive", "Builder is already working on a construction job.");
		}
		return false;
	}

	if (SettlementPlanComponent && SettlementPlanComponent->HasPendingSchema(Entry->BuildingTag))
	{
		if (OutReason)
		{
			*OutReason = FText::Format(NSLOCTEXT("HSPlayerCharacter", "BuildMenuPendingGeneric", "{0} schema is already placed. Ask the Builder to start it."), Entry->Label);
		}
		return false;
	}

	if (OutReason)
	{
		*OutReason = FText::Format(NSLOCTEXT("HSPlayerCharacter", "BuildMenuReadyGeneric", "Ready: place a {0} schema for the Builder."), Entry->Label);
	}
	return true;
}

bool AHSPlayerCharacter::CanStartBasicHouseFromBuildMenu(FText* OutReason) const
{
	return CanStartBuildMenuEntry(0, OutReason);
}

bool AHSPlayerCharacter::CanStartBasicHouseMinorUpgradeFromBuildMenu(FText* OutReason) const
{
	return CanStartBuildMenuEntry(1, OutReason);
}

bool AHSPlayerCharacter::CanStartBasicCampsiteFromBuildMenu(FText* OutReason) const
{
	return CanStartBuildMenuEntry(2, OutReason);
}

bool AHSPlayerCharacter::InspectInventoryStack(int32 StackIndex)
{
	ClampSelectedInventoryIndex();

	FHSItemStack Stack;
	if (!ResourceCarryComponent || !ResourceCarryComponent->GetStackAtIndex(StackIndex, Stack))
	{
		SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "InventoryInspectEmpty", "No carried item in that slot."));
		return false;
	}

	SetMenuFeedback(FText::Format(
		NSLOCTEXT("HSPlayerCharacter", "InventoryInspectStack", "{0} x{1}. Carried stack for quests and services."),
		FText::FromString(Stack.ItemTag.ToString()),
		FText::AsNumber(Stack.Quantity)));
	return true;
}

bool AHSPlayerCharacter::DropInventoryStackQuantity(int32 StackIndex, int32 Quantity)
{
	FHSItemStack ConsumedStack;
	if (!ResourceCarryComponent || !ResourceCarryComponent->TryConsumeStackAtIndex(StackIndex, Quantity, ConsumedStack))
	{
		SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "InventoryDropEmpty", "No carried item to drop."));
		return false;
	}

	SetMenuFeedback(FText::Format(
		NSLOCTEXT("HSPlayerCharacter", "InventoryDroppedStack", "Dropped {0} x{1}."),
		FText::FromString(ConsumedStack.ItemTag.ToString()),
		FText::AsNumber(ConsumedStack.Quantity)));
	SpawnDroppedInventoryStack(ConsumedStack);
	ClampSelectedInventoryIndex();
	return true;
}

bool AHSPlayerCharacter::InspectSelectedInventoryStack()
{
	ClampSelectedInventoryIndex();
	return InspectInventoryStack(SelectedInventoryIndex);
}

bool AHSPlayerCharacter::DropSelectedInventoryStackQuantity(int32 Quantity)
{
	ClampSelectedInventoryIndex();
	return DropInventoryStackQuantity(SelectedInventoryIndex, Quantity);
}

bool AHSPlayerCharacter::UseSelectedInventoryStack()
{
	ClampSelectedInventoryIndex();

	FHSItemStack Stack;
	if (!ResourceCarryComponent || !ResourceCarryComponent->GetStackAtIndex(SelectedInventoryIndex, Stack))
	{
		SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "InventoryUseEmpty", "No carried item to use."));
		return false;
	}

	if (Stack.ItemTag == TAG_HS_Item_Bow_Basic || Stack.ItemTag == TAG_HS_Item_Bow_Reinforced)
	{
		if (PlayerEquipmentComponent && PlayerEquipmentComponent->SetBowEquipped(true))
		{
			SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "InventoryUseBowEquipped", "Bow equipped."));
			return true;
		}

		SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "InventoryUseBowFailed", "Could not equip bow."));
		return false;
	}

	if (Stack.ItemTag == TAG_HS_Item_Spear_Basic)
	{
		if (PlayerEquipmentComponent && PlayerEquipmentComponent->SetSpearEquipped(true))
		{
			SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "InventoryUseSpearEquipped", "Spear equipped."));
			return true;
		}

		SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "InventoryUseSpearFailed", "Could not equip spear."));
		return false;
	}

	SetMenuFeedback(FText::Format(
		NSLOCTEXT("HSPlayerCharacter", "InventoryUseUnsupported", "{0} cannot be used yet."),
		FText::FromString(Stack.ItemTag.ToString())));
	return false;
}

void AHSPlayerCharacter::SelectInventoryDelta(int32 Delta)
{
	const int32 ItemCount = ResourceCarryComponent ? ResourceCarryComponent->GetCarriedItems().Num() : 0;
	if (ItemCount <= 0)
	{
		SelectedInventoryIndex = 0;
		SetMenuFeedback(NSLOCTEXT("HSPlayerCharacter", "InventorySelectionEmpty", "No carried items."));
		return;
	}

	SelectedInventoryIndex = FMath::Clamp(SelectedInventoryIndex + Delta, 0, ItemCount - 1);
	FHSItemStack Stack;
	if (ResourceCarryComponent && ResourceCarryComponent->GetStackAtIndex(SelectedInventoryIndex, Stack))
	{
		SetMenuFeedback(FText::Format(
			NSLOCTEXT("HSPlayerCharacter", "InventorySelectedStack", "Selected {0} x{1}."),
			FText::FromString(Stack.ItemTag.ToString()),
			FText::AsNumber(Stack.Quantity)));
	}
}

void AHSPlayerCharacter::CycleGraphicsQualityPreset()
{
	GraphicsQualityPreset = (GraphicsQualityPreset + 1) % 3;

	const FString PresetValue = FString::FromInt(GraphicsQualityPreset);
	UKismetSystemLibrary::ExecuteConsoleCommand(this, FString::Printf(TEXT("sg.ViewDistanceQuality %s"), *PresetValue));
	UKismetSystemLibrary::ExecuteConsoleCommand(this, FString::Printf(TEXT("sg.AntiAliasingQuality %s"), *PresetValue));
	UKismetSystemLibrary::ExecuteConsoleCommand(this, FString::Printf(TEXT("sg.ShadowQuality %s"), *PresetValue));
	UKismetSystemLibrary::ExecuteConsoleCommand(this, FString::Printf(TEXT("sg.GlobalIlluminationQuality %s"), *PresetValue));
	UKismetSystemLibrary::ExecuteConsoleCommand(this, FString::Printf(TEXT("sg.ReflectionQuality %s"), *PresetValue));
	UKismetSystemLibrary::ExecuteConsoleCommand(this, FString::Printf(TEXT("sg.PostProcessQuality %s"), *PresetValue));
	UKismetSystemLibrary::ExecuteConsoleCommand(this, FString::Printf(TEXT("sg.TextureQuality %s"), *PresetValue));
	UKismetSystemLibrary::ExecuteConsoleCommand(this, FString::Printf(TEXT("sg.EffectsQuality %s"), *PresetValue));
	UKismetSystemLibrary::ExecuteConsoleCommand(this, FString::Printf(TEXT("sg.FoliageQuality %s"), *PresetValue));
	SetMenuFeedback(FText::Format(
		NSLOCTEXT("HSPlayerCharacter", "GraphicsPresetChanged", "Graphics preset: {0}."),
		FText::FromString(GetGraphicsQualityPresetName())));
}

void AHSPlayerCharacter::CycleMouseSensitivityPreset()
{
	MouseSensitivityPreset = (MouseSensitivityPreset + 1) % 3;
	SetMenuFeedback(FText::Format(
		NSLOCTEXT("HSPlayerCharacter", "MouseSensitivityChanged", "Mouse sensitivity: {0}."),
		FText::FromString(GetMouseSensitivityPresetName())));
}

void AHSPlayerCharacter::CycleMasterVolumePreset()
{
	MasterVolumePreset = (MasterVolumePreset + 1) % 3;
	ApplyMasterVolumePreset();
	SetMenuFeedback(FText::Format(
		NSLOCTEXT("HSPlayerCharacter", "MasterVolumeChanged", "Master volume: {0}."),
		FText::FromString(GetMasterVolumePresetName())));
}

void AHSPlayerCharacter::CycleManualSaveSlot()
{
	SelectedManualSaveSlotIndex = (SelectedManualSaveSlotIndex + 1) % 3;
	SetMenuFeedback(FText::Format(
		NSLOCTEXT("HSPlayerCharacter", "ManualSaveSlotChanged", "Manual save slot: {0}."),
		FText::FromString(GetSelectedManualSaveSlotLabel())));
}

void AHSPlayerCharacter::TogglePerformanceOverlay()
{
	bPerformanceOverlayEnabled = !bPerformanceOverlayEnabled;

	if (GetWorld())
	{
		UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("stat fps"));
		UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("stat unit"));
	}

	SetMenuFeedback(bPerformanceOverlayEnabled
		? NSLOCTEXT("HSPlayerCharacter", "PerformanceOverlayEnabled", "Performance overlay: On (F10).")
		: NSLOCTEXT("HSPlayerCharacter", "PerformanceOverlayDisabled", "Performance overlay: Off (F10)."));
}

void AHSPlayerCharacter::ToggleEmergencyLowEndMode()
{
	SetEmergencyLowEndModeEnabled(!bEmergencyLowEndModeEnabled);
}

void AHSPlayerCharacter::SetEmergencyLowEndModeEnabled(bool bNewEnabled)
{
	bEmergencyLowEndModeEnabled = bNewEnabled;
	if (ExplorationStateComponent)
	{
		ExplorationStateComponent->SetLowEndStreamingEnabled(bEmergencyLowEndModeEnabled);
	}

	if (GetWorld())
	{
		if (bEmergencyLowEndModeEnabled)
		{
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.ResolutionQuality 55"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.ViewDistanceQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.AntiAliasingQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.ShadowQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.GlobalIlluminationQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.ReflectionQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.PostProcessQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.EffectsQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.FoliageQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.ShadingQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.ScreenPercentage 55"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.MotionBlurQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.BloomQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.DepthOfFieldQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.AmbientOcclusionLevels 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.SSR.Quality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.VolumetricFog 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.Fog 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("foliage.LODDistanceScale 0.45"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("grass.DensityScale 0.25"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.StaticMeshLODDistanceScale 1.75"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.SkeletalMeshLODBias 1"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.Streaming.PoolSize 512"));
		}
		else
		{
			GraphicsQualityPreset = 0;
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.ResolutionQuality 60"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.ViewDistanceQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.AntiAliasingQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.ShadowQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.GlobalIlluminationQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.ReflectionQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.PostProcessQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.TextureQuality 1"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.EffectsQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.FoliageQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("sg.ShadingQuality 0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.ScreenPercentage 60"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.Fog 1"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("foliage.LODDistanceScale 1.0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("grass.DensityScale 1.0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.StaticMeshLODDistanceScale 1.0"));
			UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("r.SkeletalMeshLODBias 0"));
		}
	}

	SetMenuFeedback(bEmergencyLowEndModeEnabled
		? NSLOCTEXT("HSPlayerCharacter", "EmergencyLowEndEnabled", "Emergency low-end mode: On (F8).")
		: NSLOCTEXT("HSPlayerCharacter", "EmergencyLowEndDisabled", "Emergency low-end mode: Off (F8)."));
}

void AHSPlayerCharacter::SetPlayerStance(EHSPlayerStance NewStance)
{
	if (PlayerStance == NewStance)
	{
		return;
	}

	PlayerStance = NewStance;
	ApplyStanceMovementSettings();
}

void AHSPlayerCharacter::ApplyInputMappingContext()
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (InputSubsystem)
	{
		InputSubsystem->RemoveMappingContext(PlayerMappingContext);
		InputSubsystem->AddMappingContext(PlayerMappingContext, 0);
	}
}

void AHSPlayerCharacter::HandlePlantEffectMovementChanged(const FHSPlantEffectState& PlantEffect)
{
	ApplyStanceMovementSettings();
}

void AHSPlayerCharacter::HandleBowEquippedChanged(bool bBowEquipped)
{
	RefreshBowVisuals();
}

float AHSPlayerCharacter::GetMouseSensitivityScale() const
{
	switch (MouseSensitivityPreset)
	{
	case 0:
		return 0.65f;
	case 1:
		return 1.0f;
	case 2:
		return 1.35f;
	default:
		return 1.0f;
	}
}

float AHSPlayerCharacter::GetMasterVolumeScale() const
{
	switch (MasterVolumePreset)
	{
	case 0:
		return 0.45f;
	case 1:
		return 0.75f;
	case 2:
		return 1.0f;
	default:
		return 1.0f;
	}
}

void AHSPlayerCharacter::ApplyMasterVolumePreset()
{
	FApp::SetVolumeMultiplier(GetMasterVolumeScale());
}

void AHSPlayerCharacter::RefreshBowVisuals()
{
	const bool bShowBow = PlayerEquipmentComponent && PlayerEquipmentComponent->IsBowEquipped();
	if (BowGripMesh)
	{
		BowGripMesh->SetHiddenInGame(!bShowBow);
	}

	if (BowUpperLimbMesh)
	{
		BowUpperLimbMesh->SetHiddenInGame(!bShowBow);
	}

	if (BowLowerLimbMesh)
	{
		BowLowerLimbMesh->SetHiddenInGame(!bShowBow);
	}

	if (BowStringMesh)
	{
		BowStringMesh->SetHiddenInGame(!bShowBow);
	}
}

void AHSPlayerCharacter::SetPauseMenuOpen(bool bNewOpen)
{
	if (bPauseMenuOpen == bNewOpen)
	{
		return;
	}

	bPauseMenuOpen = bNewOpen;
	if (bPauseMenuOpen)
	{
		CloseNonPauseMenus();
	}
	else
	{
		bPauseOptionsOpen = false;
	}

	if (GetWorld())
	{
		UGameplayStatics::SetGamePaused(this, bPauseMenuOpen);
	}

	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (PlayerController)
	{
		PlayerController->bShowMouseCursor = bPauseMenuOpen;
		PlayerController->SetIgnoreLookInput(bPauseMenuOpen);
		PlayerController->SetIgnoreMoveInput(bPauseMenuOpen);
	}
}

void AHSPlayerCharacter::SetInventoryOpen(bool bNewOpen)
{
	if (bInventoryOpen == bNewOpen)
	{
		return;
	}

	bInventoryOpen = bNewOpen;
	if (bInventoryOpen)
	{
		bBuildMenuOpen = false;
		ClampSelectedInventoryIndex();
	}
}

void AHSPlayerCharacter::SetBuildMenuOpen(bool bNewOpen)
{
	if (bBuildMenuOpen == bNewOpen)
	{
		return;
	}

	bBuildMenuOpen = bNewOpen;
	if (bBuildMenuOpen)
	{
		bInventoryOpen = false;
		ClampSelectedBuildMenuIndex();
	}
}

void AHSPlayerCharacter::CloseNonPauseMenus()
{
	bInventoryOpen = false;
	bBuildMenuOpen = false;
}

void AHSPlayerCharacter::TogglePauseOptions()
{
	bPauseOptionsOpen = !bPauseOptionsOpen;
}

void AHSPlayerCharacter::ExitGameFromPauseMenu() const
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	UKismetSystemLibrary::QuitGame(this, PlayerController, EQuitPreference::Quit, false);
}

void AHSPlayerCharacter::SetMenuFeedback(FText Feedback)
{
	LastMenuFeedback = Feedback;
	MenuFeedbackExpireTime = FPlatformTime::Seconds() + MenuFeedbackDuration;
}

bool AHSPlayerCharacter::SelectFocusedNPCServiceDelta(int32 Delta)
{
	if (!InteractionSensor)
	{
		return false;
	}

	InteractionSensor->RefreshFocus();
	AHSNPCActor* FocusedNPC = Cast<AHSNPCActor>(InteractionSensor->GetFocusedInteractable());
	UHSNPCServiceComponent* ServiceComponent = FocusedNPC ? FocusedNPC->GetNPCServiceComponent() : nullptr;
	if (!ServiceComponent || !ServiceComponent->SelectServiceDelta(Delta, this))
	{
		return false;
	}

	FHSNPCServiceOption SelectedService;
	if (ServiceComponent->GetSelectedServiceOption(this, SelectedService))
	{
		SetMenuFeedback(FText::Format(
			NSLOCTEXT("HSPlayerCharacter", "NPCServiceSelected", "Selected service: {0}."),
			SelectedService.Prompt));
	}
	return true;
}

bool AHSPlayerCharacter::SpawnDroppedInventoryStack(const FHSItemStack& DroppedStack)
{
	if (!DroppedStack.IsValidStack() || !ResourcePickupActorClass)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector DropForward = GetActorForwardVector().GetSafeNormal();
	const FVector DropLocation = GetActorLocation() + (DropForward * 120.0f) + FVector(0.0f, 0.0f, 36.0f);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHSResourcePickupActor* DroppedPickup = World->SpawnActor<AHSResourcePickupActor>(ResourcePickupActorClass, DropLocation, FRotator::ZeroRotator, SpawnParameters);
	if (!DroppedPickup)
	{
		return false;
	}

	DroppedPickup->SetItemStack(DroppedStack);
	if (UHSWorldCleanupSubsystem* CleanupSubsystem = World->GetSubsystem<UHSWorldCleanupSubsystem>())
	{
		CleanupSubsystem->RegisterTemporaryActor(DroppedPickup, 300.0f, TEXT("DroppedResource"));
	}
	return true;
}

void AHSPlayerCharacter::ClampSelectedInventoryIndex()
{
	const int32 ItemCount = ResourceCarryComponent ? ResourceCarryComponent->GetCarriedItems().Num() : 0;
	SelectedInventoryIndex = ItemCount > 0 ? FMath::Clamp(SelectedInventoryIndex, 0, ItemCount - 1) : 0;
}

void AHSPlayerCharacter::ClampSelectedBuildMenuIndex()
{
	SelectedBuildMenuIndex = FMath::Clamp(SelectedBuildMenuIndex, 0, FMath::Max(0, GetBuildMenuEntryCount() - 1));
}

void AHSPlayerCharacter::BindAutosaveSources()
{
	if (PlayerProgressComponent)
	{
		PlayerProgressComponent->OnMilestoneCompleted.RemoveDynamic(this, &AHSPlayerCharacter::HandleAutosaveMilestoneCompleted);
		PlayerProgressComponent->OnMilestoneCompleted.AddUniqueDynamic(this, &AHSPlayerCharacter::HandleAutosaveMilestoneCompleted);
	}

	if (ConstructionComponent)
	{
		ConstructionComponent->OnConstructionJobStarted.RemoveDynamic(this, &AHSPlayerCharacter::HandleAutosaveConstructionChanged);
		ConstructionComponent->OnConstructionJobStarted.AddUniqueDynamic(this, &AHSPlayerCharacter::HandleAutosaveConstructionChanged);
		ConstructionComponent->OnConstructionJobCompleted.RemoveDynamic(this, &AHSPlayerCharacter::HandleAutosaveConstructionChanged);
		ConstructionComponent->OnConstructionJobCompleted.AddUniqueDynamic(this, &AHSPlayerCharacter::HandleAutosaveConstructionChanged);
	}

	const UWorld* World = GetWorld();
	const AHSGameState* GameState = World ? World->GetGameState<AHSGameState>() : nullptr;
	UHSWorldTimeComponent* WorldTime = GameState ? GameState->GetWorldTimeComponent() : nullptr;
	if (WorldTime && BoundAutosaveWorldTime.Get() != WorldTime)
	{
		if (UHSWorldTimeComponent* PreviousWorldTime = BoundAutosaveWorldTime.Get())
		{
			PreviousWorldTime->OnTimeAdvanced.RemoveDynamic(this, &AHSPlayerCharacter::HandleAutosaveTimeAdvanced);
		}

		BoundAutosaveWorldTime = WorldTime;
		WorldTime->OnTimeAdvanced.AddUniqueDynamic(this, &AHSPlayerCharacter::HandleAutosaveTimeAdvanced);
	}
}

void AHSPlayerCharacter::HandleAutosaveMilestoneCompleted(FGameplayTag MilestoneTag)
{
	AutosaveSettlementState(FString::Printf(TEXT("milestone %s"), *MilestoneTag.ToString()));
}

void AHSPlayerCharacter::HandleAutosaveConstructionChanged(const FHSConstructionJob& ConstructionJob)
{
	if (SettlementPopulationComponent && ConstructionComponent)
	{
		SettlementPopulationComponent->RefreshFromCompletedBuildings(ConstructionComponent->GetCompletedBuildings());
	}
	AutosaveSettlementState(FString::Printf(TEXT("construction %s"), *ConstructionJob.BuildingTag.ToString()));
}

void AHSPlayerCharacter::HandleAutosaveTimeAdvanced(EHSTimePhase NewTimePhase, float ElapsedDays)
{
	(void)NewTimePhase;
	if (SettlementPopulationComponent)
	{
		SettlementPopulationComponent->AdvanceSettlementDay(0.5f);
	}
	AutosaveSettlementState(FString::Printf(TEXT("time %.1f days"), ElapsedDays));
}

void AHSPlayerCharacter::ApplyStanceMovementSettings()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!MovementComponent || !Capsule || !FirstPersonCamera)
	{
		return;
	}

	const float PlantSpeedMultiplier = PlantEffectComponent ? PlantEffectComponent->GetMovementSpeedMultiplier() : 1.0f;

	switch (PlayerStance)
	{
	case EHSPlayerStance::Crouching:
		UnCrouch(false);
		Capsule->SetCapsuleHalfHeight(CrouchCapsuleHalfHeight, true);
		FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, CrouchCameraHeight));
		MovementComponent->MaxWalkSpeed = CrouchWalkSpeed * PlantSpeedMultiplier;
		Crouch(false);
		break;

	case EHSPlayerStance::Prone:
		UnCrouch(false);
		Capsule->SetCapsuleHalfHeight(ProneCapsuleHalfHeight, true);
		FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, ProneCameraHeight));
		MovementComponent->MaxWalkSpeed = ProneMoveSpeed * PlantSpeedMultiplier;
		break;

	case EHSPlayerStance::Standing:
	default:
		UnCrouch(false);
		Capsule->SetCapsuleHalfHeight(StandingCapsuleHalfHeight, true);
		FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, StandingCameraHeight));
		MovementComponent->MaxWalkSpeed = StandingWalkSpeed * PlantSpeedMultiplier;
		break;
	}
}
