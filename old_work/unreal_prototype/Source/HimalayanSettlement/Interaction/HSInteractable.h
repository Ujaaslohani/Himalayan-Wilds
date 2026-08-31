#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HSInteractable.generated.h"

UINTERFACE(BlueprintType)
class HIMALAYANSETTLEMENT_API UHSInteractable : public UInterface
{
	GENERATED_BODY()
};

class HIMALAYANSETTLEMENT_API IHSInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionPrompt(const AActor* Interactor) const;
	virtual FText GetInteractionPrompt_Implementation(const AActor* Interactor) const
	{
		return FText::GetEmpty();
	}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(const AActor* Interactor) const;
	virtual bool CanInteract_Implementation(const AActor* Interactor) const
	{
		return true;
	}

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* Interactor);
	virtual void Interact_Implementation(AActor* Interactor)
	{
	}
};

namespace HSInteraction
{
inline FText GetInteractionPrompt(const UObject* Object, const AActor* Interactor)
{
	if (!Object || !Object->GetClass()->ImplementsInterface(UHSInteractable::StaticClass()))
	{
		return FText::GetEmpty();
	}

	if (const IHSInteractable* NativeInteractable = Cast<IHSInteractable>(Object))
	{
		return NativeInteractable->GetInteractionPrompt_Implementation(Interactor);
	}

	return IHSInteractable::Execute_GetInteractionPrompt(Object, Interactor);
}

inline bool CanInteract(const UObject* Object, const AActor* Interactor)
{
	if (!Object || !Object->GetClass()->ImplementsInterface(UHSInteractable::StaticClass()))
	{
		return false;
	}

	if (const IHSInteractable* NativeInteractable = Cast<IHSInteractable>(Object))
	{
		return NativeInteractable->CanInteract_Implementation(Interactor);
	}

	return IHSInteractable::Execute_CanInteract(Object, Interactor);
}

inline void Interact(UObject* Object, AActor* Interactor)
{
	if (!Object || !Object->GetClass()->ImplementsInterface(UHSInteractable::StaticClass()))
	{
		return;
	}

	if (IHSInteractable* NativeInteractable = Cast<IHSInteractable>(Object))
	{
		NativeInteractable->Interact_Implementation(Interactor);
		return;
	}

	IHSInteractable::Execute_Interact(Object, Interactor);
}
}
