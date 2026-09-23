#include "UCGridMapGenerator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/ConstructorHelpers.h"

AUCGridMapGenerator::AUCGridMapGenerator()
{
	// 타일 바닥을 인스턴싱하여 타일마다 Actor를 생성하는 비용을 줄입니다.
	{
		FloorTiles = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FloorTiles"));
		FloorTiles->SetFlags(RF_Transient);
		RootComponent = FloorTiles;
		FloorTiles->SetCollisionProfileName(TEXT("BlockAll"));
	}

	// 방 옆으로 넓어진 통로에서 출입구 이외의 경계를 통과하지 못하도록 벽을 준비합니다.
	{
		RoomWalls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RoomWalls"));
		RoomWalls->SetFlags(RF_Transient);
		RoomWalls->SetupAttachment(RootComponent);
		RoomWalls->SetCollisionProfileName(TEXT("BlockAll"));
	}

	// 메시 준비 전에도 맵을 살펴볼 수 있도록 기본 큐브 바닥을 설정합니다.
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> FloorMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
		FloorTiles->SetStaticMesh(FloorMesh.Object);
		RoomWalls->SetStaticMesh(FloorMesh.Object);
	}
}

bool AUCGridMapGenerator::Generate(int32 InSeed)
{
	// 회전하지 않는 제작 방의 규격을 유지하기 위해 생성기 변환을 검사합니다.
	if (!GetActorRotation().IsNearlyZero() || !GetActorScale3D().Equals(FVector::OneVector))
	{
		LastReport = TEXT("생성기는 위치만 변경할 수 있습니다. 회전 0, 스케일 1을 사용합니다.");
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastReport);
		return false;
	}
	FUCGridMapLayout Candidate;
	if (!FMath::IsFinite(RoomWallHeight) || RoomWallHeight < 1.0f)
	{
		LastReport = TEXT("방 경계 벽 높이는 1cm 이상이어야 합니다.");
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastReport);
		return false;
	}
	if (!UCGridMap::Build(Settings, InSeed, Candidate, LastReport))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastReport);
		return false;
	}

	// 모든 제작 방을 먼저 준비하고 생성 실패 시 이번 시도의 Actor만 정리합니다.
	TArray<TObjectPtr<AUCDesignedRoom>> NewRooms;
	for (const FUCGridRoom& Room : Candidate.Rooms)
	{
		if (Room.DefinitionIndex == INDEX_NONE) { continue; }
		FActorSpawnParameters Parameters;
		Parameters.Owner = this;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Parameters.ObjectFlags |= RF_Transient;
		const FVector Position = GetActorLocation() + FVector(Room.Origin.X * Settings.TileSize, Room.Origin.Y * Settings.TileSize, 0.0f);
		AUCDesignedRoom* Actor = GetWorld()->SpawnActor<AUCDesignedRoom>(Settings.DesignedRooms[Room.DefinitionIndex].RoomClass, Position, FRotator::ZeroRotator, Parameters);
		if (!IsValid(Actor))
		{
			for (AUCDesignedRoom* NewRoom : NewRooms) { if (IsValid(NewRoom)) { NewRoom->Destroy(); } }
			LastReport = TEXT("사전 제작 방 Actor 생성에 실패하여 기존 맵을 유지합니다.");
			UE_LOG(LogTemp, Error, TEXT("%s"), *LastReport);
			return false;
		}
		Actor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		NewRooms.Add(Actor);
	}

	// 성공한 데이터와 Actor가 모두 준비된 이후에만 기존 결과를 교체합니다.
	ClearGeneratedMap();
	Layout = MoveTemp(Candidate);
	SpawnedRooms = MoveTemp(NewRooms);
	GeneratedTileSize = Settings.TileSize;
	GenerationSeed = InSeed;
	if (UStaticMesh* Mesh = FloorTiles->GetStaticMesh())
	{
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const FVector MeshSize = Bounds.BoxExtent * 2.0;
		// 제작 방은 자체 바닥을 사용하고 랜덤 방과 복도만 공용 바닥을 배치합니다.
		if (MeshSize.X > UE_SMALL_NUMBER && MeshSize.Y > UE_SMALL_NUMBER && MeshSize.Z > UE_SMALL_NUMBER)
		{
			const FVector Scale(GeneratedTileSize / MeshSize.X, GeneratedTileSize / MeshSize.Y, 10.0 / MeshSize.Z);
			TArray<FTransform> Instances;
			Instances.Reserve(Layout.Cells.Num());
			for (int32 I = 0; I < Layout.Cells.Num(); ++I)
			{
				const int32 Cell = Layout.Cells[I];
				if (Cell == INDEX_NONE || (Cell >= 0 && Layout.Rooms[Cell].DefinitionIndex != INDEX_NONE)) { continue; }
				const FVector Center((I % Layout.Size.X) * GeneratedTileSize, (I / Layout.Size.X) * GeneratedTileSize, -5.0);
				Instances.Emplace(FQuat::Identity, Center - Bounds.Origin * Scale, Scale);
			}
			FloorTiles->AddInstances(Instances, false);
		}
	}
	// 랜덤 방은 실제 출입구만 남기고 경계에 벽을 배치합니다.
	if (RoomWalls->GetStaticMesh())
	{
		const FBoxSphereBounds Bounds = RoomWalls->GetStaticMesh()->GetBounds();
		const FVector MeshSize = Bounds.BoxExtent * 2.0;
		if (MeshSize.X > UE_SMALL_NUMBER && MeshSize.Y > UE_SMALL_NUMBER && MeshSize.Z > UE_SMALL_NUMBER)
		{
			TArray<FTransform> Instances;
			for (int32 I = 0; I < Layout.Rooms.Num(); ++I)
			{
				const FUCGridRoom& Room = Layout.Rooms[I];
				if (Room.DefinitionIndex != INDEX_NONE) { continue; }
				TSet<int32> OpenDoors;
				for (const FUCGridConnection& Edge : Layout.Connections)
				{
					if (Edge.RoomA == I) { OpenDoors.Add(Edge.DoorA); }
					if (Edge.RoomB == I) { OpenDoors.Add(Edge.DoorB); }
				}
				for (FIntPoint Direction : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
				{
					int32 Length = Room.Size.X;
					if (Direction.X != 0) { Length = Room.Size.Y; }
					for (int32 Offset = 0; Offset < Length; ++Offset)
					{
						FIntPoint Local(Offset, 0);
						if (Direction.Y > 0) { Local.Y = Room.Size.Y - 1; }
						if (Direction.X != 0)
						{
							Local = FIntPoint(0, Offset);
							if (Direction.X > 0) { Local.X = Room.Size.X - 1; }
						}
						bool bIsDoor = false;
						for (int32 DoorIndex : OpenDoors)
						{
							const FUCMapDoor& Door = Room.Doors[DoorIndex];
							if (Door.Tile == Local && Door.Outward == Direction) { bIsDoor = true; break; }
						}
						if (bIsDoor) { continue; }
						FVector Size(GeneratedTileSize, GeneratedTileSize * 0.05f, RoomWallHeight);
						if (Direction.X != 0) { Swap(Size.X, Size.Y); }
						const FVector Center((Room.Origin.X + Local.X + Direction.X * 0.5f) * GeneratedTileSize, (Room.Origin.Y + Local.Y + Direction.Y * 0.5f) * GeneratedTileSize, RoomWallHeight * 0.5f);
						const FVector Scale = Size / MeshSize;
						Instances.Emplace(FQuat::Identity, Center - Bounds.Origin * Scale, Scale);
					}
				}
			}
			RoomWalls->AddInstances(Instances, false);
		}
	}
	DrawGeneratedMap();
	LastReport = FString::Printf(TEXT("시드 %d: 방 %d개, 연결 %d개 생성 및 검증에 성공했습니다."), InSeed, Layout.Rooms.Num(), Layout.Connections.Num());
	UE_LOG(LogTemp, Warning, TEXT("%s"), *LastReport);
	return true;
}

