#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSPlayerHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHSPlayerHealthChangedSignature, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHSPlayerDiedSignature);

UCLASS(ClassGroup = (HimalayanSettlement), meta = (BlueprintSpawnableComponent))
class HIMALAYANSETTLEMENT_API UHSPlayerHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSPlayerHealthComponent();

	UFUNCTION(BlueprintPure, Category = "Player|Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Player|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Player|Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Player|Health")
	bool IsDead() const { return CurrentHealth <= 0.0f; }

	UFUNCTION(BlueprintCallable, Category = "Player|Health")
	float ApplyHealthDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Player|Health")
	float RestoreHealth(float HealAmount);

	UFUNCTION(BlueprintCallable, Category = "Player|Health")
	void ResetHealth();

	UPROPERTY(BlueprintAssignable, Category = "Player|Health")
	FHSPlayerHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Player|Health")
	FHSPlayerDiedSignature OnPlayerDied;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Player|Health", meta = (ClampMin = "1.0"))
	float MaxHealth;

	UPROPERTY(VisibleAnywhere, Category = "Player|Health")
	float CurrentHealth;
};
