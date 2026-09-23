#include "UCMapGenerator.h"

#include "DrawDebugHelpers.h"
#include "Containers/Queue.h"

void AUCMapGenerator::GenerateMap(AActor* GoalRootActor, uint8 SetSeed, int32 StreamSeed)
{
	// 요청한 시드 설정에 따라 맵 생성용 난수 스트림을 초기화합니다.
	if (SetSeed)
	{
		Seed.Initialize(StreamSeed);
	}
	else
	{
		Seed.GenerateNewSeed();
	}

	GoalRobotActor = GoalRootActor;
	GenerateMapData();

	// 완성된 맵의 방, 복도 및 연결 지점을 시각화합니다.
	DebugDrawGeneratedMap();
}

void AUCMapGenerator::GenerateMapData()
{
	AreaDataFormat();
	SetAreaConnectionTile();
	SetRoomTile();
	CheckRoomConnection();
	SetCorridorAroundRooms();
	
	FlowCounterTemp1 = 0;

	// 각 Area의 방과 Area 연결 타일을 모두 연결한 뒤 다음 Area를 처리합니다.
	while (FlowCounterTemp1 < AreaStates.Num())
	{
		const FVector2D AreaCoordinate = AreaStates[FlowCounterTemp1].AreaCoordinate;
		const TArray<FVector2D> RoomConnectionTargets = GetRoomConnectionTargets(AreaStates[FlowCounterTemp1]);

		const auto ConnectPath = [this, AreaCoordinate](const FVector2D& StartTarget, const FVector2D& EndTarget)
		{
			CurrentPathCheckTile = StartTarget;
			constexpr int32 MaxPathStepCount = 18 * 18;

			// Area의 전체 타일 수를 상한으로 두고 목표 지점까지 경로를 검색합니다.
			for (int32 PathStep = 0; PathStep < MaxPathStepCount; ++PathStep)
			{
				const FNextTileInfo NextTileInfo = SearchNextTilePath(AreaCoordinate, CurrentPathCheckTile, EndTarget);

				if (NextTileInfo.Finish)
				{
					return;
				}

				// 같은 타일이 반복되면 무한 반복을 방지하기 위해 현재 연결을 중단합니다.
				if (NextTileInfo.NextCheckTile == CurrentPathCheckTile)
				{
					UE_LOG(LogTemp, Error, TEXT("경로 검색이 같은 타일에서 반복되어 중단합니다. Area: %s, Tile: %s"), *AreaCoordinate.ToString(), *CurrentPathCheckTile.ToString());
					return;
				}

				CurrentPathCheckTile = NextTileInfo.NextCheckTile;
			}

			// 전체 타일 수 안에 경로를 완료하지 못하면 비정상 순환으로 판단합니다.
			UE_LOG(LogTemp, Error, TEXT("경로 검색 최대 횟수를 초과했습니다. Area: %s, Start: %s, End: %s"), *AreaCoordinate.ToString(), *StartTarget.ToString(), *EndTarget.ToString());
		};

		const auto ConnectGroups = [&RoomConnectionTargets, &ConnectPath](const FIntRect& FirstGroup, const FIntRect& SecondGroup)
		{
			FVector2D FirstTarget = FVector2D::ZeroVector;
			FVector2D SecondTarget = FVector2D::ZeroVector;
			float ClosestDistanceSquared = TNumericLimits<float>::Max();

			// 두 그룹에서 실제 방이 존재하는 연결점 중 가장 가까운 조합을 찾습니다.
			for (int32 FirstY = FirstGroup.Min.Y; FirstY < FirstGroup.Max.Y; ++FirstY)
			{
				for (int32 FirstX = FirstGroup.Min.X; FirstX < FirstGroup.Max.X; ++FirstX)
				{
					const FVector2D& FirstCandidate = RoomConnectionTargets[FirstX + FirstY * 4];
					if (FirstCandidate.IsZero())
					{
						continue;
					}

					for (int32 SecondY = SecondGroup.Min.Y; SecondY < SecondGroup.Max.Y; ++SecondY)
					{
						for (int32 SecondX = SecondGroup.Min.X; SecondX < SecondGroup.Max.X; ++SecondX)
						{
							const FVector2D& SecondCandidate = RoomConnectionTargets[SecondX + SecondY * 4];
							if (SecondCandidate.IsZero())
							{
								continue;
							}

							const float DistanceSquared = FVector2D::DistSquared(FirstCandidate, SecondCandidate);
							if (DistanceSquared < ClosestDistanceSquared)
							{
								ClosestDistanceSquared = DistanceSquared;
								FirstTarget = FirstCandidate;
								SecondTarget = SecondCandidate;
							}
						}
					}
				}
			}

			// 어느 한 그룹에 방이 없다면 이번 병합을 건너뜁니다.
			if (FirstTarget.IsZero() || SecondTarget.IsZero())
			{
				return;
			}

			ConnectPath(FirstTarget, SecondTarget);
		};

		// 가로로 인접한 방을 연결하여 16개 칸을 최대 8개 그룹으로 병합합니다.
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Column = 0; Column < 4; Column += 2)
			{
				ConnectGroups(FIntRect(Column, Row, Column + 1, Row + 1), FIntRect(Column + 1, Row, Column + 2, Row + 1));
			}
		}

		// 세로로 인접한 행의 그룹을 연결하여 최대 8개 그룹을 4개 그룹으로 병합합니다.
		for (int32 Row = 0; Row < 4; Row += 2)
		{
			for (int32 Column = 0; Column < 4; Column += 2)
			{
				ConnectGroups(FIntRect(Column, Row, Column + 2, Row + 1), FIntRect(Column, Row + 1, Column + 2, Row + 2));
			}
		}

		// 위아래 2x2 그룹을 연결하여 4개 그룹을 좌우 2개 그룹으로 병합합니다.
		for (int32 Column = 0; Column < 4; Column += 2)
		{
			ConnectGroups(FIntRect(Column, 0, Column + 2, 2), FIntRect(Column, 2, Column + 2, 4));
		}

		// 좌우 그룹을 연결하여 모든 방을 하나의 그룹으로 병합합니다.
		ConnectGroups(FIntRect(0, 0, 2, 4), FIntRect(2, 0, 4, 4));
		
		FlowCounterTemp2 = 0;

		// 모든 Area 연결 타일을 가장 가까운 방 그룹에 차례로 연결합니다.
		while (FlowCounterTemp2 < AreaStates[FlowCounterTemp1].AreaConnectionTilesCoordinates.Num())
		{
			const TArray<FVector2D> RoomConnectionTargets2 = GetRoomConnectionTargets(AreaStates[FlowCounterTemp1]);
			const FVector2D& AreaConnectionTarget = AreaStates[FlowCounterTemp1].AreaConnectionTilesCoordinates[FlowCounterTemp2];
			float ClosestDistanceSquared = TNumericLimits<float>::Max();
			int32 ClosestTargetIndex = INDEX_NONE;

			// 방이 존재하는 연결점 중 현재 Area 연결 타일과 가장 가까운 지점을 찾습니다.
			for (int32 TargetIndex = 0; TargetIndex < RoomConnectionTargets2.Num(); ++TargetIndex)
			{
				if (RoomConnectionTargets2[TargetIndex].IsZero())
				{
					continue;
				}

				const float DistanceSquared = FVector2D::DistSquared(RoomConnectionTargets2[TargetIndex], AreaConnectionTarget);
				if (DistanceSquared < ClosestDistanceSquared)
				{
					ClosestDistanceSquared = DistanceSquared;
					ClosestTargetIndex = TargetIndex;
				}
			}

			// 연결 가능한 방이 없다면 남은 Area 연결 처리를 중단합니다.
			if (ClosestTargetIndex == INDEX_NONE)
			{
				break;
			}

			// 경로 연결이 완료되면 다음 Area 연결 타일을 처리합니다.
			ConnectPath(AreaConnectionTarget, RoomConnectionTargets2[ClosestTargetIndex]);
			++FlowCounterTemp2;
		}

		// 현재 Area 연결을 마친 뒤 다음 Area의 방 연결 단계로 이동합니다.
		++FlowCounterTemp1;
	}
	
	FixBroadCorridor();
}

