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
	/** 상호작용 오브젝트의 루트, 외형을 표시할 메시 및 메시를 따라 이동하는 감지용 박스 콜리전을 생성합니다. */
	AUCInteractableObject();

private:
	/** 자식 Blueprint에서 조건을 재정의하지 않으면 상호작용을 허용합니다. */
	virtual bool CanInteract_Implementation() const override;


// ─────────────────────────────────────────────────────────────
// Component
// ─────────────────────────────────────────────────────────────
private:
	/** 하위 컴포넌트의 상대 위치, 회전 및 크기를 설정할 기준 루트를 구성합니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneComponent;
	
	/** 상호작용 오브젝트의 외형을 표시하고 감지용 박스 콜리전의 부착 기준을 제공합니다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> Mesh;
	
};
