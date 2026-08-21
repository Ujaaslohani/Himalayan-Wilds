#include "Player/HSPlayerHealthComponent.h"

UHSPlayerHealthComponent::UHSPlayerHealthComponent()
	: MaxHealth(100.0f)
	, CurrentHealth(100.0f)
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UHSPlayerHealthComponent::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f) : 0.0f;
}

float UHSPlayerHealthComponent::ApplyHealthDamage(float DamageAmount)
{
	if (DamageAmount <= 0.0f || IsDead())
	{
		return 0.0f;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
	const float AppliedDamage = PreviousHealth - CurrentHealth;
	if (AppliedDamage > 0.0f)
	{
		OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
		if (IsDead())
		{
			OnPlayerDied.Broadcast();
		}
	}

	return AppliedDamage;
}

float UHSPlayerHealthComponent::RestoreHealth(float HealAmount)
{
	if (HealAmount <= 0.0f || IsDead())
	{
		return 0.0f;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + HealAmount);
	const float RestoredHealth = CurrentHealth - PreviousHealth;
	if (RestoredHealth > 0.0f)
	{
		OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	}

	return RestoredHealth;
}

void UHSPlayerHealthComponent::ResetHealth()
{
	CurrentHealth = MaxHealth;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}