void AUCMapGenerator::ValidateRandomSeeds()
{
	constexpr int32 ValidationCount = 100;
	FRandomStream ValidationSeedGenerator;
	ValidationSeedGenerator.GenerateNewSeed();

	int32 SuccessCount = 0;
	int32 FailureCount = 0;
	int32 FirstFailureSeed = 0;
	bool bHasFailureSeed = false;

	// 검증 중에는 Goal Actor를 이동하지 않고 맵 데이터만 반복 생성합니다.
	GoalRobotActor = nullptr;

	for (int32 ValidationIndex = 0; ValidationIndex < ValidationCount; ++ValidationIndex)
	{
		const int32 ValidationSeed = static_cast<int32>(ValidationSeedGenerator.GetUnsignedInt());
		Seed.Initialize(ValidationSeed);
		GenerateMapData();

		if (ValidateGeneratedMapConnectivity(false))
		{
			++SuccessCount;
		}
		else
		{
			++FailureCount;

			if (!bHasFailureSeed)
			{
				FirstFailureSeed = ValidationSeed;
				bHasFailureSeed = true;
			}
		}
	}

	// 실패한 시드가 있으면 첫 실패 맵을 다시 생성하여 화면과 로그에 상세 결과를 표시합니다.
	if (bHasFailureSeed)
	{
		Seed.Initialize(FirstFailureSeed);
		GenerateMapData();
		DebugDrawGeneratedMap();
		ValidateGeneratedMapConnectivity(true);
		UE_LOG(LogTemp, Error, TEXT("랜덤 시드 검증에 실패했습니다. 성공: %d, 실패: %d, 첫 실패 시드: %d"), SuccessCount, FailureCount, FirstFailureSeed);
		return;
	}

	// 모든 검증에 성공하면 마지막 생성 결과를 표시합니다.
	DebugDrawGeneratedMap();
	UE_LOG(LogTemp, Warning, TEXT("랜덤 시드 100회 연결 검증에 성공했습니다."));
}

void AUCMapGenerator::AreaDataFormat()
{
	constexpr int32 TileCountPerArea = 18;
	constexpr int32 AreaCount = 4;

	AreaStates.Reset();
	AreaStates.Reserve(AreaCount * AreaCount);

	TArray<FTileState> TileStateTemp;
	TileStateTemp.Reserve(TileCountPerArea * TileCountPerArea);

	for (int32 TileY = 1; TileY <= TileCountPerArea; ++TileY)
	{
		for (int32 TileX = 1; TileX <= TileCountPerArea; ++TileX)
		{
			TileStateTemp.Emplace(FVector2D(TileX, TileY), ETileType::Blank);
		}
	}

	for (int32 AreaY = 1; AreaY <= AreaCount; ++AreaY)
	{
		for (int32 AreaX = 1; AreaX <= AreaCount; ++AreaX)
		{
			FAreaState& AreaState = AreaStates.Emplace_GetRef();
			AreaState.AreaCoordinate = FVector2D(AreaX, AreaY);
			AreaState.Tiles = TileStateTemp;
		}
	}
}

FVector AUCMapGenerator::GetTileWorldLocation(FVector2D AreaCoordinate, FVector2D TileCoordinate)
{
	constexpr float TileSize = 180.0f;
	constexpr float TileCountPerArea = 18.0f;
	constexpr float AreaSize = TileSize * TileCountPerArea;

	FVector Location = FVector::ZeroVector;

	// 맵 좌표와 월드 좌표의 X, Y축을 반대로 적용합니다.
	Location.X = AreaSize * (AreaCoordinate.Y - 1.0f);
	Location.Y = AreaSize * (AreaCoordinate.X - 1.0f);

	Location.X += TileSize * (TileCoordinate.Y - 1.0f);
	Location.Y += TileSize * (TileCoordinate.X - 1.0f);

	// PlayerStart 위치를 기준으로 월드 위치를 보정합니다.
	Location.Y -= AreaSize * 2.0f + TileSize * 8.0f;

	return Location;
}

void AUCMapGenerator::DebugAreaBaseLocation()
{
	for (const FAreaState& AreaState : AreaStates)
	{
		const FVector2D& AreaCoordinate = AreaState.AreaCoordinate;

		DrawDebugSphere(GetWorld(), GetTileWorldLocation(AreaCoordinate, FVector2D(1.0f, 1.0f)), 100.0f, 12, FColor::Green, true, 500.0f);
		DrawDebugSphere(GetWorld(), GetTileWorldLocation(AreaCoordinate, FVector2D(18.0f, 1.0f)) + FVector(0.0f, 180.0f, 0.0f), 100.0f, 12, FColor::Green, true, 500.0f);
		DrawDebugSphere(GetWorld(), GetTileWorldLocation(AreaCoordinate, FVector2D(1.0f, 18.0f)) + FVector(180.0f, 0.0f, 0.0f), 100.0f, 12, FColor::Green, true, 500.0f);
		DrawDebugSphere(GetWorld(), GetTileWorldLocation(AreaCoordinate, FVector2D(18.0f, 18.0f)) + FVector(180.0f, 180.0f, 0.0f), 100.0f, 12, FColor::Green, true, 500.0f);
	}
}

