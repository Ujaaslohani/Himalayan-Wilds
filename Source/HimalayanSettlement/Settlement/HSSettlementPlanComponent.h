#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HSSettlementPlanComponent.generated.h"

class UHSBuildingDefinition;

USTRUCT(BlueprintType)
struct HIMALAYANSETTLEMENT_API FHSPendingBuildingSchema
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settlement")
	FGameplayTag BuildingTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settlement")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settlement")
	float YawDegrees = 0.0f;

	bool IsValid() const
	{
		return BuildingTag.IsValid();
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSSettlementClaimedSignature, FVector, ClaimCenter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSBuildingSchemaPlacedSignature, const FHSPendingBuildingSchema&, PendingSchema);

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSSettlementPlanComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSSettlementPlanComponent();

	UPROPERTY(BlueprintAssignable, Category = "Settlement")
	FHSSettlementClaimedSignature OnSettlementClaimed;

	UPROPERTY(BlueprintAssignable, Category = "Settlement")
	FHSBuildingSchemaPlacedSignature OnBuildingSchemaPlaced;

	UFUNCTION(BlueprintCallable, Category = "Settlement")
	bool ClaimSettlement(FVector ClaimCenter);

	bool CanPlaceSchema(const UHSBuildingDefinition* BuildingDefinition, FVector Location, FText* OutReason = nullptr) const;

	bool PlaceSchema(const UHSBuildingDefinition* BuildingDefinition, FVector Location, float YawDegrees, FHSPendingBuildingSchema& OutPendingSchema);

	bool ConsumePendingSchema(FGameplayTag BuildingTag, FHSPendingBuildingSchema& OutPendingSchema);

	void RestoreSettlementPlan(bool bNewHasSettlementClaim, FVector NewSettlementClaimCenter, const TArray<FHSPendingBuildingSchema>& NewPendingSchemas);

	UFUNCTION(BlueprintPure, Category = "Settlement")
	bool HasSettlementClaim() const { return bHasSettlementClaim; }

	UFUNCTION(BlueprintPure, Category = "Settlement")
	FVector GetSettlementClaimCenter() const { return SettlementClaimCenter; }

	UFUNCTION(BlueprintPure, Category = "Settlement")
	float GetClaimRadius() const { return ClaimRadius; }

	UFUNCTION(BlueprintPure, Category = "Settlement")
	bool HasPendingSchema(FGameplayTag BuildingTag) const;

	UFUNCTION(BlueprintPure, Category = "Settlement")
	bool HasAnyPendingSchema() const { return PendingSchemas.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "Settlement")
	const TArray<FHSPendingBuildingSchema>& GetPendingSchemas() const { return PendingSchemas; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Settlement")
	bool bHasSettlementClaim;

	UPROPERTY(VisibleAnywhere, Category = "Settlement")
	FVector SettlementClaimCenter;

	UPROPERTY(EditDefaultsOnly, Category = "Settlement", meta = (ClampMin = "100.0", Units = "cm"))
	float ClaimRadius;

	UPROPERTY(VisibleAnywhere, Category = "Settlement")
	TArray<FHSPendingBuildingSchema> PendingSchemas;
};
