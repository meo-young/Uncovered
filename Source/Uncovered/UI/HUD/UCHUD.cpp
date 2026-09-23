#include "UCHUD.h"

#include "UI/Widget/Crosshair/UCCrosshairWidget.h"

AUCHUD::AUCHUD()
{
	// 별도 위젯 에셋을 할당하지 않아도 기본 크로스헤어를 생성하도록 설정합니다.
	CrosshairWidgetClass = UUCCrosshairWidget::StaticClass();
}

void AUCHUD::BeginPlay()
{
	Super::BeginPlay();

	// 플레이어 화면을 기준으로 중앙에 고정하고 마우스 입력을 가로채지 않도록 설정합니다.
	CrosshairWidget = CreateWidget<UUCCrosshairWidget>(GetOwningPlayerController(), CrosshairWidgetClass);
	CrosshairWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	CrosshairWidget->AddToPlayerScreen();
}

void AUCHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// HUD 교체나 레벨 이동 시 이전 크로스헤어가 화면에 남지 않도록 제거합니다.
	CrosshairWidget->RemoveFromParent();
	Super::EndPlay(EndPlayReason);
}

void AUCHUD::SetInteractionState(EUCInteractionState InteractionState)
{
	// 위젯이 표시 상태를 관리하도록 판정 결과만 전달합니다.
	CrosshairWidget->SetInteractionState(InteractionState);
}
