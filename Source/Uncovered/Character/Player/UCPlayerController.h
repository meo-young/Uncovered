#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UCPlayerController.generated.h"

class UInputMappingContext;

UCLASS()
class UNCOVERED_API AUCPlayerController : public APlayerController
{
	GENERATED_BODY()

// ─────────────────────────────────────────────────────────────
// PlayerController Interface
// ─────────────────────────────────────────────────────────────
public:
	/** 플레이어 컨트롤러를 생성합니다. */
	AUCPlayerController();

protected:
	/** 로컬 플레이어에 기본 입력 매핑 컨텍스트를 등록합니다. */
	virtual void SetupInputComponent() override;


// ─────────────────────────────────────────────────────────────
// Input Mapping
// ─────────────────────────────────────────────────────────────
protected:
	/** 로컬 플레이어에 적용할 기본 입력 키와 액션의 매핑을 정의합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "변수|입력")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
};
