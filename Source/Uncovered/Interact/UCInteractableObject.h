#pragma once

#include "CoreMinimal.h"
#include "Interactable.h"
#include "GameFramework/Actor.h"
#include "UCInteractableObject.generated.h"

class UBoxComponent;

/** 상호작용 감지 영역을 제공하고 자식 Blueprint에서 Interact 동작을 구현하도록 지원합니다. */
UCLASS(Blueprintable)
class UNCOVERED_API AUCInteractableObject : public AActor, public IInteractable
{
	GENERATED_BODY()

// ─────────────────────────────────────────────────────────────
// Interactable Interface
// ─────────────────────────────────────────────────────────────
public:
	/** 상호작용 오브젝트의 루트 컴포넌트와 감지용 박스 콜리전을 생성합니다. */
	AUCInteractableObject();

private:
	/** 자식 Blueprint에서 조건을 재정의하지 않으면 상호작용을 허용합니다. */
	virtual bool CanInteract_Implementation() const override;


// ─────────────────────────────────────────────────────────────
// Component
// ─────────────────────────────────────────────────────────────
private:
	/** 하위 컴포넌트의 상대 위치, 회전 및 크기를 설정할 기준 루트를 구성합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "변수|상호작용", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneComponent;
	
	/** Interactable 채널의 트레이스가 감지할 상호작용 영역을 정의합니다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "변수|상호작용", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> Mesh;
	
	/** Interactable 채널의 트레이스가 감지할 상호작용 영역을 정의합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "변수|상호작용", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BoxCollision;
	
};
