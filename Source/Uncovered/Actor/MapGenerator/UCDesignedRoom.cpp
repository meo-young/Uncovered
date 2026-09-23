#include "UCDesignedRoom.h"
#include "Components/SceneComponent.h"

AUCDesignedRoom::AUCDesignedRoom()
{
	// 기준 타일 중심을 메시와 소품의 공통 원점으로 설정합니다.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RoomRoot"));

	// 기본 방은 좌우 중앙에 출입구를 하나씩 구성합니다.
	{
		Doors.Add(FUCMapDoor{FIntPoint(0, 1), FIntPoint(-1, 0)});
		Doors.Add(FUCMapDoor{FIntPoint(2, 1), FIntPoint(1, 0)});
	}
}
