#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UCMapGenerator.generated.h"

UENUM()
enum class ERoomDirection : uint8
{
	Right,
	Left,
	Front,
	Forward
};

UENUM()
enum class ERoomTileDetail : uint8
{
	Center,
	Door,
	Wall,
	Wall_Blank,
	Window
};

UENUM()
enum class ETileType : uint8
{
	Blank,
	Corridor,
	Room,
	AreaConnection
};

USTRUCT()
struct FRoomTileDetail
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	ERoomTileDetail RoomTileDetail;
	
	UPROPERTY(EditAnywhere)
	ERoomDirection RoomDirection;
};

USTRUCT()
struct FRoomTileState
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	FVector2D RoomTileLocation;
	
	UPROPERTY(EditAnywhere)
	TArray<FRoomTileDetail> RoomTileDetails;
};

USTRUCT()
struct FRoomState
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	FVector2D RoomAreaCoordinate;
	
	UPROPERTY(EditAnywhere)
	uint8 MagatamaRoom : 1 = false;
	
	UPROPERTY(EditAnywhere)
	TArray<FRoomTileState> RoomTileState;
	
	UPROPERTY(EditAnywhere)
	TArray<FVector2D> AroundFloorCoordinates;
	
	UPROPERTY(EditAnywhere)
	TArray<ERoomDirection> AroundFloorDirections;
	
	UPROPERTY(EditAnywhere)
	TArray<ERoomDirection> ConnectingRoomDirections;
};

USTRUCT()
struct FTileState
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	FVector2D TileCoordinate;
	
	UPROPERTY(EditAnywhere)
	ETileType TileType;
};

USTRUCT()
struct FAreaState
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	FVector2D AreaCoordinate;
	
	UPROPERTY(EditAnywhere)
	TArray<FRoomState> Rooms;
	
	UPROPERTY(EditAnywhere)
	TArray<FTileState> Tiles;
	
	UPROPERTY(EditAnywhere)
	TArray<FVector2D> AreaConnectionTilesCoordinates;
};

USTRUCT()
struct FCoordinateThrough
{
	GENERATED_BODY()
	
	FVector2D AreaCoordinateThrough;
	FVector2D TileCoordinateThrough;
};

USTRUCT()
struct FConnectionCoordinate
{
	GENERATED_BODY()
	
	int32 Coordinate1;
	int32 Coordinate2;
};

USTRUCT()
struct FTileDetail
{
	GENERATED_BODY()
	
	ETileType RightTileType;
	ETileType LeftTileType;
	ETileType FrontTileType;
	ETileType ForwardTileType;
};

USTRUCT()
struct FNextTileInfo
{
	GENERATED_BODY()
	
	bool Finish;
	FVector2D NextCheckTile;
};

UCLASS()
class UNCOVERED_API AUCMapGenerator : public AActor
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void GenerateMap(AActor* GoalRootActor, uint8 SetSeed, int32 StreamSeed);

	/** 무작위 시드 100개로 맵을 생성하고 전체 연결 상태를 검증합니다. */
	UFUNCTION(CallInEditor, Category = "디버그|맵 검증")
	void ValidateRandomSeeds();
	
	void AreaDataFormat();
	
	FVector GetTileWorldLocation(FVector2D AreaCoordinate, FVector2D TileCoordinate);
	
	void DebugAreaBaseLocation();

	/** 생성된 맵의 방, 복도 및 연결 지점을 색상별로 표시합니다. */
	void DebugDrawGeneratedMap();
	
	FCoordinateThrough SetTileType(FVector2D AreaCoordinate, FVector2D TileCoordinate, ETileType TileType);
	void SetAreaConnectionTile();
	
	FConnectionCoordinate MakeRandomConnectionCoordinate();
	
	int32 CoordinateToIndex(FVector2D Coordinate, int32 Matrix);
	FVector2D IndexToCoordinate(int32 Index, int32 Matrix);
	
	FTileDetail GetNextTileDetail(FVector2D AreaCoordinate, FVector2D TileCoordinate);
	FTileDetail GetDiagonalTileDetail(FVector2D AreaCoordinate, FVector2D TileCoordinate);
	/** 각 방에 서로 직교하는 두 방향의 외곽 복도를 생성합니다. */
	void SetCorridorAroundRooms();
	/** 각 Area의 무작위 위치에 방 타일과 방 상태를 생성합니다. */
	void SetRoomTile();

	/** 각 방의 네 방향을 검사하고 인접한 방이 있는 방향을 기록합니다. */
	void CheckRoomConnection();
	
	TArray<FVector2D> GetRoomConnectionTargets(FAreaState AreaState);
	FNextTileInfo SearchNextTilePath(FVector2D AreaCoordinate, FVector2D StartTileCoordinate, FVector2D TargetTileCoordinate);
	
	void FixBroadCorridor();
	
	AActor* GoalRobotActor;
	FRandomStream Seed;
	TArray<FAreaState> AreaStates;
	int32 FlowCounterTemp1;
	FVector2D CurrentPathCheckTile;
	int32 FlowCounterTemp2;


// ─────────────────────────────────────────────────────────────
// Generation and Validation
// ─────────────────────────────────────────────────────────────
private:
	/** 맵 생성 데이터만 구성합니다. */
	void GenerateMapData();

	/** 생성된 모든 이동 가능 타일이 하나의 연결망인지 검사합니다. */
	bool ValidateGeneratedMapConnectivity(bool bDrawDisconnectedTiles);
};
