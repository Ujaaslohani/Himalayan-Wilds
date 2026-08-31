#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "HSBootstrapOneMorningHuntCommandlet.generated.h"

UCLASS()
class UHSBootstrapOneMorningHuntCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UHSBootstrapOneMorningHuntCommandlet();

	virtual int32 Main(const FString& Params) override;
};