void AUCMapGenerator::DebugDrawGeneratedMap()
{
	constexpr float TileHalfExtent = 75.0f;
	constexpr float TileHeight = 20.0f;
	constexpr float TargetRadius = 55.0f;

	// 이전 맵의 영구 디버그 표시를 제거합니다.
	FlushPersistentDebugLines(GetWorld());

	// 생성된 방, 복도 및 Area 연결 타일을 종류별 색상으로 표시합니다.
	for (const FAreaState& AreaState : AreaStates)
	{
		for (const FTileState& TileState : AreaState.Tiles)
		{
			FColor TileColor;

			switch (TileState.TileType)
			{
			case ETileType::Room:
				TileColor = FColor::Blue;
				break;
			case ETileType::Corridor:
				TileColor = FColor::Green;
				break;
			case ETileType::AreaConnection:
				TileColor = FColor::Red;
				break;
			default:
				continue;
			}

			const FVector DebugLocation = GetTileWorldLocation(AreaState.AreaCoordinate, TileState.TileCoordinate) + FVector(0.0f, 0.0f, TileHeight);
			DrawDebugBox(GetWorld(), DebugLocation, FVector(TileHalfExtent, TileHalfExtent, TileHeight), TileColor, true, -1.0f, 0, 4.0f);
		}

		// 경로 탐색에서 사용하는 각 방의 마지막 연결 후보를 분홍 구체로 표시합니다.
		for (const FRoomState& RoomState : AreaState.Rooms)
		{
			if (RoomState.AroundFloorCoordinates.IsEmpty())
			{
				continue;
			}

			const FVector TargetLocation = GetTileWorldLocation(AreaState.AreaCoordinate, RoomState.AroundFloorCoordinates.Last()) + FVector(0.0f, 0.0f, 80.0f);
			DrawDebugSphere(GetWorld(), TargetLocation, TargetRadius, 12, FColor::Magenta, true, -1.0f, 0, 5.0f);
		}
	}
}

bool AUCMapGenerator::ValidateGeneratedMapConnectivity(bool bDrawDisconnectedTiles)
{
	constexpr int32 TileCountPerArea = 18;
	static const FIntPoint NeighborOffsets[] =
	{
		FIntPoint(1, 0),
		FIntPoint(-1, 0),
		FIntPoint(0, 1),
		FIntPoint(0, -1)
	};

	TMap<FIntPoint, ETileType> WalkableTiles;
	FIntPoint StartCoordinate = FIntPoint::ZeroValue;
	bool bHasStartCoordinate = false;

	// Area와 내부 타일 좌표를 전체 맵 기준의 연속된 좌표로 변환합니다.
	for (const FAreaState& AreaState : AreaStates)
	{
		for (const FTileState& TileState : AreaState.Tiles)
		{
			if (TileState.TileType == ETileType::Blank)
			{
				continue;
			}

			const FIntPoint GlobalCoordinate(
				FMath::RoundToInt((AreaState.AreaCoordinate.X - 1.0f) * TileCountPerArea + TileState.TileCoordinate.X - 1.0f),
				FMath::RoundToInt((AreaState.AreaCoordinate.Y - 1.0f) * TileCountPerArea + TileState.TileCoordinate.Y - 1.0f));

			WalkableTiles.Add(GlobalCoordinate, TileState.TileType);

			if (!bHasStartCoordinate)
			{
				StartCoordinate = GlobalCoordinate;
				bHasStartCoordinate = true;
			}
		}
	}

	// 이동 가능한 타일이 없으면 검증에 실패합니다.
	if (!bHasStartCoordinate)
	{
		if (bDrawDisconnectedTiles)
		{
			UE_LOG(LogTemp, Error, TEXT("맵 연결 검증에 실패했습니다. 이동 가능한 타일이 없습니다."));
		}

		return false;
	}

	TSet<FIntPoint> VisitedCoordinates;
	TQueue<FIntPoint> PendingCoordinates;
	VisitedCoordinates.Add(StartCoordinate);
	PendingCoordinates.Enqueue(StartCoordinate);

	// 첫 이동 가능 타일에서 시작하여 상하좌우로 연결된 모든 타일을 탐색합니다.
	FIntPoint CurrentCoordinate;
	while (PendingCoordinates.Dequeue(CurrentCoordinate))
	{
		const ETileType CurrentTileType = WalkableTiles[CurrentCoordinate];

		for (const FIntPoint& NeighborOffset : NeighborOffsets)
		{
			const FIntPoint NeighborCoordinate = CurrentCoordinate + NeighborOffset;
			const ETileType* NeighborTileType = WalkableTiles.Find(NeighborCoordinate);

			if (!NeighborTileType || VisitedCoordinates.Contains(NeighborCoordinate))
			{
				continue;
			}

			const bool bCrossesArea = CurrentCoordinate.X / TileCountPerArea != NeighborCoordinate.X / TileCountPerArea || CurrentCoordinate.Y / TileCountPerArea != NeighborCoordinate.Y / TileCountPerArea;
			if (bCrossesArea && (CurrentTileType != ETileType::AreaConnection || *NeighborTileType != ETileType::AreaConnection))
			{
				continue;
			}

			VisitedCoordinates.Add(NeighborCoordinate);
			PendingCoordinates.Enqueue(NeighborCoordinate);
		}
	}

	const int32 DisconnectedTileCount = WalkableTiles.Num() - VisitedCoordinates.Num();
	if (DisconnectedTileCount == 0)
	{
		if (bDrawDisconnectedTiles)
		{
			UE_LOG(LogTemp, Warning, TEXT("맵 연결 검증에 성공했습니다. 이동 가능한 타일 %d개가 하나의 경로망으로 연결되어 있습니다."), WalkableTiles.Num());
		}

		return true;
	}

	int32 DisconnectedRequiredTileCount = 0;

	// 고립된 방과 Area 연결 타일을 집계하고 상세 검증 시 주황색 구체로 표시합니다.
	for (const FAreaState& AreaState : AreaStates)
	{
		for (const FTileState& TileState : AreaState.Tiles)
		{
			if (TileState.TileType != ETileType::Room && TileState.TileType != ETileType::AreaConnection)
			{
				continue;
			}

			const FIntPoint GlobalCoordinate(
				FMath::RoundToInt((AreaState.AreaCoordinate.X - 1.0f) * TileCountPerArea + TileState.TileCoordinate.X - 1.0f),
				FMath::RoundToInt((AreaState.AreaCoordinate.Y - 1.0f) * TileCountPerArea + TileState.TileCoordinate.Y - 1.0f));

			if (VisitedCoordinates.Contains(GlobalCoordinate))
			{
				continue;
			}

			++DisconnectedRequiredTileCount;

			if (bDrawDisconnectedTiles)
			{
				const FVector DebugLocation = GetTileWorldLocation(AreaState.AreaCoordinate, TileState.TileCoordinate) + FVector(0.0f, 0.0f, 120.0f);
				DrawDebugSphere(GetWorld(), DebugLocation, 80.0f, 12, FColor::Orange, true, -1.0f, 0, 8.0f);
			}
		}
	}

	if (bDrawDisconnectedTiles)
	{
		UE_LOG(LogTemp, Error, TEXT("맵 연결 검증에 실패했습니다. 전체 이동 타일: %d, 연결 타일: %d, 고립 타일: %d, 고립된 방 및 Area 연결 타일: %d"), WalkableTiles.Num(), VisitedCoordinates.Num(), DisconnectedTileCount, DisconnectedRequiredTileCount);
	}

	return false;
}