void AUCGridMapGenerator::GenerateMap()
{
	// 디테일 패널에서 지정한 시드를 재현합니다.
	Generate(GenerationSeed);
}

void AUCGridMapGenerator::GenerateRandomMap()
{
	// 생성 실패 시에도 입력 시드를 디테일 패널에 남겨 재현합니다.
	FRandomStream Random;
	Random.GenerateNewSeed();
	GenerationSeed = Random.GetInitialSeed();
	Generate(GenerationSeed);
}

void AUCGridMapGenerator::ClearGeneratedMap()
{
	// 이 생성기가 기록한 Actor만 제거하여 다른 레벨 Actor를 보존합니다.
	for (AUCDesignedRoom* Room : SpawnedRooms) { if (IsValid(Room)) { Room->Destroy(); } }
	SpawnedRooms.Reset();
	FloorTiles->ClearInstances();
	RoomWalls->ClearInstances();
	Layout = FUCGridMapLayout();
}

FVector AUCGridMapGenerator::GetStartWorldLocation() const
{
	// 시작 타일 중심을 생성기 기준 월드 좌표로 변환합니다.
	return GetActorLocation() + FVector(Layout.StartTile.X * GeneratedTileSize, Layout.StartTile.Y * GeneratedTileSize, 0.0f);
}

