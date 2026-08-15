#include "Progression/HSPlayerProgressComponent.h"

#include "Core/HSLogCategories.h"

UHSPlayerProgressComponent::UHSPlayerProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHSPlayerProgressComponent::MarkMilestoneCompleted(FGameplayTag MilestoneTag)
{
	if (!MilestoneTag.IsValid() || CompletedMilestones.HasTagExact(MilestoneTag))
	{
		return false;
	}

	CompletedMilestones.AddTag(MilestoneTag);
	OnMilestoneCompleted.Broadcast(MilestoneTag);
	UE_LOG(LogHSQuest, Log, TEXT("%s completed milestone %s."), *GetNameSafe(GetOwner()), *MilestoneTag.ToString());
	return true;
}

bool UHSPlayerProgressComponent::HasCompletedMilestone(FGameplayTag MilestoneTag) const
{
	return MilestoneTag.IsValid() && CompletedMilestones.HasTagExact(MilestoneTag);
}
