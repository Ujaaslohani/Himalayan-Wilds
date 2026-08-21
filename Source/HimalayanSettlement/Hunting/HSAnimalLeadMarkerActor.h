#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interaction/HSInteractable.h"
#include "HSAnimalLeadMarkerActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSAnimalLeadMarkerActor : public AActor, public IHSInteractable
{
	GENERATED_BODY()

public:
	AHSAnimalLeadMarkerActor();

	UFUNCTION(BlueprintCallable, Category = "Animal Lead")
	void ConfigureLead(FGameplayTag NewAnimalTag, int32 NewReportDay);

	UFUNCTION(BlueprintPure, Category = "Animal Lead")
	FGameplayTag GetAnimalTag() const { return AnimalTag; }

	UFUNCTION(BlueprintPure, Category = "Animal Lead")
	int32 GetReportDay() const { return ReportDay; }

	UFUNCTION(BlueprintPure, Category = "Animal Lead")
	bool IsExpired() const { return bExpired; }

	UFUNCTION(BlueprintCallable, Category = "Animal Lead")
	void ExpireLead();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionPrompt(const AActor* Interactor) const;
	virtual FText GetInteractionPrompt_Implementation(const AActor* Interactor) const override;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(const AActor* Interactor) const;
	virtual bool CanInteract_Implementation(const AActor* Interactor) const override;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* Interactor);
	virtual void Interact_Implementation(AActor* Interactor) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> PrototypeMesh;

	UPROPERTY(VisibleAnywhere, Category = "Animal Lead")
	FGameplayTag AnimalTag;

	UPROPERTY(VisibleAnywhere, Category = "Animal Lead")
	int32 ReportDay;

	UPROPERTY(VisibleAnywhere, Category = "Animal Lead")
	bool bExpired;
};