FCoordinateThrough AUCMapGenerator::SetTileType(FVector2D AreaCoordinate, FVector2D TileCoordinate, ETileType TileType)
{
	const int32 AreaIndex = CoordinateToIndex(AreaCoordinate, 4);
	const int32 TileIndex = CoordinateToIndex(TileCoordinate, 18);

	// 유효한 Area와 타일 좌표일 때 지정한 타일 타입을 적용합니다.
	if (AreaStates.IsValidIndex(AreaIndex) && AreaStates[AreaIndex].Tiles.IsValidIndex(TileIndex))
	{
		AreaStates[AreaIndex].Tiles[TileIndex].TileType = TileType;
	}

	return FCoordinateThrough(AreaCoordinate, TileCoordinate);
}

void AUCMapGenerator::SetAreaConnectionTile()
{
	FCoordinateThrough CoordinateThrough = SetTileType(FVector2D(3.0f, 1.0f), FVector2D(9.0f, 1.0f), ETileType::AreaConnection);
	
	AreaStates[CoordinateToIndex(CoordinateThrough.AreaCoordinateThrough, 4)].AreaConnectionTilesCoordinates.Add(CoordinateThrough.TileCoordinateThrough);
	
	CoordinateThrough = SetTileType(FVector2D(Seed.RandRange(1, 4), 4.0f), FVector2D(Seed.RandRange(6, 13), 18.0f), ETileType::AreaConnection);
	
	AreaStates[CoordinateToIndex(CoordinateThrough.AreaCoordinateThrough, 4)].AreaConnectionTilesCoordinates.Add(CoordinateThrough.TileCoordinateThrough);
	FVector2D GoalAreaTemp = CoordinateThrough.AreaCoordinateThrough;
	
	FVector GoalLocation = GetTileWorldLocation(CoordinateThrough.AreaCoordinateThrough, CoordinateThrough.TileCoordinateThrough);
	
	// 에디터 검증 중 Goal Actor가 없으면 위치 이동을 생략합니다.
	if (IsValid(GoalRobotActor))
	{
		GoalRobotActor->SetActorLocation(GoalLocation + FVector(180.0f, 0.0f, 0.0f));
	}
	
	for (uint8 i = 0; i < AreaStates.Num(); ++i)
	{
		if (AreaStates[i].AreaCoordinate.Y != 4.0f)
		{
			FConnectionCoordinate ConnectionCoordinate = MakeRandomConnectionCoordinate();
			for (auto Tile : AreaStates[i].Tiles)
			{
				if (Tile.TileCoordinate.Y == 18.0f && (Tile.TileCoordinate.X == ConnectionCoordinate.Coordinate1 || Tile.TileCoordinate.X == ConnectionCoordinate.Coordinate2))
				{
					CoordinateThrough = SetTileType(AreaStates[i].AreaCoordinate, Tile.TileCoordinate, ETileType::AreaConnection);
					
					AreaStates[CoordinateToIndex(CoordinateThrough.AreaCoordinateThrough, 4)].AreaConnectionTilesCoordinates.Add(CoordinateThrough.TileCoordinateThrough);
				}
			}
			
			CoordinateThrough = SetTileType(AreaStates[i].AreaCoordinate + FVector2D(0.0f, 1.0f), FVector2D(ConnectionCoordinate.Coordinate1, 1.0f), ETileType::AreaConnection);
			AreaStates[CoordinateToIndex(CoordinateThrough.AreaCoordinateThrough, 4)].AreaConnectionTilesCoordinates.Add(CoordinateThrough.TileCoordinateThrough);
			
			CoordinateThrough = SetTileType(AreaStates[i].AreaCoordinate + FVector2D(0.0f, 1.0f), FVector2D(ConnectionCoordinate.Coordinate2, 1.0f), ETileType::AreaConnection);
			AreaStates[CoordinateToIndex(CoordinateThrough.AreaCoordinateThrough, 4)].AreaConnectionTilesCoordinates.Add(CoordinateThrough.TileCoordinateThrough);
		}
		
		
		if (AreaStates[i].AreaCoordinate.X != 4.0f)
		{
			FConnectionCoordinate ConnectionCoordinate = MakeRandomConnectionCoordinate();
			for (auto Tile : AreaStates[i].Tiles)
			{
				if (Tile.TileCoordinate.X == 18.0f && (Tile.TileCoordinate.Y == ConnectionCoordinate.Coordinate1 || Tile.TileCoordinate.Y == ConnectionCoordinate.Coordinate2))
				{
					CoordinateThrough = SetTileType(AreaStates[i].AreaCoordinate, Tile.TileCoordinate, ETileType::AreaConnection);
					
					AreaStates[CoordinateToIndex(CoordinateThrough.AreaCoordinateThrough, 4)].AreaConnectionTilesCoordinates.Add(CoordinateThrough.TileCoordinateThrough);
				}
			}
			
			CoordinateThrough = SetTileType(AreaStates[i].AreaCoordinate + FVector2D(1.0f, 0.0f), FVector2D(1.0f, ConnectionCoordinate.Coordinate1), ETileType::AreaConnection);
			AreaStates[CoordinateToIndex(CoordinateThrough.AreaCoordinateThrough, 4)].AreaConnectionTilesCoordinates.Add(CoordinateThrough.TileCoordinateThrough);
			
			CoordinateThrough = SetTileType(AreaStates[i].AreaCoordinate + FVector2D(1.0f, 0.0f), FVector2D(1.0f, ConnectionCoordinate.Coordinate2), ETileType::AreaConnection);
			AreaStates[CoordinateToIndex(CoordinateThrough.AreaCoordinateThrough, 4)].AreaConnectionTilesCoordinates.Add(CoordinateThrough.TileCoordinateThrough);
		}
	}
}