const FUCGridMapLayout& AUCGridMapGenerator::GetLayout() const
{
	return Layout;
}

void AUCGridMapGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 런타임의 생성기 삭제 시 제작 방이 남지 않도록 정리합니다.
	ClearGeneratedMap();
	Super::EndPlay(EndPlayReason);
}

void AUCGridMapGenerator::Destroyed()
{
	// 에디터 월드에서도 소유한 미리보기 Actor를 함께 제거합니다.
	ClearGeneratedMap();
	Super::Destroyed();
}

void AUCGridMapGenerator::ValidateRandomSeeds()
{
	// 잘못된 설정은 반복 테스트에 진입하기 전에 한 번만 보고합니다.
	FailedRuns = 0;
	FirstFailedSeed = 0;
	if (ValidationRuns < 1 || ValidationRuns > 10000 || !UCGridMap::ValidateSettings(Settings, LastReport))
	{
		if (ValidationRuns < 1 || ValidationRuns > 10000) { LastReport = TEXT("검증 횟수는 1~10000회로 지정합니다."); }
		UE_LOG(LogTemp, Warning, TEXT("%s"), *LastReport);
		return;
	}
	FRandomStream Random;
	Random.GenerateNewSeed();
	TSet<int32> UsedSeeds;
	FString FirstError;
	int32 Completed = 0;
	const double BeginTime = FPlatformTime::Seconds();
#if WITH_EDITOR
	// 긴 검증 작업의 진행률과 취소 버튼을 에디터에 표시합니다.
	FScopedSlowTask Progress(ValidationRuns, FText::FromString(TEXT("랜덤 맵 생성 및 연결 검증")));
	Progress.MakeDialog(true);
#endif
	for (int32 I = 0; I < ValidationRuns; ++I)
	{
#if WITH_EDITOR
		if (Progress.ShouldCancel()) { break; }
		Progress.EnterProgressFrame(1.0f);
#endif
		// 같은 배치의 반복 검사를 피하도록 서로 다른 시드를 선택합니다.
		int32 TestSeed;
		do { TestSeed = static_cast<int32>(Random.GetUnsignedInt()); } while (UsedSeeds.Contains(TestSeed));
		UsedSeeds.Add(TestSeed);
		FUCGridMapLayout TestMap;
		FString Error;
		if (!UCGridMap::Build(Settings, TestSeed, TestMap, Error) || !UCGridMap::Validate(Settings, TestMap, Error))
		{
			if (FailedRuns == 0) { FirstFailedSeed = TestSeed; FirstError = Error; }
			++FailedRuns;
		}
		++Completed;
	}
	// 배치 검증은 Actor 생성 없이 실행하며 현재 표시 중인 맵을 유지합니다.
	const double Elapsed = FPlatformTime::Seconds() - BeginTime;
	LastReport = FString::Printf(TEXT("요청 %d회, 완료 %d회, 성공 %d회, 실패 %d회, 총 %.3f초, 평균 %.3fms"), ValidationRuns, Completed, Completed - FailedRuns, FailedRuns, Elapsed, Elapsed * 1000.0 / FMath::Max(Completed, 1));
	if (FailedRuns > 0) { LastReport += FString::Printf(TEXT(". 최초 실패 시드 %d: %s"), FirstFailedSeed, *FirstError); }
	UE_LOG(LogTemp, Warning, TEXT("%s"), *LastReport);
}

