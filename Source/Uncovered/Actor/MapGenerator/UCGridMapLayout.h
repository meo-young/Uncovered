#pragma once

#include "CoreMinimal.h"
#include "UCDesignedRoom.h"
#include "UCGridMapLayout.generated.h"

USTRUCT()
struct FUCDesignedRoomEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "변수|사전 제작 방")
	TSubclassOf<AUCDesignedRoom> RoomClass;

	UPROPERTY(EditAnywhere, Category = "변수|사전 제작 방", meta = (ClampMin = "1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, Category = "변수|시작점 거리", meta = (ClampMin = "0"))
	int32 MinStartDistance = 0;

	/** 0을 지정하면 시작점으로부터 최대 거리를 제한하지 않습니다. */
	UPROPERTY(EditAnywhere, Category = "변수|시작점 거리", meta = (ClampMin = "0"))
	int32 MaxStartDistance = 0;
};

USTRUCT()
struct FUCGridMapSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "변수|맵", meta = (ClampMin = "1"))
	FIntPoint MapSize = FIntPoint(64, 64);

	UPROPERTY(EditAnywhere, Category = "변수|맵", meta = (ClampMin = "1.0"))
	float TileSize = 200.0f;

	UPROPERTY(EditAnywhere, Category = "변수|방 개수", meta = (ClampMin = "2"))
	int32 MinRooms = 6;

	UPROPERTY(EditAnywhere, Category = "변수|방 개수", meta = (ClampMin = "2"))
	int32 MaxRooms = 10;

	UPROPERTY(EditAnywhere, Category = "변수|랜덤 방 크기", meta = (ClampMin = "2"))
	FIntPoint MinRoomSize = FIntPoint(3, 3);

	UPROPERTY(EditAnywhere, Category = "변수|랜덤 방 크기", meta = (ClampMin = "2"))
	FIntPoint MaxRoomSize = FIntPoint(6, 6);

	UPROPERTY(EditAnywhere, Category = "변수|방 연결", meta = (ClampMin = "1"))
	int32 MinConnections = 1;

	UPROPERTY(EditAnywhere, Category = "변수|방 연결", meta = (ClampMin = "1"))
	int32 MaxConnections = 4;

	UPROPERTY(EditAnywhere, Category = "변수|방 연결", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ExtraConnectionProbability = 0.25f;

	/** 구간별 확장 시 목표로 하는 최대 통로 폭을 설정합니다. 좁은 공간에서는 확장을 생략합니다. */
	UPROPERTY(EditAnywhere, Category = "변수|통로 형태", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxCorridorWidth = 3;

	UPROPERTY(EditAnywhere, Category = "변수|통로 형태", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CorridorWideningProbability = 0.65f;

	UPROPERTY(EditAnywhere, Category = "변수|통로 형태", meta = (ClampMin = "1"))
	int32 MinWideSectionLength = 2;

	UPROPERTY(EditAnywhere, Category = "변수|통로 형태", meta = (ClampMin = "1"))
	int32 MaxWideSectionLength = 6;

	/** 출입구 근처에서 방의 일부 측면으로 길을 뻗을 확률을 설정합니다. */
	UPROPERTY(EditAnywhere, Category = "변수|통로 형태", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RoomSidePassageProbability = 0.6f;

	UPROPERTY(EditAnywhere, Category = "변수|통로 형태", meta = (ClampMin = "0"))
	int32 MaxRoomSideLength = 6;

	UPROPERTY(EditAnywhere, Category = "변수|거리", meta = (ClampMin = "1"))
	int32 MinRoomGap = 2;

	UPROPERTY(EditAnywhere, Category = "변수|거리", meta = (ClampMin = "1"))
	int32 MinCorridorLength = 1;

	UPROPERTY(EditAnywhere, Category = "변수|거리", meta = (ClampMin = "1"))
	int32 MaxCorridorLength = 128;

	UPROPERTY(EditAnywhere, Category = "변수|사전 제작 방")
	TArray<FUCDesignedRoomEntry> DesignedRooms;

	UPROPERTY(EditAnywhere, Category = "변수|탐색 한도", meta = (ClampMin = "1"))
	int32 MaxLayoutAttempts = 64;

	UPROPERTY(EditAnywhere, Category = "변수|탐색 한도", meta = (ClampMin = "1"))
	int32 PlacementAttemptsPerRoom = 128;

	/** 한 시드에서 탐색하는 복도 노드 수를 제한합니다. */
	UPROPERTY(EditAnywhere, Category = "변수|탐색 한도", meta = (ClampMin = "1"))
	int32 MaxSearchNodes = 2000000;
};

struct FUCGridRoom
{
	FIntPoint Origin = FIntPoint::ZeroValue;
	FIntPoint Size = FIntPoint::ZeroValue;
	int32 DefinitionIndex = INDEX_NONE;
	TArray<FUCMapDoor> Doors;
	TArray<FIntPoint> BlockedTiles;
};

struct FUCGridConnection
{
	int32 RoomA = INDEX_NONE;
	int32 RoomB = INDEX_NONE;
	int32 DoorA = INDEX_NONE;
	int32 DoorB = INDEX_NONE;
	TArray<FIntPoint> Tiles;
	TArray<FIntPoint> ExtraTiles;
};

struct FUCGridMapLayout
{
	int32 Seed = 0;
	FIntPoint Size = FIntPoint::ZeroValue;
	FIntPoint StartTile = FIntPoint::ZeroValue;
	TArray<FUCGridRoom> Rooms;
	TArray<FUCGridConnection> Connections;
	TArray<int32> Cells;
};

namespace UCGridMap
{
	/** 명백하게 모순되는 설정과 사전 제작 방의 정의를 검사합니다. */
	bool ValidateSettings(const FUCGridMapSettings& Settings, FString& Error);

	/** 설정을 모두 만족하는 맵만 반환하며 실패 시 기존 출력 데이터를 유지합니다. */
	bool Build(const FUCGridMapSettings& Settings, int32 Seed, FUCGridMapLayout& Output, FString& Error);

	/** 방 개수와 실제 타일 연결, 출입구, 거리 조건을 독립적으로 검사합니다. */
	bool Validate(const FUCGridMapSettings& Settings, const FUCGridMapLayout& Layout, FString& Error);
}
