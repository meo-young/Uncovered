#include "UCGridMapLayout.h"

namespace
{
	const FIntPoint Steps[] = {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)};

	// 배열에 접근하기 전에 두 좌표 축을 각각 검사합니다.
	bool Inside(FIntPoint P, FIntPoint Size) { return P.X >= 0 && P.Y >= 0 && P.X < Size.X && P.Y < Size.Y; }
	// 2차원 타일을 행 우선 인덱스로 변환합니다.
	int32 Index(FIntPoint P, FIntPoint Size) { return P.X + P.Y * Size.X; }
	// 행 우선 인덱스를 타일 좌표로 변환합니다.
	FIntPoint Point(int32 I, FIntPoint Size) { return FIntPoint(I % Size.X, I / Size.X); }
	// 상하좌우 이동 거리를 계산합니다.
	int32 Distance(FIntPoint A, FIntPoint B) { return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y); }

	uint64 BoundaryKey(FIntPoint A, FIntPoint B, FIntPoint Size)
	{
		// 양방향 타일 경계를 하나의 키로 정규화합니다.
		const uint32 IA = Index(A, Size);
		const uint32 IB = Index(B, Size);
		return (static_cast<uint64>(FMath::Min(IA, IB)) << 32) | FMath::Max(IA, IB);
	}

	TArray<FIntPoint> PerimeterTiles(const FUCGridRoom& Room)
	{
		TArray<FIntPoint> Tiles;
		Tiles.Reserve(2 * Room.Size.X + 2 * Room.Size.Y + 4);
		// 모서리를 포함한 직사각형 외곽을 중복 없이 순환 순서로 구성합니다.
		for (int32 X = -1; X <= Room.Size.X; ++X) { Tiles.Add(Room.Origin + FIntPoint(X, -1)); }
		for (int32 Y = 0; Y <= Room.Size.Y; ++Y) { Tiles.Add(Room.Origin + FIntPoint(Room.Size.X, Y)); }
		for (int32 X = Room.Size.X - 1; X >= -1; --X) { Tiles.Add(Room.Origin + FIntPoint(X, Room.Size.Y)); }
		for (int32 Y = Room.Size.Y - 1; Y >= 0; --Y) { Tiles.Add(Room.Origin + FIntPoint(-1, Y)); }
		return Tiles;
	}

	bool Fail(FString& Error, const FString& Message)
	{
		// 호출부가 재현할 수 있도록 실패 이유를 전달합니다.
		Error = Message;
		return false;
	}

	template <typename T> void Shuffle(TArray<T>& Values, FRandomStream& Random)
	{
		// 전역 난수 대신 지정한 시드로 재현 가능한 순서를 구성합니다.
		for (int32 I = Values.Num() - 1; I > 0; --I)
		{
			Values.Swap(I, Random.RandRange(0, I));
		}
	}

	bool ValidDoor(const FUCMapDoor& Door, FIntPoint Size, const TArray<FIntPoint>& Blocked)
	{
		// 출입구는 이동 가능한 경계 타일에서 바깥쪽 한 칸으로 연결합니다.
		return Inside(Door.Tile, Size) && Distance(Door.Outward, FIntPoint::ZeroValue) == 1 && !Inside(Door.Tile + Door.Outward, Size) && !Blocked.Contains(Door.Tile);
	}

	bool Separated(const FUCGridRoom& A, const FUCGridRoom& B, int32 Gap)
	{
		// 두 사각형 사이에 지정한 폭의 빈 타일 띠를 확보합니다.
		return A.Origin.X + A.Size.X + Gap <= B.Origin.X || B.Origin.X + B.Size.X + Gap <= A.Origin.X || A.Origin.Y + A.Size.Y + Gap <= B.Origin.Y || B.Origin.Y + B.Size.Y + Gap <= A.Origin.Y;
	}

	int32 RequiredDegree(const FUCGridMapSettings& S, const FUCGridRoom& Room)
	{
		// 제작된 방은 실제 메시의 열린 출입구를 모두 연결합니다.
		if (Room.DefinitionIndex != INDEX_NONE) { return Room.Doors.Num(); }
		return S.MinConnections;
	}

	bool PlaceRooms(const FUCGridMapSettings& S, int32 Count, FRandomStream& Random, FUCGridMapLayout& Map)
	{
		TArray<FUCGridRoom> Pending;
		// 필수 사전 제작 방을 전체 방 개수에 포함합니다.
		for (int32 Definition = 0; Definition < S.DesignedRooms.Num(); ++Definition)
		{
			const FUCDesignedRoomEntry& Entry = S.DesignedRooms[Definition];
			const AUCDesignedRoom* Template = Entry.RoomClass.GetDefaultObject();
			for (int32 I = 0; I < Entry.Count; ++I)
			{
				FUCGridRoom& Room = Pending.AddDefaulted_GetRef();
				Room.DefinitionIndex = Definition;
				Room.Size = Template->Size;
				Room.Doors = Template->Doors;
				Room.BlockedTiles = Template->BlockedTiles;
			}
		}

		// 랜덤 방은 네 변의 중앙을 출입구 후보로 설정합니다.
		while (Pending.Num() < Count)
		{
			FUCGridRoom& Room = Pending.AddDefaulted_GetRef();
			Room.Size = FIntPoint(Random.RandRange(S.MinRoomSize.X, S.MaxRoomSize.X), Random.RandRange(S.MinRoomSize.Y, S.MaxRoomSize.Y));
			Room.Doors = {{FIntPoint(0, Room.Size.Y / 2), FIntPoint(-1, 0)}, {FIntPoint(Room.Size.X - 1, Room.Size.Y / 2), FIntPoint(1, 0)}, {FIntPoint(Room.Size.X / 2, 0), FIntPoint(0, -1)}, {FIntPoint(Room.Size.X / 2, Room.Size.Y - 1), FIntPoint(0, 1)}};
		}
		Shuffle(Pending, Random);
		Pending.StableSort([](const FUCGridRoom& A, const FUCGridRoom& B) { return A.Size.X * A.Size.Y > B.Size.X * B.Size.Y; });

		// 큰 방부터 배치하며 한 방의 배치 시도 횟수를 제한합니다.
		for (FUCGridRoom& Room : Pending)
		{
			bool bPlaced = false;
			for (int32 Attempt = 0; Attempt < S.PlacementAttemptsPerRoom; ++Attempt)
			{
				Room.Origin = FIntPoint(Random.RandRange(0, S.MapSize.X - Room.Size.X), Random.RandRange(0, S.MapSize.Y - Room.Size.Y));
				bool bFits = true;
				for (const FUCGridRoom& Other : Map.Rooms)
				{
					if (!Separated(Room, Other, S.MinRoomGap)) { bFits = false; break; }
				}
				// 사전 제작 출입구는 맵 외곽으로 닫히지 않도록 배치합니다.
				if (Room.DefinitionIndex != INDEX_NONE)
				{
					for (const FUCMapDoor& Door : Room.Doors)
					{
						if (!Inside(Room.Origin + Door.Tile + Door.Outward, S.MapSize)) { bFits = false; }
					}
				}
				if (bFits) { bPlaced = true; break; }
			}
			if (!bPlaced) { return false; }
			const int32 RoomIndex = Map.Rooms.Add(Room);
			for (int32 Y = 0; Y < Room.Size.Y; ++Y)
			{
				for (int32 X = 0; X < Room.Size.X; ++X) { Map.Cells[Index(Room.Origin + FIntPoint(X, Y), Map.Size)] = RoomIndex; }
			}
		}
		return true;
	}

	bool FindCorridor(const FUCGridMapSettings& S, const FUCGridMapLayout& Map, const FUCGridConnection& Edge, int32& Budget, TArray<FIntPoint>& Path)
	{
		const FUCGridRoom& A = Map.Rooms[Edge.RoomA];
		const FUCGridRoom& B = Map.Rooms[Edge.RoomB];
		const FIntPoint DoorA = A.Origin + A.Doors[Edge.DoorA].Tile;
		const FIntPoint DoorB = B.Origin + B.Doors[Edge.DoorB].Tile;
		const FIntPoint Start = DoorA + A.Doors[Edge.DoorA].Outward;
		const FIntPoint End = DoorB + B.Doors[Edge.DoorB].Outward;
		const auto CanUse = [&](FIntPoint P)
		{
			// 기존 복도에 나란히 붙는 경우도 합류로 간주하여 제외합니다.
			if (!Inside(P, Map.Size) || Map.Cells[Index(P, Map.Size)] != INDEX_NONE) { return false; }
			for (FIntPoint Step : Steps)
			{
				const FIntPoint N = P + Step;
				if (!Inside(N, Map.Size)) { continue; }
				const int32 Cell = Map.Cells[Index(N, Map.Size)];
				if (Cell == INDEX_NONE) { continue; }
				if (P == Start && N == DoorA) { continue; }
				if (P == End && N == DoorB) { continue; }
				return false;
			}
			return true;
		};

		// 최단 경로를 너비 우선 탐색하고 성공한 경로만 나중에 반영합니다.
		if (!CanUse(Start) || !CanUse(End) || Distance(Start, End) + 1 > S.MaxCorridorLength) { return false; }
		TArray<int32> Parents;
		TArray<int32> Depth;
		TArray<int32> Queue;
		Parents.Init(INDEX_NONE, Map.Cells.Num());
		Depth.Init(0, Map.Cells.Num());
		const int32 StartIndex = Index(Start, Map.Size);
		const int32 EndIndex = Index(End, Map.Size);
		Parents[StartIndex] = StartIndex;
		Depth[StartIndex] = 1;
		Queue.Add(StartIndex);
		for (int32 Head = 0; Head < Queue.Num() && Budget > 0; ++Head)
		{
			--Budget;
			const int32 Current = Queue[Head];
			if (Current == EndIndex)
			{
				if (Depth[Current] < S.MinCorridorLength) { return false; }
				int32 Cursor = Current;
				while (Cursor != StartIndex) { Path.Add(Point(Cursor, Map.Size)); Cursor = Parents[Cursor]; }
				Path.Add(Start);
				for (int32 I = 0; I < Path.Num() / 2; ++I) { Path.Swap(I, Path.Num() - 1 - I); }
				return true;
			}
			if (Depth[Current] >= S.MaxCorridorLength) { continue; }
			for (FIntPoint Step : Steps)
			{
				const FIntPoint Next = Point(Current, Map.Size) + Step;
				if (!CanUse(Next)) { continue; }
				const int32 NextIndex = Index(Next, Map.Size);
				if (Parents[NextIndex] != INDEX_NONE) { continue; }
				Parents[NextIndex] = Current;
				Depth[NextIndex] = Depth[Current] + 1;
				Queue.Add(NextIndex);
			}
		}
		return false;
	}

	bool ConnectRooms(const FUCGridMapSettings& S, FRandomStream& Random, FUCGridMapLayout& Map, int32& Budget)
	{
		const int32 Count = Map.Rooms.Num();
		TArray<int32> Degrees;
		TArray<int32> Groups;
		TArray<FIntPoint> Pairs;
		Degrees.Init(0, Count);
		// 처음에는 각 방을 독립된 그룹으로 구성합니다.
		for (int32 A = 0; A < Count; ++A)
		{
			Groups.Add(A);
			for (int32 B = A + 1; B < Count; ++B) { Pairs.Add(FIntPoint(A, B)); }
		}
		Shuffle(Pairs, Random);
		Pairs.StableSort([&](FIntPoint A, FIntPoint B) { return Distance(Map.Rooms[A.X].Origin, Map.Rooms[A.Y].Origin) < Distance(Map.Rooms[B.X].Origin, Map.Rooms[B.Y].Origin); });
		const auto TryConnect = [&](int32 A, int32 B)
		{
			// 방별 연결 상한과 중복 연결을 먼저 검사합니다.
			if (Budget <= 0 || Degrees[A] >= FMath::Min(S.MaxConnections, Map.Rooms[A].Doors.Num()) || Degrees[B] >= FMath::Min(S.MaxConnections, Map.Rooms[B].Doors.Num())) { return false; }
			TSet<int32> UsedA;
			TSet<int32> UsedB;
			for (const FUCGridConnection& Existing : Map.Connections)
			{
				if ((Existing.RoomA == A && Existing.RoomB == B) || (Existing.RoomA == B && Existing.RoomB == A)) { return false; }
				if (Existing.RoomA == A) { UsedA.Add(Existing.DoorA); }
				if (Existing.RoomB == A) { UsedA.Add(Existing.DoorB); }
				if (Existing.RoomA == B) { UsedB.Add(Existing.DoorA); }
				if (Existing.RoomB == B) { UsedB.Add(Existing.DoorB); }
			}
			TArray<FIntPoint> DoorPairs;
			for (int32 DA = 0; DA < Map.Rooms[A].Doors.Num(); ++DA)
			{
				for (int32 DB = 0; DB < Map.Rooms[B].Doors.Num(); ++DB)
				{
					if (!UsedA.Contains(DA) && !UsedB.Contains(DB)) { DoorPairs.Add(FIntPoint(DA, DB)); }
				}
			}
			Shuffle(DoorPairs, Random);
			DoorPairs.StableSort([&](FIntPoint X, FIntPoint Y)
			{
				return Distance(Map.Rooms[A].Origin + Map.Rooms[A].Doors[X.X].Tile, Map.Rooms[B].Origin + Map.Rooms[B].Doors[X.Y].Tile) < Distance(Map.Rooms[A].Origin + Map.Rooms[A].Doors[Y.X].Tile, Map.Rooms[B].Origin + Map.Rooms[B].Doors[Y.Y].Tile);
			});
			// 출입구 조합을 순서대로 탐색하여 충돌 없는 복도를 확정합니다.
			for (FIntPoint Pair : DoorPairs)
			{
				FUCGridConnection Edge;
				Edge.RoomA = A; Edge.RoomB = B; Edge.DoorA = Pair.X; Edge.DoorB = Pair.Y;
				if (!FindCorridor(S, Map, Edge, Budget, Edge.Tiles)) { continue; }
				const int32 CorridorId = -Map.Connections.Num() - 2;
				for (FIntPoint P : Edge.Tiles) { Map.Cells[Index(P, Map.Size)] = CorridorId; }
				Map.Connections.Add(MoveTemp(Edge));
				++Degrees[A]; ++Degrees[B];
				const int32 OldGroup = Groups[B];
				const int32 NewGroup = Groups[A];
				for (int32& Group : Groups) { if (Group == OldGroup) { Group = NewGroup; } }
				return true;
			}
			return false;
		};

		// 실제 복도가 성공한 경우에만 그룹을 병합하여 연결 트리를 구성합니다.
		for (FIntPoint Pair : Pairs)
		{
			if (Groups[Pair.X] != Groups[Pair.Y]) { TryConnect(Pair.X, Pair.Y); }
		}
		for (int32 Group : Groups) { if (Group != Groups[0]) { return false; } }

		// 최소 연결 수와 사전 제작 방의 필수 출입구를 우선 충족합니다.
		for (FIntPoint Pair : Pairs)
		{
			if (Degrees[Pair.X] < RequiredDegree(S, Map.Rooms[Pair.X]) || Degrees[Pair.Y] < RequiredDegree(S, Map.Rooms[Pair.Y])) { TryConnect(Pair.X, Pair.Y); }
		}
		for (int32 I = 0; I < Count; ++I) { if (Degrees[I] < RequiredDegree(S, Map.Rooms[I])) { return false; } }

		// 상한을 유지하면서 추가 연결 확률에 따라 순환 경로를 구성합니다.
		Shuffle(Pairs, Random);
		for (FIntPoint Pair : Pairs) { if (Random.FRand() < S.ExtraConnectionProbability) { TryConnect(Pair.X, Pair.Y); } }
		return true;
	}

	void ExpandCorridors(const FUCGridMapSettings& S, FRandomStream& Random, FUCGridMapLayout& Map)
	{
		TArray<int32> Order;
		for (int32 I = 0; I < Map.Connections.Num(); ++I) { Order.Add(I); }
		// 먼저 처리한 연결만 공간을 차지하는 편향을 줄이도록 순서를 섞습니다.
		Shuffle(Order, Random);
		for (int32 EdgeIndex : Order)
		{
			FUCGridConnection& Edge = Map.Connections[EdgeIndex];
			const int32 CorridorId = -EdgeIndex - 2;
			const auto Grow = [&](FIntPoint P)
			{
				// 다른 복도와 합류하거나 관계없는 방에 닿는 확장을 제외합니다.
				if (!Inside(P, Map.Size)) { return false; }
				const int32 CellIndex = Index(P, Map.Size);
				if (Map.Cells[CellIndex] == CorridorId) { return true; }
				if (Map.Cells[CellIndex] != INDEX_NONE) { return false; }
				bool bTouchesPath = false;
				for (FIntPoint Step : Steps)
				{
					const FIntPoint N = P + Step;
					if (!Inside(N, Map.Size)) { continue; }
					const int32 Cell = Map.Cells[Index(N, Map.Size)];
					if (Cell == CorridorId) { bTouchesPath = true; continue; }
					if (Cell == INDEX_NONE || Cell == Edge.RoomA || Cell == Edge.RoomB) { continue; }
					return false;
				}
				if (!bTouchesPath) { return false; }
				Map.Cells[CellIndex] = CorridorId;
				Edge.ExtraTiles.Add(P);
				return true;
			};

			// 구간마다 길이, 폭, 확장 방향을 다시 선택하여 불규칙한 바닥을 구성합니다.
			int32 Remaining = 0;
			int32 LeftWidth = 0;
			int32 RightWidth = 0;
			for (int32 T = 0; T < Edge.Tiles.Num(); ++T)
			{
				if (Remaining == 0)
				{
					Remaining = Random.RandRange(S.MinWideSectionLength, S.MaxWideSectionLength);
					LeftWidth = 0; RightWidth = 0;
					if (S.MaxCorridorWidth > 1 && Random.FRand() < S.CorridorWideningProbability)
					{
						const int32 ExtraWidth = Random.RandRange(1, S.MaxCorridorWidth - 1);
						LeftWidth = Random.RandRange(0, ExtraWidth);
						RightWidth = ExtraWidth - LeftWidth;
					}
				}
				--Remaining;
				FIntPoint Tangent = Map.Rooms[Edge.RoomA].Doors[Edge.DoorA].Outward;
				if (T + 1 < Edge.Tiles.Num()) { Tangent = Edge.Tiles[T + 1] - Edge.Tiles[T]; }
				else if (T > 0) { Tangent = Edge.Tiles[T] - Edge.Tiles[T - 1]; }
				const FIntPoint Normal(-Tangent.Y, Tangent.X);
				for (int32 Width = 1; Width <= LeftWidth; ++Width) { if (!Grow(Edge.Tiles[T] + Normal * Width)) { break; } }
				for (int32 Width = 1; Width <= RightWidth; ++Width) { if (!Grow(Edge.Tiles[T] - Normal * Width)) { break; } }
			}

			// 출입구의 일부에서만 방 옆으로 길을 연장하며 한 바퀴 생성을 강제하지 않습니다.
			for (FIntPoint Endpoint : {FIntPoint(Edge.RoomA, Edge.DoorA), FIntPoint(Edge.RoomB, Edge.DoorB)})
			{
				if (S.MaxRoomSideLength == 0 || Random.FRand() >= S.RoomSidePassageProbability) { continue; }
				const FUCGridRoom& Room = Map.Rooms[Endpoint.X];
				const FUCMapDoor& Door = Room.Doors[Endpoint.Y];
				const TArray<FIntPoint> Boundary = PerimeterTiles(Room);
				const int32 Start = Boundary.Find(Room.Origin + Door.Tile + Door.Outward);
				const int32 Length = Random.RandRange(1, FMath::Min(S.MaxRoomSideLength, Boundary.Num() - 2));
				int32 Direction = 1;
				if (Random.RandRange(0, 1) == 0) { Direction = -1; }
				for (int32 Step = 1; Step <= Length; ++Step)
				{
					const int32 BoundaryIndex = (Start + Direction * Step + Boundary.Num()) % Boundary.Num();
					if (!Grow(Boundary[BoundaryIndex])) { break; }
				}
			}
		}
	}

	TArray<int32> Flood(const FUCGridMapLayout& Map, FIntPoint Start)
	{
		TArray<int32> Distances;
		TArray<int32> Queue;
		Distances.Init(INDEX_NONE, Map.Cells.Num());
		TSet<int32> Blocked;
		// 소품으로 막힌 타일은 실제 이동 가능 영역에서 제외합니다.
		for (const FUCGridRoom& Room : Map.Rooms)
		{
			for (FIntPoint P : Room.BlockedTiles) { Blocked.Add(Index(Room.Origin + P, Map.Size)); }
		}
		TSet<uint64> Doors;
		// 넓어진 통로가 방 벽에 붙어도 선언한 출입구 경계만 통과하도록 구성합니다.
		for (const FUCGridConnection& Edge : Map.Connections)
		{
			const FUCGridRoom& A = Map.Rooms[Edge.RoomA];
			const FUCGridRoom& B = Map.Rooms[Edge.RoomB];
			Doors.Add(BoundaryKey(A.Origin + A.Doors[Edge.DoorA].Tile, Edge.Tiles[0], Map.Size));
			Doors.Add(BoundaryKey(B.Origin + B.Doors[Edge.DoorB].Tile, Edge.Tiles.Last(), Map.Size));
		}
		const int32 First = Index(Start, Map.Size);
		if (Map.Cells[First] == INDEX_NONE || Blocked.Contains(First)) { return Distances; }
		Distances[First] = 0;
		Queue.Add(First);
		// 방 벽을 관통하지 않는 네 방향 이동으로 실제 최단 거리를 계산합니다.
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 Current = Queue[Head];
			for (FIntPoint Step : Steps)
			{
				const FIntPoint P = Point(Current, Map.Size) + Step;
				if (!Inside(P, Map.Size)) { continue; }
				const int32 Next = Index(P, Map.Size);
				if (Map.Cells[Next] == INDEX_NONE || Blocked.Contains(Next) || Distances[Next] != INDEX_NONE) { continue; }
				if (Map.Cells[Current] != Map.Cells[Next] && (Map.Cells[Current] >= 0 || Map.Cells[Next] >= 0) && !Doors.Contains(BoundaryKey(Point(Current, Map.Size), P, Map.Size))) { continue; }
				Distances[Next] = Distances[Current] + 1;
				Queue.Add(Next);
			}
		}
		return Distances;
	}
}

