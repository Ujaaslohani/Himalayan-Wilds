#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSSettlementPersistenceComponent.generated.h"

class UHSSettlementSaveGame;

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSSettlementPersistenceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSSettlementPersistenceComponent();

	UFUNCTION(BlueprintCallable, Category = "Persistence")
	UHSSettlementSaveGame* CreateSnapshot() const;

	UFUNCTION(BlueprintCallable, Category = "Persistence")
	bool CaptureToSaveGame(UHSSettlementSaveGame* SaveGame) const;

	UFUNCTION(BlueprintCallable, Category = "Persistence")
	bool RestoreFromSaveGame(const UHSSettlementSaveGame* SaveGame);

	UFUNCTION(BlueprintCallable, Category = "Persistence")
	bool SaveToSlot(const FString& SlotName, int32 UserIndex = 0) const;

	UFUNCTION(BlueprintCallable, Category = "Persistence")
	bool LoadFromSlot(const FString& SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintPure, Category = "Persistence")
	const FString& GetDefaultSlotName() const { return DefaultSlotName; }

private:
	class UHSWorldTimeComponent* ResolveWorldTime() const;

	UPROPERTY(EditDefaultsOnly, Category = "Persistence")
	FString DefaultSlotName;
};
