#include "Equipment/HSPlayerEquipmentComponent.h"

#include "Core/HSGameplayTags.h"
#include "Resources/HSItemStack.h"
#include "Resources/HSResourceCarryComponent.h"

UHSPlayerEquipmentComponent::UHSPlayerEquipmentComponent()
	: bBowEquipped(false)
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHSPlayerEquipmentComponent::HasBow() const
{
	const UHSResourceCarryComponent* ResourceCarry = ResolveResourceCarry();
	return ResourceCarry && ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Basic) > 0;
}

int32 UHSPlayerEquipmentComponent::GetArrowCount() const
{
	const UHSResourceCarryComponent* ResourceCarry = ResolveResourceCarry();
	return ResourceCarry ? ResourceCarry->GetQuantityForTag(TAG_HS_Item_Arrow_Basic) : 0;
}

bool UHSPlayerEquipmentComponent::CanUseBow() const
{
	return bBowEquipped && HasBow() && GetArrowCount() > 0;
}

bool UHSPlayerEquipmentComponent::SetBowEquipped(bool bNewBowEquipped)
{
	if (bNewBowEquipped && !HasBow())
	{
		const bool bWasBowEquipped = bBowEquipped;
		bBowEquipped = false;
		if (bWasBowEquipped)
		{
			OnBowEquippedChanged.Broadcast(bBowEquipped);
		}
		return false;
	}

	if (bBowEquipped == bNewBowEquipped)
	{
		return true;
	}

	bBowEquipped = bNewBowEquipped;
	OnBowEquippedChanged.Broadcast(bBowEquipped);
	return true;
}

bool UHSPlayerEquipmentComponent::ToggleBowEquipped()
{
	return SetBowEquipped(!bBowEquipped);
}

bool UHSPlayerEquipmentComponent::ConsumeArrow()
{
	if (!CanUseBow())
	{
		return false;
	}

	UHSResourceCarryComponent* ResourceCarry = ResolveResourceCarry();
	return ResourceCarry && ResourceCarry->TryConsumeStack(FHSItemStack{ TAG_HS_Item_Arrow_Basic, 1 });
}

UHSResourceCarryComponent* UHSPlayerEquipmentComponent::ResolveResourceCarry() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UHSResourceCarryComponent>() : nullptr;
}