bool UCGridMap::ValidateSettings(const FUCGridMapSettings& S, FString& Error)
{
	// 메모리와 계산량 상한을 포함하여 스크립트 입력도 직접 검사합니다.
	Error.Reset();
	if (S.MapSize.X < 1 || S.MapSize.Y < 1 || S.MapSize.X > 512 || S.MapSize.Y > 512 || !FMath::IsFinite(S.TileSize) || S.TileSize < 1.0f) { return Fail(Error, TEXT("맵 크기는 축별 1~512타일, 타일 크기는 1cm 이상이어야 합니다.")); }
	if (S.MinRooms < 2 || S.MaxRooms < S.MinRooms || S.MaxRooms > 128) { return Fail(Error, TEXT("최소 연결 수 1을 위해 방 개수는 2~128 범위여야 합니다.")); }
	if (S.MinRoomSize.X < 2 || S.MinRoomSize.Y < 2 || S.MaxRoomSize.X < S.MinRoomSize.X || S.MaxRoomSize.Y < S.MinRoomSize.Y || S.MaxRoomSize.X > S.MapSize.X || S.MaxRoomSize.Y > S.MapSize.Y) { return Fail(Error, TEXT("랜덤 방 크기 범위가 잘못되었거나 맵 크기를 초과합니다.")); }
	if (S.MaxCorridorWidth < 1 || S.MaxCorridorWidth > 8 || !FMath::IsFinite(S.CorridorWideningProbability) || S.CorridorWideningProbability < 0.0f || S.CorridorWideningProbability > 1.0f || !FMath::IsFinite(S.RoomSidePassageProbability) || S.RoomSidePassageProbability < 0.0f || S.RoomSidePassageProbability > 1.0f || S.MinWideSectionLength < 1 || S.MaxWideSectionLength < S.MinWideSectionLength || S.MaxWideSectionLength > 512 || S.MaxRoomSideLength < 0 || S.MaxRoomSideLength > 512) { return Fail(Error, TEXT("통로 확장 폭, 구간 길이, 측면 길이 또는 확률이 유효하지 않습니다.")); }
	if (S.MinConnections < 1 || S.MaxConnections < S.MinConnections || S.MaxConnections > 4 || S.MinConnections >= S.MinRooms || (S.MaxConnections == 1 && S.MaxRooms > 2)) { return Fail(Error, TEXT("연결 수는 1~4 범위여야 하며 전체 방을 연결할 수 있어야 합니다.")); }
	if (S.MinConnections == S.MaxConnections && S.MinConnections % 2 == 1 && (S.MinRooms != S.MaxRooms || S.MinRooms % 2 == 1)) { return Fail(Error, TEXT("모든 방의 연결 수가 같은 홀수이면 전체 방 개수도 짝수로 고정해야 합니다.")); }
	if (S.MinRoomGap < 1 || S.MinRoomGap > 512 || S.MinCorridorLength < 1 || S.MaxCorridorLength < S.MinCorridorLength || S.MaxCorridorLength > S.MapSize.X * S.MapSize.Y || !FMath::IsFinite(S.ExtraConnectionProbability) || S.ExtraConnectionProbability < 0.0f || S.ExtraConnectionProbability > 1.0f) { return Fail(Error, TEXT("방 간격, 복도 길이 또는 추가 연결 확률이 유효하지 않습니다.")); }
	if (S.MaxLayoutAttempts < 1 || S.MaxLayoutAttempts > 10000 || S.PlacementAttemptsPerRoom < 1 || S.PlacementAttemptsPerRoom > 10000 || S.MaxSearchNodes < 1 || S.MaxSearchNodes > 100000000) { return Fail(Error, TEXT("배치 시도는 1~10000회, 탐색 노드 한도는 1~100000000 범위여야 합니다.")); }
	int64 FixedCount = 0;
	int64 FixedArea = 0;
	bool bCanStart = false;
	// 사전 제작 방의 출입구와 내부 이동 영역을 생성 전에 검사합니다.
	for (const FUCDesignedRoomEntry& Entry : S.DesignedRooms)
	{
		if (!Entry.RoomClass || Entry.RoomClass->HasAnyClassFlags(CLASS_Abstract) || Entry.Count < 1 || Entry.Count > S.MinRooms || Entry.MinStartDistance < 0 || Entry.MaxStartDistance < 0 || (Entry.MaxStartDistance > 0 && Entry.MaxStartDistance < Entry.MinStartDistance)) { return Fail(Error, TEXT("사전 제작 방 클래스, 개수 또는 시작점 거리 범위가 유효하지 않습니다.")); }
		const AUCDesignedRoom* Room = Entry.RoomClass.GetDefaultObject();
		if (Room->Size.X < 1 || Room->Size.Y < 1 || Room->Size.X > S.MapSize.X || Room->Size.Y > S.MapSize.Y || !FMath::IsNearlyEqual(Room->AuthoredTileSize, S.TileSize)) { return Fail(Error, TEXT("사전 제작 방의 크기 또는 제작 타일 규격이 맵과 맞지 않습니다.")); }
		if (Room->Doors.Num() < S.MinConnections || Room->Doors.Num() > S.MaxConnections || Room->Doors.Num() >= S.MinRooms) { return Fail(Error, TEXT("사전 제작 방의 모든 출입구를 연결하려면 출입구 수가 연결 제한과 방 개수에 맞아야 합니다.")); }
		TSet<FIntPoint> Blocked;
		for (FIntPoint P : Room->BlockedTiles)
		{
			if (!Inside(P, Room->Size) || Blocked.Contains(P)) { return Fail(Error, TEXT("사전 제작 방의 막힌 타일이 중복되거나 영역을 벗어납니다.")); }
			Blocked.Add(P);
		}
		TSet<FIntPoint> DoorTiles;
		for (const FUCMapDoor& Door : Room->Doors)
		{
			if (!ValidDoor(Door, Room->Size, Room->BlockedTiles) || DoorTiles.Contains(Door.Tile)) { return Fail(Error, TEXT("출입구는 서로 다른 이동 가능 경계 타일에서 바깥 방향을 향해야 합니다.")); }
			DoorTiles.Add(Door.Tile);
		}
		FUCGridMapLayout Interior;
		Interior.Size = Room->Size;
		Interior.Cells.Init(0, Room->Size.X * Room->Size.Y);
		FUCGridRoom& InteriorRoom = Interior.Rooms.AddDefaulted_GetRef();
		InteriorRoom.Size = Room->Size;
		InteriorRoom.BlockedTiles = Room->BlockedTiles;
		const TArray<int32> Distances = Flood(Interior, Room->Doors[0].Tile);
		for (int32 I = 0; I < Distances.Num(); ++I)
		{
			if (!Blocked.Contains(Point(I, Room->Size)) && Distances[I] == INDEX_NONE) { return Fail(Error, TEXT("사전 제작 방 내부의 이동 가능 타일과 출입구가 단절되어 있습니다.")); }
		}
		FixedCount += Entry.Count;
		FixedArea += static_cast<int64>(Entry.Count) * Room->Size.X * Room->Size.Y;
		bCanStart |= Entry.MinStartDistance == 0;
	}
	if (FixedCount > S.MinRooms) { return Fail(Error, TEXT("사전 제작 방 총개수는 최소 방 개수를 초과할 수 없습니다.")); }
	if (FixedCount == S.MinRooms && !bCanStart) { return Fail(Error, TEXT("최소 방 개수에서도 시작점 거리 0을 허용하는 방이 필요합니다.")); }
	if (FixedArea + (S.MaxRooms - FixedCount) * S.MinRoomSize.X * S.MinRoomSize.Y > S.MapSize.X * S.MapSize.Y) { return Fail(Error, TEXT("최대 방 개수의 최소 점유 면적이 맵보다 큽니다.")); }
	return true;
}

