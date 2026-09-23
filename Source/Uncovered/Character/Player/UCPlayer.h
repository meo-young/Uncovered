#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UCPlayer.generated.h"

struct FInputActionValue;
class UInputAction;

UCLASS()
class UNCOVERED_API AUCPlayer : public ACharacter
{
	GENERATED_BODY()

// ─────────────────────────────────────────────────────────────
// Player Interface
// ─────────────────────────────────────────────────────────────
public:
	/** 플레이어 캐릭터를 생성합니다. */
	AUCPlayer();

protected:
	/** 이동, 시점 및 상호작용 입력 액션을 처리 함수에 바인딩합니다. */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** 카메라 중앙의 대상과 상호작용 가능 여부를 검사하여 크로스헤어 상태를 전달합니다. */
	virtual void Tick(float DeltaSeconds) override;


// ─────────────────────────────────────────────────────────────
// Input Action
// ─────────────────────────────────────────────────────────────
private:
	/** 캐릭터의 전방과 우측 방향을 기준으로 이동 입력을 적용합니다. */
	void DoMove(const FInputActionValue& Value);

	/** 시점 입력을 컨트롤러의 피치와 요 회전에 적용합니다. */
	void DoLook(const FInputActionValue& Value);

	/** 현재 카메라 중앙에서 감지한 대상의 상호작용을 실행합니다. */
	void DoInteract();

	/** 캐릭터의 전후 및 좌우 이동 입력을 정의합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "변수|입력")
	TObjectPtr<UInputAction> MoveAction;

	/** 시점의 상하 및 좌우 회전 입력을 정의합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "변수|입력")
	TObjectPtr<UInputAction> LookAction;

	/** 대상과의 상호작용을 요청하는 입력을 정의합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "변수|입력")
	TObjectPtr<UInputAction> InteractAction;


// ─────────────────────────────────────────────────────────────
// Interaction
// ─────────────────────────────────────────────────────────────
private:
	/** 카메라 중앙에서 지정 거리 이내의 첫 충돌 대상이 상호작용 인터페이스를 구현하는지 검사하여 반환합니다. */
	AActor* FindInteractable() const;

	/** 카메라를 기준으로 상호작용을 검사할 최대 거리를 센티미터 단위로 설정합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "변수|상호작용", meta = (ClampMin = "0.0", Units = "cm"))
	float InteractionDistance = 100.0f;
};
