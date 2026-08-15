#include "Resources/HSResourceCarryComponent.h"

UHSResourceCarryComponent::UHSResourceCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHSResourceCarryComponent::AddStack(const FHSItemStack& Stack)
{
	if (!Stack.IsValidStack())
	{
		return;
	}

	for (FHSItemStack& ExistingStack : CarriedItems)
	{
		if (ExistingStack.ItemTag == Stack.ItemTag)
		{
			ExistingStack.Quantity += Stack.Quantity;
			return;
		}
	}

	CarriedItems.Add(Stack);
}

void UHSResourceCarryComponent::AddStacks(const TArray<FHSItemStack>& Stacks)
{
	for (const FHSItemStack& Stack : Stacks)
	{
		AddStack(Stack);
	}
}

bool UHSResourceCarryComponent::TryConsumeStack(const FHSItemStack& Stack)
{
	if (!HasAtLeast(Stack))
	{
		return false;
	}

	for (int32 StackIndex = 0; StackIndex < CarriedItems.Num(); ++StackIndex)
	{
		FHSItemStack& ExistingStack = CarriedItems[StackIndex];
		if (ExistingStack.ItemTag == Stack.ItemTag)
		{
			ExistingStack.Quantity -= Stack.Quantity;
			if (ExistingStack.Quantity <= 0)
			{
				CarriedItems.RemoveAt(StackIndex);
			}
			return true;
		}
	}

	return false;
}

bool UHSResourceCarryComponent::TryConsumeStacks(const TArray<FHSItemStack>& Stacks)
{
	for (const FHSItemStack& Stack : Stacks)
	{
		if (!HasAtLeast(Stack))
		{
			return false;
		}
	}

	for (const FHSItemStack& Stack : Stacks)
	{
		if (!TryConsumeStack(Stack))
		{
			return false;
		}
	}

	return true;
}

int32 UHSResourceCarryComponent::GetQuantityForTag(FGameplayTag ItemTag) const
{
	if (!ItemTag.IsValid())
	{
		return 0;
	}

	for (const FHSItemStack& ExistingStack : CarriedItems)
	{
		if (ExistingStack.ItemTag == ItemTag)
		{
			return ExistingStack.Quantity;
		}
	}

	return 0;
}

bool UHSResourceCarryComponent::HasAtLeast(const FHSItemStack& Stack) const
{
	return Stack.IsValidStack() && GetQuantityForTag(Stack.ItemTag) >= Stack.Quantity;
}
