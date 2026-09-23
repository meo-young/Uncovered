#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Actor/MapGenerator/UCGridMapGenerator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUCGridMapRandomTest, "Uncovered.GridMap.RandomSeeds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUCGridMapRandomTest::RunTest(const FString& Parameters)
{
	// 기본 설정에서 서로 다른 시드 100개와 과거 실패 시드를 검증합니다.
	FUCGridMapSettings Settings;
	FRandomStream Seeds(81273);
	const double Begin = FPlatformTime::Seconds();
	for (int32 I = 0; I < 100; ++I)
	{
		int32 Seed = static_cast<int32>(Seeds.GetUnsignedInt());
		if (I == 0) { Seed = -873325580; }
		FUCGridMapLayout Layout;
		FString Error;
		if (!UCGridMap::Build(Settings, Seed, Layout, Error)) { AddError(Error); return false; }
		if (!UCGridMap::Validate(Settings, Layout, Error)) { AddError(Error); return false; }
	}
	AddInfo(FString::Printf(TEXT("100 seeds: %.3f seconds"), FPlatformTime::Seconds() - Begin));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUCGridMapConstraintsTest, "Uncovered.GridMap.Constraints", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUCGridMapConstraintsTest::RunTest(const FString& Parameters)
{
	// 사용자가 제시한 작은 직사각형 맵에서도 고정 개수와 크기를 검사합니다.
	FUCGridMapSettings Small;
	Small.MapSize = FIntPoint(14, 20);
	Small.MinRooms = 4; Small.MaxRooms = 4;
	Small.MinRoomSize = FIntPoint(2, 2); Small.MaxRoomSize = FIntPoint(3, 3);
	Small.MaxCorridorLength = 40;
	FString Error;
	for (int32 Seed = 0; Seed < 20; ++Seed)
	{
		FUCGridMapLayout Layout;
		if (!UCGridMap::Build(Small, Seed, Layout, Error)) { AddError(Error); return false; }
		TestEqual(TEXT("Exact room count"), Layout.Rooms.Num(), 4);
		TestEqual(TEXT("Exact tile count"), Layout.Cells.Num(), 280);
		TestTrue(TEXT("Small map constraints"), UCGridMap::Validate(Small, Layout, Error));
	}

	// 같은 시드가 방, 출입구와 경로를 동일하게 재현하는지 검사합니다.
	FUCGridMapLayout A;
	FUCGridMapLayout B;
	TestTrue(TEXT("First deterministic build"), UCGridMap::Build(Small, 71, A, Error));
	TestTrue(TEXT("Second deterministic build"), UCGridMap::Build(Small, 71, B, Error));
	TestTrue(TEXT("Deterministic tiles"), A.Cells == B.Cells);
	TestTrue(TEXT("Deterministic start"), A.StartTile == B.StartTile);

	// 서로 다른 방 연결을 중복하거나 경로를 끊으면 검증이 실패해야 합니다.
	if (!A.Connections.IsEmpty())
	{
		FUCGridMapLayout Corrupt = A;
		const FUCGridConnection Duplicate = Corrupt.Connections[0];
		Corrupt.Connections.Add(Duplicate);
		TestFalse(TEXT("Duplicate link rejected"), UCGridMap::Validate(Small, Corrupt, Error));
		Corrupt = A;
		const FIntPoint Tile = Corrupt.Connections[0].Tiles[0];
		Corrupt.Cells[Tile.X + Tile.Y * Corrupt.Size.X] = INDEX_NONE;
		TestFalse(TEXT("Broken corridor rejected"), UCGridMap::Validate(Small, Corrupt, Error));
	}

	// 불가능한 설정은 실패하며 이전 결과를 변경하지 않아야 합니다.
	FUCGridMapSettings Invalid = Small;
	Invalid.MaxConnections = 1;
	TestFalse(TEXT("Impossible graph rejected"), UCGridMap::Build(Invalid, 19, A, Error));
	TestTrue(TEXT("Failed build preserves output"), A.Cells == B.Cells);
	Invalid = Small; Invalid.MapSize = FIntPoint(3, 3);
	TestFalse(TEXT("Impossible room area rejected"), UCGridMap::ValidateSettings(Invalid, Error));
	Invalid = Small; Invalid.MinCorridorLength = 41;
	TestFalse(TEXT("Reversed length range rejected"), UCGridMap::ValidateSettings(Invalid, Error));

	// 경로 상한 때문에 실패하더라도 제한된 탐색 후에 종료해야 합니다.
	Invalid = Small; Invalid.MinRoomGap = 3; Invalid.MaxCorridorLength = 1; Invalid.MaxLayoutAttempts = 2;
	TestFalse(TEXT("Unroutable layout terminates"), UCGridMap::Build(Invalid, 31, A, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUCGridMapDesignedTest, "Uncovered.GridMap.DesignedRooms", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUCGridMapDesignedTest::RunTest(const FString& Parameters)
{
	// 사전 제작 방의 크기와 필수 출입구 및 시작점 거리 조건을 검사합니다.
	FUCGridMapSettings Settings;
	FUCDesignedRoomEntry& Entry = Settings.DesignedRooms.AddDefaulted_GetRef();
	Entry.RoomClass = AUCDesignedRoom::StaticClass();
	Entry.Count = 2;
	Entry.MinStartDistance = 15;
	Entry.MaxStartDistance = 160;
	FString Error;
	for (int32 Seed = 0; Seed < 20; ++Seed)
	{
		FUCGridMapLayout Layout;
		if (!UCGridMap::Build(Settings, Seed, Layout, Error)) { AddError(Error); return false; }
		TestTrue(TEXT("Designed room constraints"), UCGridMap::Validate(Settings, Layout, Error));
		int32 Count = 0;
		for (const FUCGridRoom& Room : Layout.Rooms) { if (Room.DefinitionIndex == 0) { ++Count; TestTrue(TEXT("Authored size preserved"), Room.Size == FIntPoint(3, 3)); } }
		TestEqual(TEXT("Required instances"), Count, 2);
	}
	// 다른 타일 규격의 제작 방과 불가능한 거리 범위를 거부합니다.
	Settings.TileSize = 300.0f;
	TestFalse(TEXT("Authored scale mismatch rejected"), UCGridMap::ValidateSettings(Settings, Error));
	Settings.TileSize = 200.0f;
	Entry.MinStartDistance = 1000; Entry.MaxStartDistance = 0;
	Settings.MaxLayoutAttempts = 2;
	FUCGridMapLayout Layout;
	TestFalse(TEXT("Unreachable treasure distance rejected"), UCGridMap::Build(Settings, 42, Layout, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUCGridMapCycleTest, "Uncovered.GridMap.Cycles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUCGridMapCycleTest::RunTest(const FString& Parameters)
{
	// 모든 방이 정확히 두 방과 연결되면 하나의 순환 동선을 형성해야 합니다.
	FUCGridMapSettings Settings;
	Settings.MinRooms = 4; Settings.MaxRooms = 4;
	Settings.MinConnections = 2; Settings.MaxConnections = 2;
	FString Error;
	for (int32 Seed = 0; Seed < 10; ++Seed)
	{
		FUCGridMapLayout Layout;
		if (!UCGridMap::Build(Settings, Seed, Layout, Error)) { AddError(Error); return false; }
		TestEqual(TEXT("Cycle edge count"), Layout.Connections.Num(), 4);
		TestTrue(TEXT("Cycle geometry"), UCGridMap::Validate(Settings, Layout, Error));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUCGridMapActorTest, "Uncovered.GridMap.ActorLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUCGridMapActorTest::RunTest(const FString& Parameters)
{
	// 레벨 에셋을 변경하지 않는 임시 월드에서 Actor 배치와 교체를 검사합니다.
	const UWorld::InitializationValues Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
	AUCGridMapGenerator* Generator = World->SpawnActor<AUCGridMapGenerator>();
	FUCDesignedRoomEntry& Entry = Generator->Settings.DesignedRooms.AddDefaulted_GetRef();
	Entry.RoomClass = AUCDesignedRoom::StaticClass();
	Entry.MinStartDistance = 10;
	TestTrue(TEXT("Actor generation"), Generator->Generate(77));
	TestTrue(TEXT("Instanced floors"), Generator->FloorTiles->GetInstanceCount() > 0);
	// 랜덤 방의 출입구를 제외한 경계에만 벽을 배치하는지 검사합니다.
	int32 ExpectedWalls = 0;
	const FUCGridMapLayout& InitialLayout = Generator->GetLayout();
	for (int32 I = 0; I < InitialLayout.Rooms.Num(); ++I)
	{
		const FUCGridRoom& Room = InitialLayout.Rooms[I];
		if (Room.DefinitionIndex != INDEX_NONE) { continue; }
		ExpectedWalls += 2 * Room.Size.X + 2 * Room.Size.Y;
		for (const FUCGridConnection& Edge : InitialLayout.Connections) { if (Edge.RoomA == I || Edge.RoomB == I) { --ExpectedWalls; } }
	}
	TestEqual(TEXT("Room wall openings"), Generator->RoomWalls->GetInstanceCount(), ExpectedWalls);
	TArray<AActor*> Children;
	Generator->GetAttachedActors(Children);
	TestEqual(TEXT("Designed actor spawned"), Children.Num(), 1);
	const TArray<int32> PreviousCells = Generator->GetLayout().Cells;
	// 에디터 버튼과 같은 진입점으로 반복 횟수 설정 및 현재 맵 보존을 검사합니다.
	Generator->ValidationRuns = 10;
	Generator->ValidateRandomSeeds();
	TestEqual(TEXT("Ten validation runs"), Generator->FailedRuns, 0);
	TestTrue(TEXT("Validation preserves visible map"), PreviousCells == Generator->GetLayout().Cells);
	Generator->Settings.DesignedRooms.Reset();
	Generator->ValidationRuns = 1000;
	Generator->ValidateRandomSeeds();
	TestEqual(TEXT("Thousand validation runs"), Generator->FailedRuns, 0);
	Generator->Settings.MaxConnections = 1;
	TestFalse(TEXT("Failed generation"), Generator->Generate(78));
	TestTrue(TEXT("Preserved visible map"), PreviousCells == Generator->GetLayout().Cells);
	Generator->ClearGeneratedMap();
	Children.Reset();
	Generator->GetAttachedActors(Children);
	TestEqual(TEXT("Designed actors removed"), Children.Num(), 0);
	TestEqual(TEXT("Floor instances removed"), Generator->FloorTiles->GetInstanceCount(), 0);
	TestEqual(TEXT("Room walls removed"), Generator->RoomWalls->GetInstanceCount(), 0);
	// BeginPlay가 없는 에디터 삭제 경로에서도 제작 방 정리를 검사합니다.
	Generator->Settings = FUCGridMapSettings();
	FUCDesignedRoomEntry& NewEntry = Generator->Settings.DesignedRooms.AddDefaulted_GetRef();
	NewEntry.RoomClass = AUCDesignedRoom::StaticClass();
	TestTrue(TEXT("Regenerate before deletion"), Generator->Generate(77));
	Children.Reset();
	Generator->GetAttachedActors(Children);
	Generator->Destroy();
	for (AActor* Child : Children) { TestTrue(TEXT("Child destroyed with generator"), !IsValid(Child) || Child->IsActorBeingDestroyed()); }
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUCGridMapIrregularTest, "Uncovered.GridMap.IrregularCorridors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUCGridMapIrregularTest::RunTest(const FString& Parameters)
{
	FUCGridMapSettings Settings;
	FUCGridMapLayout Layout;
	FString Error;
	// 확장된 경로가 일관된 한 칸 폭이 아닌 연결된 바닥 영역인지 검사합니다.
	if (!UCGridMap::Build(Settings, 1337, Layout, Error)) { AddError(Error); return false; }
	int32 ExtraCount = 0;
	int32 WideSquares = 0;
	for (const FUCGridConnection& Edge : Layout.Connections) { ExtraCount += Edge.ExtraTiles.Num(); }
	for (int32 Y = 0; Y + 1 < Layout.Size.Y; ++Y)
	{
		for (int32 X = 0; X + 1 < Layout.Size.X; ++X)
		{
			const int32 I = X + Y * Layout.Size.X;
			const int32 Cell = Layout.Cells[I];
			if (Cell < INDEX_NONE && Layout.Cells[I + 1] == Cell && Layout.Cells[I + Layout.Size.X] == Cell && Layout.Cells[I + Layout.Size.X + 1] == Cell) { ++WideSquares; }
		}
	}
	TestTrue(TEXT("Corridor floor expansions"), ExtraCount > 0);
	TestTrue(TEXT("Areas wider than a one tile path"), WideSquares > 0);

	// 모든 방을 완전히 감싸지 않고 일부 측면만 길에 닿는 배치가 존재해야 합니다.
	bool bPartialSide = false;
	for (const FUCGridRoom& Room : Layout.Rooms)
	{
		int32 Filled = 0;
		const int32 PerimeterCount = 2 * Room.Size.X + 2 * Room.Size.Y + 4;
		for (int32 Y = -1; Y <= Room.Size.Y; ++Y)
		{
			for (int32 X = -1; X <= Room.Size.X; ++X)
			{
				if (X != -1 && Y != -1 && X != Room.Size.X && Y != Room.Size.Y) { continue; }
				const FIntPoint P = Room.Origin + FIntPoint(X, Y);
				if (P.X >= 0 && P.Y >= 0 && P.X < Layout.Size.X && P.Y < Layout.Size.Y && Layout.Cells[P.X + P.Y * Layout.Size.X] < INDEX_NONE) { ++Filled; }
			}
		}
		if (Filled > 1 && Filled < PerimeterCount) { bPartialSide = true; }
	}
	TestTrue(TEXT("Partial room side passages without mandatory rings"), bPartialSide);

	// 좌표 배열은 일치하더라도 떨어진 확장 타일이 있으면 검증이 실패해야 합니다.
	FUCGridMapLayout Broken = Layout;
	bool bInjectedIsland = false;
	for (int32 I = 0; I < Broken.Cells.Num() && !bInjectedIsland; ++I)
	{
		if (Broken.Cells[I] != INDEX_NONE) { continue; }
		const FIntPoint P(I % Broken.Size.X, I / Broken.Size.X);
		bool bTouchesPath = false;
		for (FIntPoint Step : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
		{
			const FIntPoint N = P + Step;
			if (N.X >= 0 && N.Y >= 0 && N.X < Broken.Size.X && N.Y < Broken.Size.Y && Broken.Cells[N.X + N.Y * Broken.Size.X] == -2) { bTouchesPath = true; }
		}
		if (!bTouchesPath)
		{
			Broken.Cells[I] = -2;
			Broken.Connections[0].ExtraTiles.Add(P);
			bInjectedIsland = true;
		}
	}
	TestTrue(TEXT("Disconnected expansion fixture"), bInjectedIsland);
	TestFalse(TEXT("Isolated corridor expansion rejected"), UCGridMap::Validate(Settings, Broken, Error));

	// 확장을 끄면 기존 한 칸 통로 모양도 재현할 수 있어야 합니다.
	Settings.MaxCorridorWidth = 1;
	Settings.RoomSidePassageProbability = 0.0f;
	for (int32 Seed = 0; Seed < 20; ++Seed)
	{
		if (!UCGridMap::Build(Settings, Seed, Layout, Error)) { AddError(Error); return false; }
		for (const FUCGridConnection& Edge : Layout.Connections) { TestTrue(TEXT("Disabled expansions"), Edge.ExtraTiles.IsEmpty()); }
		TestTrue(TEXT("Thin corridor validation"), UCGridMap::Validate(Settings, Layout, Error));
	}
	Settings.MaxWideSectionLength = 0;
	TestFalse(TEXT("Invalid segment lengths rejected"), UCGridMap::ValidateSettings(Settings, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUCGridMapWallDistanceTest, "Uncovered.GridMap.WallDistances", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUCGridMapWallDistanceTest::RunTest(const FString& Parameters)
{
	// 내부 장애물 때문에 벽 우회가 필요한 제작 방을 구성하고 테스트 후 기본값을 복원합니다.
	AUCDesignedRoom* Template = GetMutableDefault<AUCDesignedRoom>();
	TGuardValue<FIntPoint> RestoreSize(Template->Size, FIntPoint(7, 7));
	const TArray<FUCMapDoor> MazeDoors = {{FIntPoint(0, 3), FIntPoint(-1, 0)}, {FIntPoint(6, 3), FIntPoint(1, 0)}};
	TGuardValue<TArray<FUCMapDoor>> RestoreDoors(Template->Doors, MazeDoors);
	TArray<FIntPoint> MazeBlocks;
	for (int32 Y = 0; Y < 6; ++Y) { MazeBlocks.Add(FIntPoint(1, Y)); MazeBlocks.Add(FIntPoint(5, Y)); MazeBlocks.Add(FIntPoint(3, Y + 1)); }
	TGuardValue<TArray<FIntPoint>> RestoreBlocks(Template->BlockedTiles, MazeBlocks);
	FUCGridMapSettings Settings;
	Settings.DesignedRooms.AddDefaulted_GetRef().RoomClass = AUCDesignedRoom::StaticClass();
	Settings.DesignedRooms.AddDefaulted_GetRef().RoomClass = AUCDesignedRoom::StaticClass();
	FString Error;
	bool bFoundWallDetour = false;
	// 벽을 무시한 거리를 대조군으로 계산하여 검증이 벽을 실제로 우회하는지 확인합니다.
	for (int32 Seed = 0; Seed < 20 && !bFoundWallDetour; ++Seed)
	{
		FUCGridMapLayout Layout;
		if (!UCGridMap::Build(Settings, Seed, Layout, Error)) { AddError(Error); return false; }
		for (const FUCGridRoom& Room : Layout.Rooms) { if (Room.DefinitionIndex == 0) { Layout.StartTile = Room.Origin + FIntPoint(2, 1); } }
		const int32 Start = Layout.StartTile.X + Layout.StartTile.Y * Layout.Size.X;
		TArray<int32> Distances;
		TArray<int32> Queue;
		Distances.Init(INDEX_NONE, Layout.Cells.Num());
		Distances[Start] = 0;
		Queue.Add(Start);
		int32 OpenDistance = INDEX_NONE;
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 I = Queue[Head];
			const int32 Cell = Layout.Cells[I];
			if (Cell >= 0 && Layout.Rooms[Cell].DefinitionIndex == 1) { OpenDistance = Distances[I]; break; }
			for (FIntPoint Step : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
			{
				const FIntPoint P(I % Layout.Size.X + Step.X, I / Layout.Size.X + Step.Y);
				if (P.X < 0 || P.Y < 0 || P.X >= Layout.Size.X || P.Y >= Layout.Size.Y) { continue; }
				const int32 Next = P.X + P.Y * Layout.Size.X;
				if (Layout.Cells[Next] == INDEX_NONE || Distances[Next] != INDEX_NONE) { continue; }
				const int32 NextCell = Layout.Cells[Next];
				if (NextCell >= 0 && Layout.Rooms[NextCell].BlockedTiles.Contains(P - Layout.Rooms[NextCell].Origin)) { continue; }
				Distances[Next] = Distances[I] + 1;
				Queue.Add(Next);
			}
		}
		TestTrue(TEXT("Treasure reachable in open-wall control"), OpenDistance >= 0);
		Settings.DesignedRooms[1].MinStartDistance = OpenDistance + 1;
		bFoundWallDetour = UCGridMap::Validate(Settings, Layout, Error);
		Settings.DesignedRooms[1].MinStartDistance = 0;
		if (bFoundWallDetour)
		{
			// 같은 맵에서 벽을 무시한 거리를 최대값으로 지정하면 실패해야 합니다.
			Settings.DesignedRooms[1].MaxStartDistance = OpenDistance;
			TestFalse(TEXT("Distance maximum respects walls"), UCGridMap::Validate(Settings, Layout, Error));
		}
	}
	TestTrue(TEXT("Door-only movement differs from wall shortcut"), bFoundWallDetour);
	return true;
}

#endif