bool UCGridMap::Build(const FUCGridMapSettings& S, int32 Seed, FUCGridMapLayout& Output, FString& Error)
{
	// 목표 방 개수를 시드당 한 번 결정하며 재시도 중 줄이지 않습니다.
	if (!ValidateSettings(S, Error)) { return false; }
	FRandomStream Random(Seed);
	const int32 Count = Random.RandRange(S.MinRooms, S.MaxRooms);
	int32 Budget = S.MaxSearchNodes;
	FString LastError = TEXT("방 배치 또는 필수 복도 연결 조건을 만족하지 못했습니다.");
	for (int32 Attempt = 0; Attempt < S.MaxLayoutAttempts && Budget > 0; ++Attempt)
	{
		FUCGridMapLayout Candidate;
		Candidate.Seed = Seed;
		Candidate.Size = S.MapSize;
		Candidate.Cells.Init(INDEX_NONE, S.MapSize.X * S.MapSize.Y);
		if (!PlaceRooms(S, Count, Random, Candidate) || !ConnectRooms(S, Random, Candidate, Budget)) { continue; }
		ExpandCorridors(S, Random, Candidate);

		// 완성된 연결망에서 시작점을 선택하여 보물방의 최단 이동 거리를 검사합니다.
		TArray<int32> StartRooms;
		for (int32 I = 0; I < Candidate.Rooms.Num(); ++I)
		{
			const int32 Definition = Candidate.Rooms[I].DefinitionIndex;
			if (Definition == INDEX_NONE || S.DesignedRooms[Definition].MinStartDistance == 0) { StartRooms.Add(I); }
		}
		Shuffle(StartRooms, Random);
		for (int32 StartRoom : StartRooms)
		{
			const FUCGridRoom& Room = Candidate.Rooms[StartRoom];
			FIntPoint LocalStart(Room.Size.X / 2, Room.Size.Y / 2);
			if (Room.BlockedTiles.Contains(LocalStart)) { LocalStart = Room.Doors[0].Tile; }
			Candidate.StartTile = Room.Origin + LocalStart;
			if (Validate(S, Candidate, LastError)) { Output = MoveTemp(Candidate); Error.Reset(); return true; }
		}
	}
	return Fail(Error, FString::Printf(TEXT("시드 %d, 목표 방 %d개: 제한 내 생성에 실패했습니다. 남은 탐색 노드 %d. %s"), Seed, Count, Budget, *LastError));
}

