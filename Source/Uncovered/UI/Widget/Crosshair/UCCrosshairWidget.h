#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/UCUserWidget.h"
#include "Interact/UCInteractionTypes.h"
#include "UCCrosshairWidget.generated.h"

class UImage;
class UTexture2D;

UCLASS()
class UNCOVERED_API UUCCrosshairWidget : public UUCUserWidget
{
	GENERATED_BODY()

// ─────────────────────────────────────────────────────────────
// Widget Interface
// ─────────────────────────────────────────────────────────────
public:
	/** 상태별 기본 크로스헤어 텍스처를 설정합니다. */
	UUCCrosshairWidget(const FObjectInitializer& ObjectInitializer);


// ─────────────────────────────────────────────────────────────
// Interaction State
// ─────────────────────────────────────────────────────────────
public:
	/** 이전 상태와 달라진 경우에만 크로스헤어 이미지를 갱신합니다. */
	void SetInteractionState(EUCInteractionState InteractionState);

private:
	/** 현재 상태에 해당하는 텍스처를 이미지에 적용합니다. */
	void ApplyInteractionState();

	/** 마지막으로 적용한 상호작용 표시 상태를 보관합니다. */
	EUCInteractionState CurrentInteractionState = EUCInteractionState::None;

	/** 화면 중앙에 표시할 이미지 위젯을 참조합니다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> CrosshairImage;

	/** 상호작용 대상을 바라보지 않을 때 표시할 텍스처를 지정합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "변수|크로스헤어")
	TObjectPtr<UTexture2D> NormalTexture;

	/** 현재 상호작용할 수 있는 대상을 바라볼 때 표시할 텍스처를 지정합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "변수|크로스헤어")
	TObjectPtr<UTexture2D> AvailableTexture;

	/** 상호작용 대상의 사용 조건을 충족하지 못할 때 표시할 텍스처를 지정합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "변수|크로스헤어")
	TObjectPtr<UTexture2D> UnavailableTexture;
};
