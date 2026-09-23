#include "UCInteractableObject.h"

#include "Components/BoxComponent.h"

AUCInteractableObject::AUCInteractableObject()
{
	// 액터의 기준 위치를 유지하면서 감지 영역을 독립적으로 조절할 수 있도록 별도의 루트를 생성합니다.
	{
		SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
		SetRootComponent(SceneComponent);
	}
	
	{
		Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
		Mesh->SetupAttachment(SceneComponent);
	}
	
	// 루트에 감지용 박스를 부착하고 물리 충돌과 오버랩 이벤트 없이 Interactable 채널의 트레이스만 차단하도록 설정합니다.
	{
		BoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
		BoxCollision->InitBoxExtent(FVector(50.0f));
		BoxCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BoxCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
		BoxCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
		BoxCollision->SetGenerateOverlapEvents(false);
		BoxCollision->SetupAttachment(Mesh);
	}
}

bool AUCInteractableObject::CanInteract_Implementation() const
{
	// 별도의 사용 조건이 없는 공통 상호작용 오브젝트는 기본적으로 실행을 허용합니다.
	return true;
}
