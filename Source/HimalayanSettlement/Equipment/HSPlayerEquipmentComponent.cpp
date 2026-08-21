#include "Equipment/HSPlayerEquipmentComponent.h"

#include "Core/HSGameplayTags.h"
#include "Resources/HSItemStack.h"
#include "Resources/HSResourceCarryComponent.h"

UHSPlayerEquipmentComponent::UHSPlayerEquipmentComponent()
	: bBowEquipped(false)
	, EquippedWeaponMode(EHSPlayerWeaponMode::Unarmed)
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHSPlayerEquipmentComponent::HasBow() const
{
	const UHSResourceCarryComponent* ResourceCarry = ResolveResourceCarry();
	return ResourceCarry && (ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Basic) > 0 || ResourceCarry->GetQuantityForTag(TAG_HS_Item_Bow_Reinforced) > 0);
}

bool UHSPlayerEquipmentComponent::HasSpear() const
{
	const UHSResourceCarryComponent* ResourceCarry = ResolveResourceCarry();
	return ResourceCarry && ResourceCarry->GetQuantityForTag(TAG_HS_Item_Spear_Basic) > 0;
}

int32 UHSPlayerEquipmentComponent::GetArrowCount() const
{
	const UHSResourceCarryComponent* ResourceCarry = ResolveResourceCarry();
	return ResourceCarry ? ResourceCarry->GetQuantityForTag(TAG_HS_Item_Arrow_Basic) : 0;
}

bool UHSPlayerEquipmentComponent::CanUseBow() const
{
	return EquippedWeaponMode == EHSPlayerWeaponMode::Bow && HasBow() && GetArrowCount() > 0;
}

bool UHSPlayerEquipmentComponent::SetBowEquipped(bool bNewBowEquipped)
{
	return SetWeaponMode(bNewBowEquipped ? EHSPlayerWeaponMode::Bow : EHSPlayerWeaponMode::Unarmed);
}

bool UHSPlayerEquipmentComponent::SetSpearEquipped(bool bNewSpearEquipped)
{
	return SetWeaponMode(bNewSpearEquipped ? EHSPlayerWeaponMode::Spear : EHSPlayerWeaponMode::Unarmed);
}

bool UHSPlayerEquipmentComponent::SetWeaponMode(EHSPlayerWeaponMode NewWeaponMode)
{
	if (NewWeaponMode == EHSPlayerWeaponMode::Bow && !HasBow())
	{
		return false;
	}

	if (NewWeaponMode == EHSPlayerWeaponMode::Spear && !HasSpear())
	{
		return false;
	}

	if (EquippedWeaponMode == NewWeaponMode)
	{
		return true;
	}

	const bool bWasBowEquipped = bBowEquipped;
	EquippedWeaponMode = NewWeaponMode;
	bBowEquipped = EquippedWeaponMode == EHSPlayerWeaponMode::Bow;
	if (bWasBowEquipped != bBowEquipped)
	{
		OnBowEquippedChanged.Broadcast(bBowEquipped);
	}
	OnWeaponModeChanged.Broadcast(EquippedWeaponMode);
	return true;
}

bool UHSPlayerEquipmentComponent::ToggleBowEquipped()
{
	return SetBowEquipped(!bBowEquipped);
}

bool UHSPlayerEquipmentComponent::CycleWeaponMode()
{
	if (EquippedWeaponMode == EHSPlayerWeaponMode::Unarmed)
	{
		if (HasBow())
		{
			return SetWeaponMode(EHSPlayerWeaponMode::Bow);
		}
		return HasSpear() && SetWeaponMode(EHSPlayerWeaponMode::Spear);
	}

	if (EquippedWeaponMode == EHSPlayerWeaponMode::Bow)
	{
		if (HasSpear())
		{
			return SetWeaponMode(EHSPlayerWeaponMode::Spear);
		}
		return SetWeaponMode(EHSPlayerWeaponMode::Unarmed);
	}

	return SetWeaponMode(EHSPlayerWeaponMode::Unarmed);
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
