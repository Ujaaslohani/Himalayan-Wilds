#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HSPlantEffectComponent.generated.h"

class UHSPlantDefinition;
enum class EHSTimePhase : uint8;

USTRUCT(BlueprintType)
struct HIMALAYANSETTLEMENT_API FHSPlantEffectState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plant Effect")
	FGameplayTag PlantTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plant Effect")
	FGameplayTag EffectTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plant Effect")
	float MovementSpeedMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plant Effect")
	float StartElapsedDays = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plant Effect")
	float DurationDays = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plant Effect")
	bool bActive = false;

	bool IsValid() const
	{
		return bActive && PlantTag.IsValid() && EffectTag.IsValid() && MovementSpeedMultiplier >= 1.0f && DurationDays > 0.0f;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSPlantEffectSignature, const FHSPlantEffectState&, PlantEffect);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSPlantKnowledgeSignature, FGameplayTag, PlantTag);

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSPlantEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSPlantEffectComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintAssignable, Category = "Plant Effect")
	FHSPlantEffectSignature OnPlantEffectApplied;

	UPROPERTY(BlueprintAssignable, Category = "Plant Effect")
	FHSPlantEffectSignature OnPlantEffectExpired;

	UPROPERTY(BlueprintAssignable, Category = "Plant Knowledge")
	FHSPlantKnowledgeSignature OnPlantKnowledgeLearned;

	UFUNCTION(BlueprintCallable, Category = "Plant Knowledge")
	bool LearnPlant(FGameplayTag PlantTag);

	UFUNCTION(BlueprintPure, Category = "Plant Knowledge")
	bool HasPlantKnowledge(FGameplayTag PlantTag) const;

	UFUNCTION(BlueprintCallable, Category = "Plant Effect")
	bool ApplyPlantEffect(const UHSPlantDefinition* PlantDefinition);

	UFUNCTION(BlueprintCallable, Category = "Plant Effect")
	void ExpireActiveEffect();

	UFUNCTION(BlueprintCallable, Category = "Plant Effect")
	void BindToWorldTime();

	UFUNCTION(BlueprintPure, Category = "Plant Effect")
	bool HasActiveEffect() const { return ActiveEffect.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Plant Effect")
	float GetMovementSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Plant Effect")
	const FHSPlantEffectState& GetActiveEffect() const { return ActiveEffect; }

	UFUNCTION(BlueprintPure, Category = "Plant Knowledge")
	const FGameplayTagContainer& GetKnownPlants() const { return KnownPlants; }

	void RestorePlantState(const FGameplayTagContainer& NewKnownPlants, const FHSPlantEffectState& NewActiveEffect);

private:
	UFUNCTION()
	void HandleTimeAdvanced(EHSTimePhase NewTimePhase, float ElapsedDays);

	float ResolveElapsedDays() const;
	class UHSWorldTimeComponent* ResolveWorldTime() const;

	UPROPERTY(VisibleAnywhere, Category = "Plant Knowledge")
	FGameplayTagContainer KnownPlants;

	UPROPERTY(VisibleAnywhere, Category = "Plant Effect")
	FHSPlantEffectState ActiveEffect;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UHSWorldTimeComponent> BoundWorldTime;
};
