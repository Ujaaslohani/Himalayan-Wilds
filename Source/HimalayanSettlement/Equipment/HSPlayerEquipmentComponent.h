#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSPlayerEquipmentComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSBowEquippedChangedSignature, bool, bBowEquipped);

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSPlayerEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSPlayerEquipmentComponent();

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool IsBowEquipped() const { return bBowEquipped; }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool HasBow() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetArrowCount() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool CanUseBow() const;

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool SetBowEquipped(bool bNewBowEquipped);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool ToggleBowEquipped();

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool ConsumeArrow();

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FHSBowEquippedChangedSignature OnBowEquippedChanged;

private:
	class UHSResourceCarryComponent* ResolveResourceCarry() const;

	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	bool bBowEquipped;
};