bool UCGridMap::Validate(const FUCGridMapSettings& S, const FUCGridMapLayout& Map, FString& Error)
{
	// 생성 과정의 성공 플래그 대신 결과 데이터로 제약을 다시 검사합니다.
	if (!ValidateSettings(S, Error)) { return false; }
	if (Map.Size != S.MapSize || Map.Cells.Num() != S.MapSize.X * S.MapSize.Y || Map.Rooms.Num() < S.MinRooms || Map.Rooms.Num() > S.MaxRooms || !Inside(Map.StartTile, Map.Size)) { return Fail(Error, TEXT("맵 크기, 방 개수 또는 시작점이 유효하지 않습니다.")); }
	TArray<int32> Expected;
	TArray<int32> Degrees;
	TArray<int32> DefinitionCounts;
	Expected.Init(INDEX_NONE, Map.Cells.Num());
	Degrees.Init(0, Map.Rooms.Num());
	DefinitionCounts.Init(0, S.DesignedRooms.Num());
	TSet<int32> Blocked;
	for (int32 I = 0; I < Map.Rooms.Num(); ++I)
	{
		const FUCGridRoom& Room = Map.Rooms[I];
		if (Room.Size.X < 1 || Room.Size.Y < 1 || !Inside(Room.Origin, Map.Size) || !Inside(Room.Origin + Room.Size - FIntPoint(1, 1), Map.Size)) { return Fail(Error, TEXT("방이 맵 경계를 벗어납니다.")); }
		if (Room.DefinitionIndex == INDEX_NONE)
		{
			if (Room.Size.X < S.MinRoomSize.X || Room.Size.Y < S.MinRoomSize.Y || Room.Size.X > S.MaxRoomSize.X || Room.Size.Y > S.MaxRoomSize.Y || !Room.BlockedTiles.IsEmpty()) { return Fail(Error, TEXT("랜덤 방의 크기 또는 내부 타일이 설정과 다릅니다.")); }
		}
		else
		{
			if (!S.DesignedRooms.IsValidIndex(Room.DefinitionIndex)) { return Fail(Error, TEXT("사전 제작 방 정의 인덱스가 잘못되었습니다.")); }
			const AUCDesignedRoom* Template = S.DesignedRooms[Room.DefinitionIndex].RoomClass.GetDefaultObject();
			if (Room.Size != Template->Size || Room.BlockedTiles != Template->BlockedTiles || Room.Doors.Num() != Template->Doors.Num()) { return Fail(Error, TEXT("사전 제작 방의 형태가 변경되었습니다.")); }
			for (int32 D = 0; D < Room.Doors.Num(); ++D) { if (Room.Doors[D].Tile != Template->Doors[D].Tile || Room.Doors[D].Outward != Template->Doors[D].Outward) { return Fail(Error, TEXT("사전 제작 출입구가 변경되었습니다.")); } }
			++DefinitionCounts[Room.DefinitionIndex];
		}
		for (FIntPoint P : Room.BlockedTiles) { Blocked.Add(Index(Room.Origin + P, Map.Size)); }
		for (const FUCMapDoor& Door : Room.Doors) { if (!ValidDoor(Door, Room.Size, Room.BlockedTiles)) { return Fail(Error, TEXT("방의 출입구 정보가 유효하지 않습니다.")); } }
		for (int32 J = 0; J < I; ++J) { if (!Separated(Room, Map.Rooms[J], S.MinRoomGap)) { return Fail(Error, TEXT("방 사이 최소 간격을 위반합니다.")); } }
		for (int32 Y = 0; Y < Room.Size.Y; ++Y) { for (int32 X = 0; X < Room.Size.X; ++X) { Expected[Index(Room.Origin + FIntPoint(X, Y), Map.Size)] = I; } }
	}
	for (int32 I = 0; I < DefinitionCounts.Num(); ++I) { if (DefinitionCounts[I] != S.DesignedRooms[I].Count) { return Fail(Error, TEXT("사전 제작 방 개수가 일치하지 않습니다.")); } }

	TSet<FIntPoint> UsedDoors;
	TSet<FIntPoint> UsedPairs;
	TSet<uint64> OpenBoundaries;
	// 중복 방 연결, 출입구 재사용, 복도 길이와 실제 연속성을 검사합니다.
	for (int32 I = 0; I < Map.Connections.Num(); ++I)
	{
		const FUCGridConnection& Edge = Map.Connections[I];
		if (!Map.Rooms.IsValidIndex(Edge.RoomA) || !Map.Rooms.IsValidIndex(Edge.RoomB) || Edge.RoomA == Edge.RoomB || Edge.Tiles.Num() < S.MinCorridorLength || Edge.Tiles.Num() > S.MaxCorridorLength) { return Fail(Error, TEXT("복도 대상 또는 길이가 유효하지 않습니다.")); }
		const FUCGridRoom& A = Map.Rooms[Edge.RoomA];
		const FUCGridRoom& B = Map.Rooms[Edge.RoomB];
		if (!A.Doors.IsValidIndex(Edge.DoorA) || !B.Doors.IsValidIndex(Edge.DoorB)) { return Fail(Error, TEXT("복도 출입구 인덱스가 유효하지 않습니다.")); }
		const FIntPoint Pair(FMath::Min(Edge.RoomA, Edge.RoomB), FMath::Max(Edge.RoomA, Edge.RoomB));
		const FIntPoint DA(Edge.RoomA, Edge.DoorA);
		const FIntPoint DB(Edge.RoomB, Edge.DoorB);
		if (UsedPairs.Contains(Pair) || UsedDoors.Contains(DA) || UsedDoors.Contains(DB)) { return Fail(Error, TEXT("방 연결 또는 출입구를 중복 사용합니다.")); }
		UsedPairs.Add(Pair); UsedDoors.Add(DA); UsedDoors.Add(DB);
		++Degrees[Edge.RoomA]; ++Degrees[Edge.RoomB];
		const FIntPoint DoorA = A.Origin + A.Doors[Edge.DoorA].Tile;
		const FIntPoint DoorB = B.Origin + B.Doors[Edge.DoorB].Tile;
		if (Edge.Tiles[0] != DoorA + A.Doors[Edge.DoorA].Outward || Edge.Tiles.Last() != DoorB + B.Doors[Edge.DoorB].Outward) { return Fail(Error, TEXT("복도가 출입구의 바깥쪽 타일과 일치하지 않습니다.")); }
		for (int32 T = 0; T < Edge.Tiles.Num(); ++T)
		{
			const FIntPoint P = Edge.Tiles[T];
			if (!Inside(P, Map.Size) || (T > 0 && Distance(P, Edge.Tiles[T - 1]) != 1)) { return Fail(Error, TEXT("복도가 끊어지거나 맵을 벗어납니다.")); }
			if (Expected[Index(P, Map.Size)] != INDEX_NONE) { return Fail(Error, TEXT("복도가 다른 방 또는 복도와 겹칩니다.")); }
			Expected[Index(P, Map.Size)] = -I - 2;
		}
		// 넓어진 바닥도 해당 연결의 일부로 기록하며 겹침을 금지합니다.
		for (FIntPoint P : Edge.ExtraTiles)
		{
			if (!Inside(P, Map.Size) || Expected[Index(P, Map.Size)] != INDEX_NONE) { return Fail(Error, TEXT("통로 확장 타일이 겹치거나 맵을 벗어납니다.")); }
			Expected[Index(P, Map.Size)] = -I - 2;
		}
		OpenBoundaries.Add(BoundaryKey(DoorA, Edge.Tiles[0], Map.Size));
		OpenBoundaries.Add(BoundaryKey(DoorB, Edge.Tiles.Last(), Map.Size));
	}
	if (Expected != Map.Cells) { return Fail(Error, TEXT("타일 배열과 방 및 복도 데이터가 일치하지 않습니다.")); }
	for (int32 I = 0; I < Degrees.Num(); ++I) { if (Degrees[I] < RequiredDegree(S, Map.Rooms[I]) || Degrees[I] > S.MaxConnections) { return Fail(Error, TEXT("방별 연결 수 제한을 위반합니다.")); } }

	// 확장된 바닥 자체의 연결성과 출입구 사이 최단 길이를 다시 검사합니다.
	for (int32 E = 0; E < Map.Connections.Num(); ++E)
	{
		const FUCGridConnection& Edge = Map.Connections[E];
		TArray<int32> Distances;
		TArray<int32> Queue;
		Distances.Init(INDEX_NONE, Map.Cells.Num());
		const int32 Start = Index(Edge.Tiles[0], Map.Size);
		Distances[Start] = 0;
		Queue.Add(Start);
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 Current = Queue[Head];
			for (FIntPoint Step : Steps)
			{
				const FIntPoint P = Point(Current, Map.Size) + Step;
				if (!Inside(P, Map.Size)) { continue; }
				const int32 Next = Index(P, Map.Size);
				if (Expected[Next] != -E - 2 || Distances[Next] != INDEX_NONE) { continue; }
				Distances[Next] = Distances[Current] + 1;
				Queue.Add(Next);
			}
		}
		const int32 ActualLength = Distances[Index(Edge.Tiles.Last(), Map.Size)] + 1;
		if (Queue.Num() != Edge.Tiles.Num() + Edge.ExtraTiles.Num() || ActualLength < S.MinCorridorLength || ActualLength > S.MaxCorridorLength) { return Fail(Error, TEXT("확장 통로가 단절되거나 실제 최단 길이 제한을 위반합니다.")); }
	}

	// 연결 대상 방의 벽 접촉만 허용하며 다른 복도와의 합류를 금지합니다.
	for (int32 I = 0; I < Expected.Num(); ++I)
	{
		if (Expected[I] == INDEX_NONE) { continue; }
		for (FIntPoint Step : {FIntPoint(1, 0), FIntPoint(0, 1)})
		{
			const FIntPoint N = Point(I, Map.Size) + Step;
			if (!Inside(N, Map.Size)) { continue; }
			const int32 J = Index(N, Map.Size);
			const int32 Other = Expected[J];
			if (Other == INDEX_NONE) { continue; }
			int32 RoomId = Expected[I];
			int32 CorridorId = Other;
			if (Other >= 0) { RoomId = Other; CorridorId = Expected[I]; }
			if (RoomId >= 0 && CorridorId < INDEX_NONE)
			{
				const FUCGridConnection& Edge = Map.Connections[-CorridorId - 2];
				if (RoomId == Edge.RoomA || RoomId == Edge.RoomB) { continue; }
			}
			if (Other != Expected[I] && !OpenBoundaries.Contains(BoundaryKey(Point(I, Map.Size), N, Map.Size))) { return Fail(Error, TEXT("복도 합류 또는 출입구 외의 방 접촉을 발견합니다.")); }
		}
	}
	if (Expected[Index(Map.StartTile, Map.Size)] < 0 || Blocked.Contains(Index(Map.StartTile, Map.Size))) { return Fail(Error, TEXT("시작점은 방 내부의 이동 가능 타일이어야 합니다.")); }
	const TArray<int32> Distances = Flood(Map, Map.StartTile);
	for (int32 I = 0; I < Expected.Num(); ++I) { if (Expected[I] != INDEX_NONE && !Blocked.Contains(I) && Distances[I] == INDEX_NONE) { return Fail(Error, TEXT("시작점에서 도달할 수 없는 방 또는 복도 타일이 존재합니다.")); } }
	// 사전 제작 방으로 최초 진입하는 최단 이동 거리를 검사합니다.
	for (const FUCGridRoom& Room : Map.Rooms)
	{
		if (Room.DefinitionIndex == INDEX_NONE) { continue; }
		int32 Minimum = MAX_int32;
		for (int32 Y = 0; Y < Room.Size.Y; ++Y)
		{
			for (int32 X = 0; X < Room.Size.X; ++X)
			{
				const int32 D = Distances[Index(Room.Origin + FIntPoint(X, Y), Map.Size)];
				if (D >= 0) { Minimum = FMath::Min(Minimum, D); }
			}
		}
		const FUCDesignedRoomEntry& Entry = S.DesignedRooms[Room.DefinitionIndex];
		if (Minimum < Entry.MinStartDistance || (Entry.MaxStartDistance > 0 && Minimum > Entry.MaxStartDistance)) { return Fail(Error, TEXT("사전 제작 방의 시작점 거리 조건을 만족하지 못합니다.")); }
	}
	Error.Reset();
	return true;
}
