#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interaction/HSInteractable.h"
#include "HSNPCActor.generated.h"

class UHSNPCDefinition;
class UHSQuestDefinition;
class UHSQuestHandoffComponent;
class UHSNPCServiceComponent;
class UStaticMeshComponent;
class UStaticMesh;

UCLASS()
class HIMALAYANSETTLEMENT_API AHSNPCActor : public AActor, public IHSInteractable
{
	GENERATED_BODY()

public:
	AHSNPCActor();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	class UBoxComponent* GetInteractionBounds() const { return InteractionBounds; }

	UFUNCTION(BlueprintPure, Category = "Visual")
	UStaticMeshComponent* GetPrototypeMesh() const { return PrototypeMesh; }

	UFUNCTION(BlueprintPure, Category = "NPC")
	const UHSNPCDefinition* GetNPCDefinition() const { return NPCDefinition; }

	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCDefinition(UHSNPCDefinition* NewNPCDefinition);

	UFUNCTION(BlueprintPure, Category = "NPC")
	FText GetDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "NPC")
	FGameplayTag GetProfessionTag() const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	int32 GetInteractionCount() const { return InteractionCount; }

	UFUNCTION(BlueprintPure, Category = "Quest")
	UHSQuestHandoffComponent* GetQuestHandoffComponent() const { return QuestHandoffComponent; }

	UFUNCTION(BlueprintPure, Category = "NPC")
	UHSNPCServiceComponent* GetNPCServiceComponent() const { return NPCServiceComponent; }

	UFUNCTION(BlueprintCallable, Category = "Quest")
	void SetOfferedQuest(UHSQuestDefinition* NewQuestDefinition);

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
	void ApplyProfessionVisuals();
	void ApplyMeshColor(UStaticMeshComponent* MeshComponent, FLinearColor Color);
	void SnapFeetToGround();

	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	TObjectPtr<class UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> PrototypeMesh;

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> LegMesh;

	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> HatMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY(VisibleAnywhere, Category = "Quest")
	TObjectPtr<UHSQuestHandoffComponent> QuestHandoffComponent;

	UPROPERTY(VisibleAnywhere, Category = "NPC")
	TObjectPtr<UHSNPCServiceComponent> NPCServiceComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSNPCDefinition> NPCDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC", meta = (AllowPrivateAccess = "true"))
	FText FallbackDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	FText InteractionPromptFormat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	bool bCanTalk;

	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	int32 InteractionCount;
};
