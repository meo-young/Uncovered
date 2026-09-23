#include "UCPlayer.h"

#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interact/Interactable.h"
#include "UI/HUD/UCHUD.h"

AUCPlayer::AUCPlayer()
{
	// 매 프레임 카메라 중앙의 상호작용 대상을 검사하도록 Tick을 활성화합니다.
	PrimaryActorTick.bCanEverTick = true;
}

void AUCPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Enhanced Input 컴포넌트를 사용하여 이동과 시점 액션의 Triggered 이벤트에 처리 함수를 바인딩합니다.
	UEnhancedInputComponent* EIC = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::DoMove);
	EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::DoLook);
	EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::DoInteract);
}

void AUCPlayer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 빙의 전이나 원격 캐릭터의 Tick에서는 로컬 카메라 검사를 생략합니다.
	if (!IsLocallyControlled())
	{
		return;
	}

	// 같은 대상을 바라보는 동안에도 조건이 바뀔 수 있으므로 매 프레임 가능 여부를 다시 확인합니다.
	EUCInteractionState InteractionState = EUCInteractionState::None;
	if (AActor* Interactable = FindInteractable())
	{
		InteractionState = EUCInteractionState::Unavailable;
		if (IInteractable::Execute_CanInteract(Interactable))
		{
			InteractionState = EUCInteractionState::Available;
		}
	}

	// 로컬 HUD가 아직 생성되지 않은 시점에는 표시 갱신만 생략합니다.
	if (AUCHUD* HUD = CastChecked<APlayerController>(GetController())->GetHUD<AUCHUD>())
	{
		HUD->SetInteractionState(InteractionState);
	}
}

void AUCPlayer::DoMove(const FInputActionValue& Value)
{
	// 이동 입력의 X축은 캐릭터의 전방 이동에, Y축은 우측 이동에 적용합니다.
	const FVector2D& MoveValue = Value.Get<FVector2D>();

	AddMovementInput(GetActorForwardVector(), MoveValue.X);
	AddMovementInput(GetActorRightVector(), MoveValue.Y);
}

void AUCPlayer::DoLook(const FInputActionValue& Value)
{
	// 시점 입력의 Y축은 피치 회전에, X축은 요 회전에 적용합니다.
	const FVector2D& LookValue = Value.Get<FVector2D>();

	AddControllerPitchInput(LookValue.Y);
	AddControllerYawInput(LookValue.X);
}

void AUCPlayer::DoInteract()
{
	// 입력 시 대상을 다시 검사하여 사라지거나 시야에서 벗어난 대상의 호출을 방지합니다.
	if (AActor* Interactable = FindInteractable())
	{
		// 마지막 UI 상태와 무관하게 입력 시점의 조건을 확인하여 불가능한 상호작용을 차단합니다.
		if (IInteractable::Execute_CanInteract(Interactable))
		{
			IInteractable::Execute_Interact(Interactable);
		}
	}
}

AActor* AUCPlayer::FindInteractable() const
{
	// 실제 플레이어 카메라의 중앙 방향으로 지정 거리만큼 검사하고 자기 자신은 제외합니다.
	FVector ViewLocation;
	FRotator ViewRotation;
	CastChecked<APlayerController>(GetController())->GetPlayerViewPoint(ViewLocation, ViewRotation);
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InteractionTrace), false, this);
	GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * InteractionDistance, ECC_GameTraceChannel1, QueryParams);

	// 가장 먼저 충돌한 액터가 인터페이스를 구현한 경우에만 상호작용 대상으로 반환합니다.
	AActor* HitActor = Hit.GetActor();
	if (IsValid(HitActor) && HitActor->Implements<UInteractable>())
	{
		return HitActor;
	}

	// 충돌이 없거나 벽처럼 상호작용할 수 없는 대상이 앞을 막으면 빈 결과를 반환합니다.
	return nullptr;
}
