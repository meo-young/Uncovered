#include "UCInteractableObject.h"

AUCInteractableObject::AUCInteractableObject()
{
	// 액터의 기준 위치를 유지하면서 메시와 하위 감지 영역의 변환을 조절할 수 있도록 별도의 루트를 생성합니다.
	{
		SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
		SetRootComponent(SceneComponent);
	}
	
	// 외형을 표시할 메시를 루트에 부착하여 하위 감지용 박스가 메시의 변환을 따르도록 구성합니다.
	{
		Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
		Mesh->SetupAttachment(SceneComponent);
		Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	}
}

bool AUCInteractableObject::CanInteract_Implementation() const
{
	// 별도의 사용 조건이 없는 공통 상호작용 오브젝트는 기본적으로 실행을 허용합니다.
	return true;
}