FConnectionCoordinate AUCMapGenerator::MakeRandomConnectionCoordinate()
{
	int32 CoordinateTemp1, CoordinateTemp2;
	
	CoordinateTemp1 = Seed.RandRange(2, 7);
	CoordinateTemp2 = Seed.RandRange(12, 17);

	return FConnectionCoordinate(CoordinateTemp1, CoordinateTemp2);
}

int32 AUCMapGenerator::CoordinateToIndex(FVector2D Coordinate, int32 Matrix)
{
	// 왼쪽 아래부터 오른쪽 순서로 증가하는 좌표 관계를 변환하여 반환합니다.
	return FMath::TruncToInt(Coordinate.X + (Coordinate.Y - 1.0f) * Matrix - 1.0f);
}

FVector2D AUCMapGenerator::IndexToCoordinate(int32 Index, int32 Matrix)
{
	// 0부터 시작하는 인덱스를 1부터 시작하는 맵 좌표로 변환합니다.
	return FVector2D(Index % Matrix + 1, Index / Matrix + 1);
}

FTileDetail AUCMapGenerator::GetNextTileDetail(FVector2D AreaCoordinate, FVector2D TileCoordinate)
{
	// Area 경계 밖 방향은 이동할 수 없는 타일로 취급합니다.
	ETileType Left = ETileType::Room;
	ETileType Right = ETileType::Room;
	ETileType Forward = ETileType::Room;
	ETileType Front = ETileType::Room;
	
	
	for (int32 i=0; i<=3; ++i)
	{
		FVector2D Temp = TileCoordinate;
		switch (i)
		{
		case 0:
			Temp = TileCoordinate + FVector2D(-1.0f, 0.0f);
			break;
			case 1:
			Temp = TileCoordinate + FVector2D(1.0f, 0.0f);
			break;
			case 2:
			Temp = TileCoordinate + FVector2D(0.0f, 1.0f);
			break;
			case 3:
			Temp = TileCoordinate + FVector2D(0.0f, -1.0f);
			break;
		}
		
		if (Temp.X >= 1.0f && Temp.X <= 18.0f && Temp.Y >= 1.0f && Temp.Y <= 18.0f)
		{
			switch (i)
			{
			case 0:
				Left = AreaStates[CoordinateToIndex(AreaCoordinate, 4)].Tiles[CoordinateToIndex(Temp, 18)].TileType;
				break;
			case 1:
				Right =AreaStates[CoordinateToIndex(AreaCoordinate, 4)].Tiles[CoordinateToIndex(Temp, 18)].TileType;
				break;
			case 2:
				Forward = AreaStates[CoordinateToIndex(AreaCoordinate, 4)].Tiles[CoordinateToIndex(Temp, 18)].TileType;
				break;
				case 3:
				Front = AreaStates[CoordinateToIndex(AreaCoordinate, 4)].Tiles[CoordinateToIndex(Temp, 18)].TileType;
				break;
			}
		}

	}
	
	
	// FTileDetail의 Right, Left, Front, Forward 멤버 순서에 맞춰 반환합니다.
	return FTileDetail(Right, Left, Front, Forward);
}

FTileDetail AUCMapGenerator::GetDiagonalTileDetail(FVector2D AreaCoordinate, FVector2D TileCoordinate)
{
	// Area 경계 밖 대각선 방향은 복도가 아닌 타일로 취급합니다.
	ETileType Front_Left = ETileType::Room;
	ETileType Front_Right = ETileType::Room;
	ETileType Forward_Left = ETileType::Room;
	ETileType Forward_Right = ETileType::Room;
	
	
	for (int32 i=0; i<=3; ++i)
	{
		FVector2D Temp = TileCoordinate;
		switch (i)
		{
		case 0:
			Temp = TileCoordinate + FVector2D(-1.0f, -1.0f);
			break;
		case 1:
			Temp = TileCoordinate + FVector2D(1.0f, -1.0f);
			break;
		case 2:
			Temp = TileCoordinate + FVector2D(-1.0f, 1.0f);
			break;
		case 3:
			Temp = TileCoordinate + FVector2D(1.0f, 1.0f);
			break;
		}
		
		if (Temp.X >= 1.0f && Temp.X <= 18.0f && Temp.Y >= 1.0f && Temp.Y <= 18.0f)
		{
			switch (i)
			{
			case 0:
				Front_Left = AreaStates[CoordinateToIndex(AreaCoordinate, 4)].Tiles[CoordinateToIndex(Temp, 18)].TileType;
				break;
			case 1:
				Front_Right =AreaStates[CoordinateToIndex(AreaCoordinate, 4)].Tiles[CoordinateToIndex(Temp, 18)].TileType;
				break;
			case 2:
				Forward_Left = AreaStates[CoordinateToIndex(AreaCoordinate, 4)].Tiles[CoordinateToIndex(Temp, 18)].TileType;
				break;
			case 3:
				Forward_Right = AreaStates[CoordinateToIndex(AreaCoordinate, 4)].Tiles[CoordinateToIndex(Temp, 18)].TileType;
				break;
			}
		}

	}
	
	
	return FTileDetail(Front_Left, Front_Right, Forward_Left, Forward_Right);
}

