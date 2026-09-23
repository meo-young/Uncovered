#include "UCCrosshairWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

UUCCrosshairWidget::UUCCrosshairWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// 프로젝트의 기존 텍스처를 상태별 기본 이미지로 설정합니다.
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> NormalAsset(TEXT("/Game/_Uncovered/Texture/Crosshair/T_Crosshair_Normal"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> AvailableAsset(TEXT("/Game/_Uncovered/Texture/Crosshair/T_Crosshair_Available"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> UnavailableAsset(TEXT("/Game/_Uncovered/Texture/Crosshair/T_Crosshair_Unavailable"));
		NormalTexture = NormalAsset.Object;
		AvailableTexture = AvailableAsset.Object;
		UnavailableTexture = UnavailableAsset.Object;
	}
}

void UUCCrosshairWidget::SetInteractionState(EUCInteractionState InteractionState)
{
	// 동일한 상태에서는 브러시 설정을 반복하지 않도록 갱신을 생략합니다.
	if (CurrentInteractionState == InteractionState)
	{
		return;
	}

	// 변경된 상태를 저장하고 해당 상태의 이미지를 적용합니다.
	CurrentInteractionState = InteractionState;
	ApplyInteractionState();
}

void UUCCrosshairWidget::ApplyInteractionState()
{
	// 일반 상태를 기본값으로 사용하고 상호작용 가능 여부에 따라 표시 텍스처를 선택합니다.
	UTexture2D* Texture = NormalTexture;
	switch (CurrentInteractionState)
	{
		case EUCInteractionState::Available:
			Texture = AvailableTexture;
			break;
		
		case EUCInteractionState::Unavailable:
			Texture = UnavailableTexture;
			break;
		
		default:
			break;
	}
	CrosshairImage->SetBrushFromTexture(Texture);
}
