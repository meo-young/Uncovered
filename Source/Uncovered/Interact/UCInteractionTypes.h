#pragma once

#include "CoreMinimal.h"
#include "UCInteractionTypes.generated.h"

/** 플레이어가 바라보는 대상의 상호작용 표시 상태를 구분합니다. */
UENUM(BlueprintType)
enum class EUCInteractionState : uint8
{
	None,
	Available,
	Unavailable
};
