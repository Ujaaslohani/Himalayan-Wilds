#include "Settlement/HSSettlementUnlockComponent.h"

#include "Core/HSLogCategories.h"

UHSSettlementUnlockComponent::UHSSettlementUnlockComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHSSettlementUnlockComponent::GrantUnlock(FGameplayTag UnlockTag)
{
	if (!UnlockTag.IsValid() || GrantedUnlocks.HasTagExact(UnlockTag))
	{
		return false;
	}

	GrantedUnlocks.AddTag(UnlockTag);
	OnSettlementUnlockGranted.Broadcast(UnlockTag);
	UE_LOG(LogHSSettlement, Log, TEXT("%s unlocked %s."), *GetNameSafe(GetOwner()), *UnlockTag.ToString());
	return true;
}

int32 UHSSettlementUnlockComponent::GrantUnlocks(const FGameplayTagContainer& UnlockTags)
{
	int32 GrantedCount = 0;
	for (const FGameplayTag& UnlockTag : UnlockTags)
	{
		GrantedCount += GrantUnlock(UnlockTag) ? 1 : 0;
	}
	return GrantedCount;
}

bool UHSSettlementUnlockComponent::HasUnlock(FGameplayTag UnlockTag) const
{
	return UnlockTag.IsValid() && GrantedUnlocks.HasTagExact(UnlockTag);
}