void AUCGridMapGenerator::DrawGeneratedMap()
{
	// 전역 디버그 선을 지우지 않고 이 맵을 유한 시간 동안 표시합니다.
	const auto World = [&](FIntPoint Tile) { return GetActorLocation() + FVector(Tile.X * GeneratedTileSize, Tile.Y * GeneratedTileSize, 20.0f); };
	for (int32 I = 0; I < Layout.Cells.Num(); ++I)
	{
		const int32 Cell = Layout.Cells[I];
		if (Cell == INDEX_NONE) { continue; }
		FColor Color = FColor::Green;
		const FIntPoint Tile(I % Layout.Size.X, I / Layout.Size.X);
		if (Cell >= 0)
		{
			Color = FColor::Blue;
			if (Layout.Rooms[Cell].DefinitionIndex != INDEX_NONE) { Color = FColor::Cyan; }
			if (Layout.Rooms[Cell].BlockedTiles.Contains(Tile - Layout.Rooms[Cell].Origin)) { Color = FColor::Red; }
		}
		DrawDebugBox(GetWorld(), World(Tile), FVector(GeneratedTileSize * 0.45f, GeneratedTileSize * 0.45f, 10.0f), Color, false, DebugDuration, 0, 2.0f);
	}
	// 각 방의 연결 수와 실제로 사용한 출입구를 함께 표시합니다.
	TArray<int32> Degrees;
	Degrees.Init(0, Layout.Rooms.Num());
	for (const FUCGridConnection& Edge : Layout.Connections)
	{
		++Degrees[Edge.RoomA]; ++Degrees[Edge.RoomB];
		for (FIntPoint Endpoint : {FIntPoint(Edge.RoomA, Edge.DoorA), FIntPoint(Edge.RoomB, Edge.DoorB)})
		{
			const FUCGridRoom& Room = Layout.Rooms[Endpoint.X];
			const FUCMapDoor& Door = Room.Doors[Endpoint.Y];
			DrawDebugDirectionalArrow(GetWorld(), World(Room.Origin + Door.Tile), World(Room.Origin + Door.Tile + Door.Outward), 30.0f, FColor::Magenta, false, DebugDuration, 0, 5.0f);
		}
	}
	for (int32 I = 0; I < Layout.Rooms.Num(); ++I) { DrawDebugString(GetWorld(), World(Layout.Rooms[I].Origin) + FVector(0, 0, 60), FString::Printf(TEXT("Room %d / Links %d"), I, Degrees[I]), nullptr, FColor::White, DebugDuration); }
	if (!Layout.Rooms.IsEmpty()) { DrawDebugSphere(GetWorld(), GetStartWorldLocation() + FVector(0, 0, 60), GeneratedTileSize * 0.25f, 12, FColor::Yellow, false, DebugDuration, 0, 4.0f); }
}