void AUCMapGenerator::SetCorridorAroundRooms()
{
	static constexpr ERoomDirection AllDirections[] =
	{
		ERoomDirection::Right,
		ERoomDirection::Left,
		ERoomDirection::Front,
		ERoomDirection::Forward
	};
	static constexpr ERoomDirection HorizontalDirections[] =
	{
		ERoomDirection::Right,
		ERoomDirection::Left
	};
	static constexpr ERoomDirection VerticalDirections[] =
	{
		ERoomDirection::Front,
		ERoomDirection::Forward
	};

	const auto GetDirectionOffset = [](ERoomDirection Direction)
	{
		// 방향을 타일 좌표 오프셋으로 변환합니다.
		switch (Direction)
		{
		case ERoomDirection::Right:
			return FVector2D(1.0f, 0.0f);
		case ERoomDirection::Left:
			return FVector2D(-1.0f, 0.0f);
		case ERoomDirection::Front:
			return FVector2D(0.0f, -1.0f);
		case ERoomDirection::Forward:
			return FVector2D(0.0f, 1.0f);
		default:
			return FVector2D::ZeroVector;
		}
	};

	// 각 방에 무작위로 선택한 서로 직교하는 두 방향의 복도를 생성합니다.
	for (FAreaState& AreaState : AreaStates)
	{
		for (FRoomState& RoomState : AreaState.Rooms)
		{
			RoomState.AroundFloorDirections.Reset();
			RoomState.AroundFloorCoordinates.Reset();

			// 첫 방향과 직교하는 두 번째 방향을 무작위로 선택합니다.
			const ERoomDirection FirstDirection = AllDirections[Seed.RandRange(0, UE_ARRAY_COUNT(AllDirections) - 1)];
			const FVector2D FirstOffset = GetDirectionOffset(FirstDirection);
			ERoomDirection SecondDirection;

			if (FirstOffset.X != 0.0f)
			{
				SecondDirection = VerticalDirections[Seed.RandRange(0, UE_ARRAY_COUNT(VerticalDirections) - 1)];
			}
			else
			{
				SecondDirection = HorizontalDirections[Seed.RandRange(0, UE_ARRAY_COUNT(HorizontalDirections) - 1)];
			}

			RoomState.AroundFloorDirections.Add(FirstDirection);
			RoomState.AroundFloorDirections.Add(SecondDirection);

			const auto AddCorridorTile = [this, &AreaState, &RoomState](const FVector2D& TileCoordinate)
			{
				const int32 TileIndex = CoordinateToIndex(TileCoordinate, 18);

				// 영역 밖이거나 방으로 사용 중인 타일은 복도 연결 후보에서 제외합니다.
				if (!AreaState.Tiles.IsValidIndex(TileIndex) || AreaState.Tiles[TileIndex].TileType == ETileType::Room)
				{
					return;
				}

				// 빈 타일은 복도로 변경하고 기존 복도와 Area 연결 타일은 그대로 재사용합니다.
				if (AreaState.Tiles[TileIndex].TileType == ETileType::Blank)
				{
					SetTileType(AreaState.AreaCoordinate, TileCoordinate, ETileType::Corridor);
				}

				RoomState.AroundFloorCoordinates.AddUnique(TileCoordinate);
			};

			// 방을 구성하는 모든 타일에서 선택된 두 방향의 외곽 복도를 생성합니다.
			for (const FRoomTileState& RoomTileState : RoomState.RoomTileState)
			{
				for (const ERoomDirection Direction : RoomState.AroundFloorDirections)
				{
					AddCorridorTile(RoomTileState.RoomTileLocation + GetDirectionOffset(Direction));
				}
			}

			FVector2D MinimumCoordinate = RoomState.RoomTileState[0].RoomTileLocation;
			FVector2D MaximumCoordinate = MinimumCoordinate;

			// 고정된 방 타일 인덱스 대신 실제 방의 좌표 범위를 계산합니다.
			for (const FRoomTileState& RoomTileState : RoomState.RoomTileState)
			{
				MinimumCoordinate.X = FMath::Min(MinimumCoordinate.X, RoomTileState.RoomTileLocation.X);
				MinimumCoordinate.Y = FMath::Min(MinimumCoordinate.Y, RoomTileState.RoomTileLocation.Y);
				MaximumCoordinate.X = FMath::Max(MaximumCoordinate.X, RoomTileState.RoomTileLocation.X);
				MaximumCoordinate.Y = FMath::Max(MaximumCoordinate.Y, RoomTileState.RoomTileLocation.Y);
			}

			FVector2D CornerCoordinate = FVector2D::ZeroVector;

			// 두 방향의 외곽 복도가 만나는 모서리 좌표를 계산합니다.
			for (const ERoomDirection Direction : RoomState.AroundFloorDirections)
			{
				const FVector2D DirectionOffset = GetDirectionOffset(Direction);

				if (DirectionOffset.X > 0.0f)
				{
					CornerCoordinate.X = MaximumCoordinate.X + 1.0f;
				}
				else if (DirectionOffset.X < 0.0f)
				{
					CornerCoordinate.X = MinimumCoordinate.X - 1.0f;
				}

				if (DirectionOffset.Y > 0.0f)
				{
					CornerCoordinate.Y = MaximumCoordinate.Y + 1.0f;
				}
				else if (DirectionOffset.Y < 0.0f)
				{
					CornerCoordinate.Y = MinimumCoordinate.Y - 1.0f;
				}
			}

			AddCorridorTile(CornerCoordinate);

			// 외곽 연결 후보가 없으면 방 타일을 경로 탐색의 폴백 목표로 사용합니다.
			if (RoomState.AroundFloorCoordinates.IsEmpty())
			{
				RoomState.AroundFloorCoordinates.Add(RoomState.RoomTileState[0].RoomTileLocation);
			}
		}
	}
}

