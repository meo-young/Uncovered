#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UCGridMapLayout.h"
#include "UCGridMapGenerator.generated.h"

class UInstancedStaticMeshComponent;

UCLASS()
class UNCOVERED_API AUCGridMapGenerator : public AActor
{
	GENERATED_BODY()

// ─────────────────────────────────────────────────────────────
// Generation
// ─────────────────────────────────────────────────────────────
public:
	/** 바닥 인스턴스와 생성기의 기준점을 구성합니다. */
	AUCGridMapGenerator();

	/** 지정한 시드로 검증된 맵을 생성하고 방 Actor를 배치합니다. */
	bool Generate(int32 InSeed);

	/** 현재 시드와 설정으로 맵을 생성합니다. */
	UFUNCTION(CallInEditor, Category = "맵 생성")
	void GenerateMap();

	/** 새로운 시드로 맵을 생성하고 재현용 시드를 기록합니다. */
	UFUNCTION(CallInEditor, Category = "맵 생성")
	void GenerateRandomMap();

	/** 현재 생성기가 소유한 방과 바닥만 제거합니다. */
	UFUNCTION(CallInEditor, Category = "맵 생성")
	void ClearGeneratedMap();

	/** 현재 맵의 시작 타일 중심을 월드 좌표로 반환합니다. */
	FVector GetStartWorldLocation() const;

	UPROPERTY(EditAnywhere, Category = "변수|생성")
	FUCGridMapSettings Settings;

	UPROPERTY(EditAnywhere, Category = "변수|생성")
	int32 GenerationSeed = 1337;

	/** 렌더링 또는 게임 로직에서 읽을 검증된 타일 데이터를 제공합니다. */
	const FUCGridMapLayout& GetLayout() const;


// ─────────────────────────────────────────────────────────────
// Validation
// ─────────────────────────────────────────────────────────────
public:
	/** 새로운 랜덤 시드마다 맵을 생성하여 지정한 횟수만큼 검사합니다. */
	UFUNCTION(CallInEditor, Category = "맵 검증")
	void ValidateRandomSeeds();

	UPROPERTY(EditAnywhere, Category = "변수|검증", meta = (ClampMin = "1", ClampMax = "10000"))
	int32 ValidationRuns = 100;

	UPROPERTY(VisibleInstanceOnly, Category = "변수|검증 결과")
	FString LastReport;

	UPROPERTY(VisibleInstanceOnly, Category = "변수|검증 결과")
	int32 FirstFailedSeed = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "변수|검증 결과")
	int32 FailedRuns = 0;


// ─────────────────────────────────────────────────────────────
// Presentation
// ─────────────────────────────────────────────────────────────
public:
	/** 현재 결과의 방, 복도, 출입구 및 시작점을 표시합니다. */
	UFUNCTION(CallInEditor, Category = "맵 표시")
	void DrawGeneratedMap();

	UPROPERTY(EditAnywhere, Category = "변수|표시", meta = (ClampMin = "1.0"))
	float DebugDuration = 60.0f;

	UPROPERTY(VisibleAnywhere, Transient, Category = "변수|바닥")
	TObjectPtr<UInstancedStaticMeshComponent> FloorTiles;

	/** 넓어진 통로와 랜덤 방 사이의 경계 벽 높이를 설정합니다. */
	UPROPERTY(EditAnywhere, Category = "변수|방 경계", meta = (ClampMin = "1.0"))
	float RoomWallHeight = 200.0f;

	UPROPERTY(VisibleAnywhere, Transient, Category = "변수|방 경계")
	TObjectPtr<UInstancedStaticMeshComponent> RoomWalls;


// ─────────────────────────────────────────────────────────────
// Generated State
// ─────────────────────────────────────────────────────────────
private:
	/** 에디터에서 생성기를 삭제할 때 사전 제작 방도 함께 제거합니다. */
	virtual void Destroyed() override;

	/** 생성기가 제거될 때 런타임에 생성한 사전 제작 방을 정리합니다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AUCDesignedRoom>> SpawnedRooms;

	FUCGridMapLayout Layout;
	float GeneratedTileSize = 200.0f;
};
