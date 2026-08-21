#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSPlayerEquipmentComponent.generated.h"

UENUM(BlueprintType)
enum class EHSPlayerWeaponMode : uint8
{
	Unarmed,
	Bow,
	Spear
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSBowEquippedChangedSignature, bool, bBowEquipped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSWeaponModeChangedSignature, EHSPlayerWeaponMode, NewWeaponMode);

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSPlayerEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSPlayerEquipmentComponent();

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool IsBowEquipped() const { return bBowEquipped; }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	EHSPlayerWeaponMode GetEquippedWeaponMode() const { return EquippedWeaponMode; }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool IsSpearEquipped() const { return EquippedWeaponMode == EHSPlayerWeaponMode::Spear; }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool HasBow() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool HasSpear() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetArrowCount() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool CanUseBow() const;

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool SetBowEquipped(bool bNewBowEquipped);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool SetSpearEquipped(bool bNewSpearEquipped);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool SetWeaponMode(EHSPlayerWeaponMode NewWeaponMode);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool ToggleBowEquipped();

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool CycleWeaponMode();

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool ConsumeArrow();

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FHSBowEquippedChangedSignature OnBowEquippedChanged;

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FHSWeaponModeChangedSignature OnWeaponModeChanged;

private:
	class UHSResourceCarryComponent* ResolveResourceCarry() const;

	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	bool bBowEquipped;

	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	EHSPlayerWeaponMode EquippedWeaponMode;
};
