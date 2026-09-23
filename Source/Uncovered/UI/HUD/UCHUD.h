#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Interact/UCInteractionTypes.h"
#include "UCHUD.generated.h"

class UUCCrosshairWidget;

UCLASS()
class UNCOVERED_API AUCHUD : public AHUD
{
	GENERATED_BODY()

// ─────────────────────────────────────────────────────────────
// HUD Interface
// ─────────────────────────────────────────────────────────────
public:
	/** 기본 크로스헤어 위젯 클래스를 설정합니다. */
	AUCHUD();

protected:
	/** 소유 플레이어의 화면 중앙에 크로스헤어를 생성합니다. */
	virtual void BeginPlay() override;

	/** HUD가 종료될 때 화면에서 크로스헤어를 제거합니다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


// ─────────────────────────────────────────────────────────────
// Crosshair
// ─────────────────────────────────────────────────────────────
public:
	/** 플레이어가 판정한 상호작용 상태를 크로스헤어에 전달합니다. */
	void SetInteractionState(EUCInteractionState InteractionState);

private:
	/** 화면에 생성할 크로스헤어 위젯 클래스를 지정합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "변수|크로스헤어")
	TSubclassOf<UUCCrosshairWidget> CrosshairWidgetClass;

	/** 소유 플레이어의 화면에 표시하는 크로스헤어를 참조합니다. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "변수|크로스헤어")
	TObjectPtr<UUCCrosshairWidget> CrosshairWidget;
	
};