void AUCMapGenerator::SetRoomTile()
{
	static const FVector2D RoomRootOffsets[] =
	{
		FVector2D(0.0f, 0.0f),
		FVector2D(1.0f, 0.0f),
		FVector2D(1.0f, 1.0f),
		FVector2D(0.0f, 1.0f)
	};
	static const FVector2D RoomTileOffsets[] =
	{
		FVector2D(0.0f, 0.0f),
		FVector2D(1.0f, 0.0f),
		FVector2D(2.0f, 0.0f),
		FVector2D(0.0f, 1.0f),
		FVector2D(0.0f, 2.0f),
		FVector2D(1.0f, 1.0f),
		FVector2D(2.0f, 1.0f),
		FVector2D(1.0f, 2.0f),
		FVector2D(2.0f, 2.0f)
	};
	constexpr int32 RoomGridSize = 4;
	constexpr int32 RoomSlotCount = RoomGridSize * RoomGridSize;

	// 각 Area의 4x4 방 슬롯 중 무작위 위치에 방을 배치합니다.
	for (FAreaState& AreaState : AreaStates)
	{
		TArray<bool> RoomSlots;
		RoomSlots.Init(false, RoomSlotCount);

		const int32 RoomSelectionCount = Seed.RandRange(5, 10);

		// 선택 횟수만큼 방 슬롯을 활성화하며 중복 선택은 하나의 방으로 처리합니다.
		for (int32 SelectionIndex = 0; SelectionIndex < RoomSelectionCount; ++SelectionIndex)
		{
			RoomSlots[Seed.RandRange(0, RoomSlotCount - 1)] = true;
		}

		TArray<int32> SelectedRoomIndices;

		// 활성화된 방 슬롯 인덱스를 수집합니다.
		for (int32 RoomIndex = 0; RoomIndex < RoomSlots.Num(); ++RoomIndex)
		{
			if (RoomSlots[RoomIndex])
			{
				SelectedRoomIndices.Add(RoomIndex);
			}
		}

		const int32 MagatamaRoomIndex = SelectedRoomIndices[Seed.RandRange(0, SelectedRoomIndices.Num() - 1)];

		// 선택된 각 슬롯에 3x3 크기의 방 타일을 생성합니다.
		for (const int32 RoomIndex : SelectedRoomIndices)
		{
			FRoomState& RoomState = AreaState.Rooms.Emplace_GetRef();
			RoomState.RoomAreaCoordinate = IndexToCoordinate(RoomIndex, RoomGridSize);
			RoomState.MagatamaRoom = MagatamaRoomIndex == RoomIndex;

			const FVector2D RoomSlotRoot(RoomIndex % RoomGridSize * RoomGridSize + 2, RoomIndex / RoomGridSize * RoomGridSize + 2);
			const FVector2D& RoomRootOffset = RoomRootOffsets[Seed.RandRange(0, UE_ARRAY_COUNT(RoomRootOffsets) - 1)];
			const FVector2D RoomTileRoot = RoomSlotRoot + RoomRootOffset;

			// 기존 RoomTileState 인덱스 순서를 유지하며 방 타일을 배치합니다.
			for (const FVector2D& RoomTileOffset : RoomTileOffsets)
			{
				FRoomTileState& RoomTileState = RoomState.RoomTileState.Emplace_GetRef();
				RoomTileState.RoomTileLocation = RoomTileRoot + RoomTileOffset;
				SetTileType(AreaState.AreaCoordinate, RoomTileState.RoomTileLocation, ETileType::Room);
			}
		}
	}
}

void AUCMapGenerator::CheckRoomConnection()
{
	// 각 방의 네 방향 중앙 타일을 기준으로 인접한 방을 확인합니다.
	for (FAreaState& AreaState : AreaStates)
	{
		for (FRoomState& RoomState : AreaState.Rooms)
		{
			RoomState.ConnectingRoomDirections.Reset();

			const FTileDetail FrontTileDetail = GetNextTileDetail(AreaState.AreaCoordinate, RoomState.RoomTileState[1].RoomTileLocation);
			const FTileDetail LeftTileDetail = GetNextTileDetail(AreaState.AreaCoordinate, RoomState.RoomTileState[3].RoomTileLocation);
			const FTileDetail RightTileDetail = GetNextTileDetail(AreaState.AreaCoordinate, RoomState.RoomTileState[5].RoomTileLocation);
			const FTileDetail ForwardTileDetail = GetNextTileDetail(AreaState.AreaCoordinate, RoomState.RoomTileState[7].RoomTileLocation);

			// 인접 타일이 방이면 해당 방향을 연결된 방향으로 기록합니다.
			if (FrontTileDetail.FrontTileType == ETileType::Room)
			{
				RoomState.ConnectingRoomDirections.Add(ERoomDirection::Front);
			}

			if (LeftTileDetail.LeftTileType == ETileType::Room)
			{
				RoomState.ConnectingRoomDirections.Add(ERoomDirection::Left);
			}

			if (RightTileDetail.RightTileType == ETileType::Room)
			{
				RoomState.ConnectingRoomDirections.Add(ERoomDirection::Right);
			}

			if (ForwardTileDetail.ForwardTileType == ETileType::Room)
			{
				RoomState.ConnectingRoomDirections.Add(ERoomDirection::Forward);
			}
		}
	}
}

TArray<FVector2D> AUCMapGenerator::GetRoomConnectionTargets(FAreaState AreaState)
{
	TArray<FVector2D> TargetTiles;
	TargetTiles.Init(FVector2D::ZeroVector, 16);
	
	// 각 방의 4x4 위치에 대응하는 연결 타일을 저장합니다.
	for (int32 i=0; i< AreaState.Rooms.Num(); ++i)
	{
		// 연결 후보가 없는 방은 제외하여 빈 배열의 Last 호출을 방지합니다.
		if (AreaState.Rooms[i].AroundFloorCoordinates.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("방 연결 목표가 없습니다. Area: %s, Room: %s"), *AreaState.AreaCoordinate.ToString(), *AreaState.Rooms[i].RoomAreaCoordinate.ToString());
			continue;
		}

		TargetTiles[CoordinateToIndex(AreaState.Rooms[i].RoomAreaCoordinate, 4)] = AreaState.Rooms[i].AroundFloorCoordinates.Last();
	}
	
	return TargetTiles;
}

