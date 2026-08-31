#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSWorldTimeComponent.generated.h"

UENUM(BlueprintType)
enum class EHSTimePhase : uint8
{
	Day,
	Night
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHSTimeAdvancedSignature, EHSTimePhase, NewTimePhase, float, ElapsedDays);

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSWorldTimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSWorldTimeComponent();

	UPROPERTY(BlueprintAssignable, Category = "World Time")
	FHSTimeAdvancedSignature OnTimeAdvanced;

	UFUNCTION(BlueprintPure, Category = "World Time")
	EHSTimePhase GetTimePhase() const { return TimePhase; }

	UFUNCTION(BlueprintPure, Category = "World Time")
	float GetElapsedDays() const { return ElapsedHalfDays * 0.5f; }

	UFUNCTION(BlueprintPure, Category = "World Time")
	int32 GetElapsedHalfDays() const { return ElapsedHalfDays; }

	UFUNCTION(BlueprintPure, Category = "World Time")
	FText GetTimePhaseText() const;

	UFUNCTION(BlueprintCallable, Category = "World Time")
	void SetTimePhase(EHSTimePhase NewTimePhase);

	UFUNCTION(BlueprintCallable, Category = "World Time")
	void SetTimeState(EHSTimePhase NewTimePhase, int32 NewElapsedHalfDays);

	UFUNCTION(BlueprintCallable, Category = "World Time")
	EHSTimePhase AdvanceHalfDay();

private:
	UPROPERTY(VisibleAnywhere, Category = "World Time")
	EHSTimePhase TimePhase;

	UPROPERTY(VisibleAnywhere, Category = "World Time")
	int32 ElapsedHalfDays;
};
