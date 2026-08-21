#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HSExplorationStateComponent.generated.h"

USTRUCT(BlueprintType)
struct HIMALAYANSETTLEMENT_API FHSDiscoveredLocation
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exploration")
	FName LocationId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exploration")
	FText DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exploration")
	FGameplayTag LocationTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exploration")
	FVector Location = FVector::ZeroVector;

	bool IsValid() const
	{
		return !LocationId.IsNone() && LocationTag.IsValid();
	}
};

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSExplorationStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSExplorationStateComponent();

	UFUNCTION(BlueprintCallable, Category = "Exploration")
	bool DiscoverLocation(FName LocationId, FText DisplayName, FGameplayTag LocationTag, FVector Location);

	UFUNCTION(BlueprintCallable, Category = "Exploration")
	void SetLowEndStreamingEnabled(bool bNewEnabled);

	UFUNCTION(BlueprintPure, Category = "Exploration")
	bool IsLowEndStreamingEnabled() const { return bLowEndStreamingEnabled; }

	UFUNCTION(BlueprintPure, Category = "Exploration")
	float GetNaturalBoundaryRadius() const { return NaturalBoundaryRadius; }

	UFUNCTION(BlueprintPure, Category = "Exploration")
	const TArray<FHSDiscoveredLocation>& GetDiscoveredLocations() const { return DiscoveredLocations; }

	UFUNCTION(BlueprintPure, Category = "Exploration")
	int32 GetDiscoveredLocationCount() const { return DiscoveredLocations.Num(); }

	UFUNCTION(BlueprintPure, Category = "Exploration")
	int32 CountDiscoveredByTag(FGameplayTag LocationTag) const;

	UFUNCTION(BlueprintPure, Category = "Exploration")
	FText GetExplorationStatusText() const;

	void RestoreExplorationState(const TArray<FHSDiscoveredLocation>& NewDiscoveredLocations, bool bNewLowEndStreamingEnabled);

private:
	bool HasDiscoveredLocation(FName LocationId) const;

	UPROPERTY(VisibleAnywhere, Category = "Exploration")
	TArray<FHSDiscoveredLocation> DiscoveredLocations;

	UPROPERTY(EditDefaultsOnly, Category = "Exploration", meta = (ClampMin = "1000.0", Units = "cm"))
	float NaturalBoundaryRadius;

	UPROPERTY(VisibleAnywhere, Category = "Exploration")
	bool bLowEndStreamingEnabled;
};