FNextTileInfo AUCMapGenerator::SearchNextTilePath(FVector2D AreaCoordinate, FVector2D StartTileCoordinate,FVector2D TargetTileCoordinate)
{
	FVector2D CurrentCheckTile = StartTileCoordinate;
	FVector2D TargetTileTemp = TargetTileCoordinate;
	FVector2D AreaCoordinateTemp = AreaCoordinate;
	bool CheckedOtherDirection = false;
	
	FTileDetail TileDetail = GetNextTileDetail(AreaCoordinateTemp, CurrentCheckTile);
	
	if (FMath::Sign((TargetTileTemp - CurrentCheckTile).X) == 1.0)
	{
		if (TileDetail.RightTileType == ETileType::Blank)
		{
			FCoordinateThrough CoordinateThrough = SetTileType(AreaCoordinateTemp, CurrentCheckTile + FVector2D(1.0f, 0.0f), ETileType::Corridor);
			return FNextTileInfo(false, CoordinateThrough.TileCoordinateThrough);;
		}
	}
	else if (FMath::Sign((TargetTileTemp - CurrentCheckTile).X) == -1.0)
	{
		if (TileDetail.LeftTileType == ETileType::Blank)
		{
			FCoordinateThrough CoordinateThrough = SetTileType(AreaCoordinateTemp, CurrentCheckTile + FVector2D(-1.0f, 0.0f), ETileType::Corridor);
			return FNextTileInfo(false, CoordinateThrough.TileCoordinateThrough);;
		}
	}
	else if (FMath::Sign((TargetTileTemp - CurrentCheckTile).Y) == 1.0)
	{
		if (TileDetail.ForwardTileType == ETileType::Blank)
		{
			FCoordinateThrough CoordinateThrough = SetTileType(AreaCoordinateTemp, CurrentCheckTile + FVector2D(0.0f, 1.0f), ETileType::Corridor);
			return FNextTileInfo(false, CoordinateThrough.TileCoordinateThrough);;
		}
	}
	else if (FMath::Sign((TargetTileTemp - CurrentCheckTile).Y) == -1.0)
	{
		if (TileDetail.FrontTileType == ETileType::Blank)
		{
			FCoordinateThrough CoordinateThrough = SetTileType(AreaCoordinateTemp, CurrentCheckTile + FVector2D(0.0f, -1.0f), ETileType::Corridor);
			return FNextTileInfo(false, CoordinateThrough.TileCoordinateThrough);;
		}
	}
	
	const FVector2D DistanceToTarget = TargetTileTemp - CurrentCheckTile;

	// 목표 타일과 인접하거나 같은 위치에 도달하면 경로 검색을 종료합니다.
	if (FMath::Abs(DistanceToTarget.X) <= 1.0f && FMath::Abs(DistanceToTarget.Y) <= 1.0f)
	{
		return FNextTileInfo(true, FVector2D(0.0f, 0.0f));
	}
	else if ((TargetTileTemp - CurrentCheckTile).X == 0.0f)
	{
		if (TileDetail.RightTileType == ETileType::Blank)
		{
			if ((CurrentCheckTile + FVector2D(1.0f, 0.0f)).X != 19.0)
			{
				 FCoordinateThrough CoordinateThrough = SetTileType(AreaCoordinateTemp, CurrentCheckTile + FVector2D(1.0f, 0.0f), ETileType::Corridor);
				return FNextTileInfo(false, CoordinateThrough.TileCoordinateThrough);
			}
		}
		
		if (TileDetail.LeftTileType == ETileType::Blank)
		{
			if ((CurrentCheckTile + FVector2D(-1.0f, 0.0f)).X != 0.0)
			{
				FCoordinateThrough CoordinateThrough = SetTileType(AreaCoordinateTemp, CurrentCheckTile + FVector2D(-1.0f, 0.0f), ETileType::Corridor);
				return FNextTileInfo(false, CoordinateThrough.TileCoordinateThrough);
			}
			else
			{
				return FNextTileInfo(true, FVector2D(0.0f, 0.0f));			
			}
		}
		
		if (CheckedOtherDirection)
		{
			return FNextTileInfo(true, FVector2D(0.0f, 0.0f));
		}
		else
		{
			CheckedOtherDirection = true;
			if (TileDetail.ForwardTileType == ETileType::Blank)
			{
				if ((CurrentCheckTile + FVector2D(0.0f, 1.0f)).Y != 19.0)
				{
					FCoordinateThrough CoordinateThrough = SetTileType(AreaCoordinateTemp, CurrentCheckTile + FVector2D(0.0f, 1.0f), ETileType::Corridor);
					return FNextTileInfo(false, CoordinateThrough.TileCoordinateThrough);
				}
			}
		
			if (TileDetail.FrontTileType == ETileType::Blank)
			{
				if ((CurrentCheckTile + FVector2D(0.0f, -1.0f)).Y != 0.0)
				{
					FCoordinateThrough CoordinateThrough = SetTileType(AreaCoordinateTemp, CurrentCheckTile + FVector2D(0.0f, -1.0f), ETileType::Corridor);
					return FNextTileInfo(false, CoordinateThrough.TileCoordinateThrough);
				}
			}
		}
	}
	else
	{
		if (TileDetail.ForwardTileType == ETileType::Blank)
		{
			if ((CurrentCheckTile + FVector2D(0.0f, 1.0f)).Y != 19.0)
			{
				FCoordinateThrough CoordinateThrough = SetTileType(AreaCoordinateTemp, CurrentCheckTile + FVector2D(0.0f, 1.0f), ETileType::Corridor);
				return FNextTileInfo(false, CoordinateThrough.TileCoordinateThrough);
			}
		}
		
		if (TileDetail.FrontTileType == ETileType::Blank)
		{
			if ((CurrentCheckTile + FVector2D(0.0f, -1.0f)).Y != 0.0)
			{
				FCoordinateThrough CoordinateThrough = SetTileType(AreaCoordinateTemp, CurrentCheckTile + FVector2D(0.0f, -1.0f), ETileType::Corridor);
				return FNextTileInfo(false, CoordinateThrough.TileCoordinateThrough);
			}
			else
			{
				return FNextTileInfo(true, FVector2D(0.0f, 0.0f));
			}
		}
	}
	
	// 진행 가능한 타일이 없다면 현재 경로 검색을 종료합니다.
	return FNextTileInfo(true, FVector2D::ZeroVector);
}

void AUCMapGenerator::FixBroadCorridor()
{
	// 주변 여덟 칸이 모두 복도인 타일을 찾아 넓은 복도의 중앙을 제거합니다.
	for (FAreaState& AreaState : AreaStates)
	{
		TArray<int32> FixTileIndices;

		for (int32 TileIndex = 0; TileIndex < AreaState.Tiles.Num(); ++TileIndex)
		{
			if (AreaState.Tiles[TileIndex].TileType != ETileType::Corridor)
			{
				continue;
			}

			const FVector2D& TileCoordinate = AreaState.Tiles[TileIndex].TileCoordinate;
			const FTileDetail TileDetail = GetNextTileDetail(AreaState.AreaCoordinate, TileCoordinate);
			if (TileDetail.RightTileType == ETileType::Corridor && TileDetail.LeftTileType == ETileType::Corridor && TileDetail.ForwardTileType == ETileType::Corridor && TileDetail.FrontTileType == ETileType::Corridor)
			{
				const FTileDetail DiagonalTileDetail = GetDiagonalTileDetail(AreaState.AreaCoordinate, TileCoordinate);
				if (DiagonalTileDetail.RightTileType == ETileType::Corridor && DiagonalTileDetail.LeftTileType == ETileType::Corridor && DiagonalTileDetail.ForwardTileType == ETileType::Corridor && DiagonalTileDetail.FrontTileType == ETileType::Corridor)
				{
					FixTileIndices.Add(TileIndex);
				}
			}
		}

		// 검색 중 타일 상태가 바뀌지 않도록 수집이 끝난 뒤 중앙 타일을 제거합니다.
		for (const int32 TileIndex : FixTileIndices)
		{
			SetTileType(AreaState.AreaCoordinate, AreaState.Tiles[TileIndex].TileCoordinate, ETileType::Blank);
		}
	}
}
