#include "UCPlayerController.h"

#include "EnhancedInputSubsystems.h"

AUCPlayerController::AUCPlayerController()
{
}

void AUCPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 로컬 플레이어의 Enhanced Input 서브시스템에 기본 매핑 컨텍스트를 우선순위 0으로 등록합니다.
	UEnhancedInputLocalPlayerSubsystem* SubSystem = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

	SubSystem->AddMappingContext(DefaultMappingContext, 0);
}
