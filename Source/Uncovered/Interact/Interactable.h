#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(BlueprintType)
class UNCOVERED_API UInteractable : public UInterface
{
	GENERATED_BODY()
};

class UNCOVERED_API IInteractable
{
	GENERATED_BODY()

// ─────────────────────────────────────────────────────────────
// Interaction
// ─────────────────────────────────────────────────────────────
public:
	/** 상태를 변경하지 않고 현재 상호작용을 실행할 수 있는지 반환합니다. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "상호작용")
	bool CanInteract() const;

	/** 플레이어가 요청한 상호작용을 Blueprint에서 실행합니다. */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "상호작용")
	void Interact();
};
