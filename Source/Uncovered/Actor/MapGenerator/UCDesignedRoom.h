#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UCDesignedRoom.generated.h"

USTRUCT()
struct FUCMapDoor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "변수|출입구")
	FIntPoint Tile = FIntPoint(0, 1);

	UPROPERTY(EditAnywhere, Category = "변수|출입구")
	FIntPoint Outward = FIntPoint(-1, 0);
};

UCLASS(Blueprintable)
class UNCOVERED_API AUCDesignedRoom : public AActor
{
	GENERATED_BODY()

// ─────────────────────────────────────────────────────────────
// Room Definition
// ─────────────────────────────────────────────────────────────
public:
	/** 소품과 메시를 배치할 방 기준점을 생성합니다. */
	AUCDesignedRoom();

	UPROPERTY(EditDefaultsOnly, Category = "변수|방 규격", meta = (ClampMin = "1"))
	FIntPoint Size = FIntPoint(3, 3);

	UPROPERTY(EditDefaultsOnly, Category = "변수|방 규격", meta = (ClampMin = "1.0"))
	float AuthoredTileSize = 200.0f;

	/** 방 내부의 이동 불가능한 타일을 지정합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "변수|방 규격")
	TArray<FIntPoint> BlockedTiles;

	/** 모든 출입구를 서로 다른 방에 한 번씩 연결합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "변수|방 규격")
	TArray<FUCMapDoor> Doors;
};
