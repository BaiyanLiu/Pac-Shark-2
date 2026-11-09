// Pac Shark - pure game simulation for a classic tile-based Pac-Man.

#include "PacGame.h"

namespace PacGame
{
	// '#' wall, '.' pellet, 'o' power pellet, '-' ghost house door, ' ' empty floor.
	// Row 14 is the wrap-around tunnel. Rows 9-19 (den, tunnel and the corridors around them) are kept
	// as is; the top and bottom sections are regenerated at random for every level by BuildRandomMaze.
	const TCHAR* const Maze[FPacGame::Height] = {
		TEXT("############################"),
		TEXT("#............##............#"),
		TEXT("#.####.#####.##.#####.####.#"),
		TEXT("#o####.#####.##.#####.####o#"),
		TEXT("#.####.#####.##.#####.####.#"),
		TEXT("#..........................#"),
		TEXT("#.####.##.########.##.####.#"),
		TEXT("#.####.##.########.##.####.#"),
		TEXT("#......##....##....##......#"),
		TEXT("######.##### ## #####.######"),
		TEXT("     #.##### ## #####.#     "),
		TEXT("     #.##          ##.#     "),
		TEXT("     #.## ###--### ##.#     "),
		TEXT("######.## #      # ##.######"),
		TEXT("      .   #      #   .      "),
		TEXT("######.## #      # ##.######"),
		TEXT("     #.## ######## ##.#     "),
		TEXT("     #.##          ##.#     "),
		TEXT("     #.## ######## ##.#     "),
		TEXT("######.## ######## ##.######"),
		TEXT("#............##............#"),
		TEXT("#.####.#####.##.#####.####.#"),
		TEXT("#.####.#####.##.#####.####.#"),
		TEXT("#o..##.......  .......##..o#"),
		TEXT("###.##.##.########.##.##.###"),
		TEXT("###.##.##.########.##.##.###"),
		TEXT("#......##....##....##......#"),
		TEXT("#.##########.##.##########.#"),
		TEXT("#.##########.##.##########.#"),
		TEXT("#..........................#"),
		TEXT("############################"),
	};

	const FIntPoint PacStart(13, 23);
	const FIntPoint HouseExit(13, 11);	// tile just above the door
	const FIntPoint HouseCenter(13, 14);

	// Scatter / chase alternation in seconds, starting with scatter. Chase forever afterwards.
	const float ModeDurations[] = { 7.f, 20.f, 7.f, 20.f, 5.f, 20.f, 5.f };
	constexpr int32 NumModes = UE_ARRAY_COUNT(ModeDurations);

	constexpr float PacSpeed = 7.5f;		// tiles per second
	constexpr float PacFrightSpeed = 8.5f;
	constexpr float GhostSpeed = 7.0f;
	constexpr float GhostFrightSpeed = 4.5f;
	constexpr float GhostTunnelSpeed = 3.5f;
	constexpr float EyesSpeed = 15.f;
	constexpr float HouseSpeed = 3.f;
	constexpr float CollisionDistance = 0.7f;
	constexpr float CornerWindow = 0.4f;	// tiles before/after a center where an early or late turn still counts
	constexpr float MaxStep = 1.f / 60.f;
	constexpr int32 ExtraLifeScore = 10000;

	const EPacDir DecisionOrder[] = { EPacDir::Up, EPacDir::Left, EPacDir::Down, EPacDir::Right };

	FIntPoint DirOffset(EPacDir Dir)
	{
		switch (Dir)
		{
		case EPacDir::Up: return FIntPoint(0, -1);
		case EPacDir::Down: return FIntPoint(0, 1);
		case EPacDir::Left: return FIntPoint(-1, 0);
		case EPacDir::Right: return FIntPoint(1, 0);
		default: return FIntPoint(0, 0);
		}
	}

	EPacDir Opposite(EPacDir Dir)
	{
		switch (Dir)
		{
		case EPacDir::Up: return EPacDir::Down;
		case EPacDir::Down: return EPacDir::Up;
		case EPacDir::Left: return EPacDir::Right;
		case EPacDir::Right: return EPacDir::Left;
		default: return EPacDir::None;
		}
	}

	FIntPoint ToTile(const FVector2D& Pos)
	{
		return FIntPoint(FMath::RoundToInt(Pos.X), FMath::RoundToInt(Pos.Y));
	}

	bool IsAtCenter(const FVector2D& Pos)
	{
		return FMath::Abs(Pos.X - FMath::RoundToDouble(Pos.X)) < 1e-4 && FMath::Abs(Pos.Y - FMath::RoundToDouble(Pos.Y)) < 1e-4;
	}

	// Column X folded into the maze, so the side tunnel joins the left and right edges.
	int32 WrapX(int32 X)
	{
		return ((X % FPacGame::Width) + FPacGame::Width) % FPacGame::Width;
	}

	// The tile one step from Tile in Dir, wrapping through the side tunnel.
	FIntPoint Neighbor(const FIntPoint& Tile, EPacDir Dir)
	{
		return FIntPoint(WrapX(Tile.X + DirOffset(Dir).X), Tile.Y + DirOffset(Dir).Y);
	}

	void WrapTunnel(FVector2D& Pos)
	{
		if (Pos.X <= -1.0)
		{
			Pos.X += FPacGame::Width;
		}
		else if (Pos.X >= FPacGame::Width)
		{
			Pos.X -= FPacGame::Width;
		}
	}

	// Scripted movement used inside the ghost house: X first, then Y. Returns true on arrival.
	bool MoveToward(FVector2D& Pos, const FIntPoint& Target, float Distance)
	{
		const double DX = Target.X - Pos.X;
		const double StepX = FMath::Clamp<double>(DX, -Distance, Distance);
		Pos.X += StepX;
		Distance -= FMath::Abs(StepX);

		const double DY = Target.Y - Pos.Y;
		const double StepY = FMath::Clamp<double>(DY, -Distance, Distance);
		Pos.Y += StepY;

		return FMath::Abs(Target.X - Pos.X) < 1e-4 && FMath::Abs(Target.Y - Pos.Y) < 1e-4;
	}

	// Corridor lattice for the random sections. Every corridor runs between two of these nodes, and the
	// gaps between them are wide enough that open water never forms a 2x2 block.
	const int32 NodeCols[] = { 1, 6, 9, 12, 15, 18, 21, 26 };
	constexpr int32 NumNodeCols = UE_ARRAY_COUNT(NodeCols);
	const int32 BottomRows[] = { 20, 23, 26, 29 };
	// Power pellet spots in the order they are used; every one is open in any generated maze.
	const FIntPoint PowerPellets[] = {
		FIntPoint(1, 3), FIntPoint(26, 3), FIntPoint(1, 23), FIntPoint(26, 23),
		FIntPoint(6, 14), FIntPoint(21, 14), FIntPoint(1, 29), FIntPoint(26, 29)
	};
	static_assert(UE_ARRAY_COUNT(PowerPellets) == FPacGame::MaxPowerPellets, "one spot per possible power pellet");

	struct FMazeEdge
	{
		FIntPoint A;
		FIntPoint B;
		bool bOpen = true;
	};

	void CarveEdges(TArray<char>& Grid, const TArray<FMazeEdge>& Edges)
	{
		constexpr int32 W = FPacGame::Width;
		for (int32 Y = 1; Y < FPacGame::Height - 1; ++Y)
		{
			if (Y >= 9 && Y <= 19)
			{
				continue;
			}
			for (int32 X = 1; X < W - 1; ++X)
			{
				Grid[Y * W + X] = '#';
			}
		}
		for (const FMazeEdge& Edge : Edges)
		{
			if (!Edge.bOpen)
			{
				continue;
			}
			for (int32 Y = Edge.A.Y; Y <= Edge.B.Y; ++Y)
			{
				for (int32 X = Edge.A.X; X <= Edge.B.X; ++X)
				{
					Grid[Y * W + X] = '.';
				}
			}
		}
	}

	// Every pellet must be reachable from the start, and nothing reachable may be a dead end.
	bool IsPlayable(const TArray<char>& Grid)
	{
		constexpr int32 W = FPacGame::Width;
		constexpr int32 H = FPacGame::Height;
		auto IsOpen = [&Grid](const FIntPoint& Tile)
		{
			return Tile.Y >= 0 && Tile.Y < FPacGame::Height && Grid[Tile.Y * FPacGame::Width + Tile.X] != '#';
		};

		TArray<bool> Seen;
		Seen.Init(false, W * H);
		TArray<FIntPoint> Stack;
		Stack.Add(PacStart);
		Seen[PacStart.Y * W + PacStart.X] = true;
		while (Stack.Num() > 0)
		{
			const FIntPoint Tile = Stack.Pop();
			int32 Exits = 0;
			for (EPacDir Dir : DecisionOrder)
			{
				const FIntPoint Next = Neighbor(Tile, Dir);
				if (!IsOpen(Next))
				{
					continue;
				}
				++Exits;
				if (!Seen[Next.Y * W + Next.X])
				{
					Seen[Next.Y * W + Next.X] = true;
					Stack.Add(Next);
				}
			}
			if (Exits < 2)
			{
				return false;
			}
		}

		for (int32 i = 0; i < W * H; ++i)
		{
			if ((Grid[i] == '.' || Grid[i] == 'o') && !Seen[i])
			{
				return false;
			}
		}
		return true;
	}

	// Keeps the den and tunnel band, then builds a fresh left/right symmetric layout above and below it:
	// start from a full lattice of corridors and knock out random ones while the maze stays playable.
	void BuildRandomMaze(TArray<char>& Grid, int32 NumPowerPellets)
	{
		constexpr int32 W = FPacGame::Width;
		Grid.SetNum(W * FPacGame::Height);
		for (int32 Y = 0; Y < FPacGame::Height; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				Grid[Y * W + X] = static_cast<char>(Maze[Y][X]);
			}
		}

		const int32 TopRows[] = { 1, FMath::RandHelper(2) == 0 ? 4 : 5, 8 };
		TArray<FMazeEdge> Edges;
		auto AddSection = [&Edges](const int32* Rows, int32 NumRows)
		{
			for (int32 r = 0; r < NumRows; ++r)
			{
				for (int32 c = 0; c + 1 < NumNodeCols; ++c)
				{
					FMazeEdge Edge;
					Edge.A = FIntPoint(NodeCols[c], Rows[r]);
					Edge.B = FIntPoint(NodeCols[c + 1], Rows[r]);
					Edges.Add(Edge);
				}
			}
			for (int32 r = 0; r + 1 < NumRows; ++r)
			{
				for (int32 c = 0; c < NumNodeCols; ++c)
				{
					FMazeEdge Edge;
					Edge.A = FIntPoint(NodeCols[c], Rows[r]);
					Edge.B = FIntPoint(NodeCols[c], Rows[r + 1]);
					Edges.Add(Edge);
				}
			}
		};
		AddSection(TopRows, 3);
		AddSection(BottomRows, 4);

		// Pair each left-half edge with its mirror image. The start corridor under the den always stays.
		auto FindEdge = [&Edges](const FIntPoint& A, const FIntPoint& B)
		{
			for (int32 i = 0; i < Edges.Num(); ++i)
			{
				if (Edges[i].A == A && Edges[i].B == B)
				{
					return i;
				}
			}
			return -1;
		};
		TArray<FIntPoint> Pairs;
		for (int32 i = 0; i < Edges.Num(); ++i)
		{
			const FMazeEdge& Edge = Edges[i];
			if (Edge.A.X >= W / 2 || (Edge.A.Y == PacStart.Y && Edge.A.X < PacStart.X && Edge.B.X > PacStart.X))
			{
				continue;
			}
			const int32 Mirror = Edge.A.X < W / 2 && Edge.B.X >= W / 2 ? i
				: FindEdge(FIntPoint(W - 1 - Edge.B.X, Edge.A.Y), FIntPoint(W - 1 - Edge.A.X, Edge.B.Y));
			Pairs.Add(FIntPoint(i, Mirror));
		}
		for (int32 i = Pairs.Num() - 1; i > 0; --i)
		{
			Swap(Pairs[i], Pairs[FMath::RandHelper(i + 1)]);
		}

		for (const FIntPoint& Pair : Pairs)
		{
			// Only about a fifth of the corridors are tried for removal, so every maze keeps plenty of paths.
			if (FMath::RandHelper(100) >= 20)
			{
				continue;
			}
			Edges[Pair.X].bOpen = false;
			Edges[Pair.Y].bOpen = false;
			CarveEdges(Grid, Edges);
			if (!IsPlayable(Grid))
			{
				Edges[Pair.X].bOpen = true;
				Edges[Pair.Y].bOpen = true;
			}
		}
		CarveEdges(Grid, Edges);

		for (int32 i = 0; i < NumPowerPellets; ++i)
		{
			Grid[PowerPellets[i].Y * W + PowerPellets[i].X] = 'o';
		}
		Grid[PacStart.Y * W + PacStart.X] = ' ';
		Grid[PacStart.Y * W + PacStart.X + 1] = ' ';
	}
}

using namespace PacGame;

void FPacGame::NewGame()
{
	Score = 0;
	Lives = 3;
	Level = 1;
	bExtraLifeAwarded = false;
	StartLevel();
}

void FPacGame::StartLevel()
{
	TArray<char> Grid;
	BuildRandomMaze(Grid, PowerPelletCount);

	Tiles.SetNum(Width * Height);
	PelletsLeft = 0;
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			EPacTile Tile = EPacTile::Empty;
			switch (Grid[Y * Width + X])
			{
			case '#': Tile = EPacTile::Wall; break;
			case '-': Tile = EPacTile::Door; break;
			case '.': Tile = EPacTile::Pellet; ++PelletsLeft; break;
			case 'o': Tile = EPacTile::PowerPellet; ++PelletsLeft; break;
			default: break;
			}
			Tiles[Y * Width + X] = Tile;
		}
	}
	LevelPelletTotal = FMath::Max(1, PelletsLeft);
	++MazeId;
	BuildHomeDistances();
	Ghosts.SetNum(GhostCount);
	ResetActors();
}

void FPacGame::BuildHomeDistances()
{
	// Breadth-first search out from the den exit over open water, wrapping through the side tunnel.
	HomeDistance.Init(-1, Width * Height);
	TArray<FIntPoint> Queue;
	Queue.Add(HouseExit);
	HomeDistance[HouseExit.Y * Width + HouseExit.X] = 0;
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const FIntPoint Tile = Queue[Head];
		const int32 Dist = HomeDistance[Tile.Y * Width + Tile.X];
		for (EPacDir Dir : DecisionOrder)
		{
			if (!CanMove(Tile, Dir))
			{
				continue;
			}
			const FIntPoint Next = Neighbor(Tile, Dir);
			int32& NextDist = HomeDistance[Next.Y * Width + Next.X];
			if (NextDist < 0)
			{
				NextDist = Dist + 1;
				Queue.Add(Next);
			}
		}
	}
}

void FPacGame::ResetActors()
{
	PacPos = FVector2D(PacStart.X, PacStart.Y);
	PacDir = EPacDir::Left;
	PacFacing = EPacDir::Left;
	QueuedDir = EPacDir::None;

	// Ghosts past the classic four reuse their personalities and wait in the den a little longer each.
	const FIntPoint Starts[] = { HouseExit, HouseCenter, FIntPoint(11, 14), FIntPoint(15, 14) };
	// Extras wait in the den's upper and lower rows so they don't sit exactly on top of the first four.
	const FIntPoint ExtraStarts[] = { FIntPoint(12, 13), FIntPoint(14, 13), FIntPoint(12, 15), FIntPoint(14, 15) };
	const FIntPoint Corners[] = { FIntPoint(25, -3), FIntPoint(2, -3), FIntPoint(27, 31), FIntPoint(0, 31) };
	const float Releases[] = { 0.f, 0.5f, 4.f, 8.f };
	for (int32 i = 0; i < Ghosts.Num(); ++i)
	{
		FPacGhost& Ghost = Ghosts[i];
		Ghost = FPacGhost();
		const FIntPoint Start = i < 4 ? Starts[i] : ExtraStarts[(i - 4) % 4];
		Ghost.Pos = FVector2D(Start.X, Start.Y);
		Ghost.ScatterTarget = Corners[i % 4];
		Ghost.ReleaseDelay = i < 4 ? Releases[i] : 8.f + 3.f * (i - 3);
		Ghost.State = (i == Blinky) ? EPacGhostState::Active : EPacGhostState::InHouse;
		Ghost.Dir = (i == Blinky) ? EPacDir::Left : EPacDir::Up;
	}

	Popups.Reset();
	FrightTimer = 0.f;
	FreezeTimer = 0.f;
	InvincibleTimer = 0.f;
	GhostCombo = 0;
	ModeIndex = 0;
	ModeTimer = ModeDurations[0];
	Phase = EPacGamePhase::Ready;
	PhaseTimer = ReadyDuration;
}

EPacTile FPacGame::GetTile(int32 X, int32 Y) const
{
	if (Y < 0 || Y >= Height || Tiles.Num() != Width * Height)
	{
		return EPacTile::Wall;
	}
	X = WrapX(X);
	return Tiles[Y * Width + X];
}

bool FPacGame::CanMove(const FIntPoint& Tile, EPacDir Dir) const
{
	if (Dir == EPacDir::None)
	{
		return false;
	}
	const FIntPoint Next = Tile + DirOffset(Dir);
	const EPacTile NextTile = GetTile(Next.X, Next.Y);
	return NextTile != EPacTile::Wall && NextTile != EPacTile::Door;
}

float FPacGame::GetLevelSpeedScale() const
{
	return FMath::Min(1.f + 0.05f * (Level - 1), 1.25f);
}

void FPacGame::Tick(float DeltaSeconds)
{
	DeltaSeconds = FMath::Clamp(DeltaSeconds, 0.f, 0.1f);

	for (int32 i = Popups.Num() - 1; i >= 0; --i)
	{
		Popups[i].TimeLeft -= DeltaSeconds;
		if (Popups[i].TimeLeft <= 0.f)
		{
			Popups.RemoveAt(i);
		}
	}

	switch (Phase)
	{
	case EPacGamePhase::Ready:
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.f)
		{
			Phase = EPacGamePhase::Playing;
		}
		break;

	case EPacGamePhase::Playing:
		// Sub-step so fast movers never tunnel through each other on a slow frame.
		while (DeltaSeconds > 0.f && Phase == EPacGamePhase::Playing)
		{
			const float Step = FMath::Min(DeltaSeconds, MaxStep);
			DeltaSeconds -= Step;
			if (FreezeTimer > 0.f)
			{
				FreezeTimer -= Step;
				continue;
			}
			TickPlaying(Step);
		}
		break;

	case EPacGamePhase::Dying:
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.f)
		{
			--Lives;
			if (Lives <= 0)
			{
				Phase = EPacGamePhase::GameOver;
			}
			else
			{
				ResetActors();
			}
		}
		break;

	case EPacGamePhase::LevelClear:
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.f)
		{
			++Level;
			StartLevel();
		}
		break;

	case EPacGamePhase::GameOver:
		break;
	}
}

void FPacGame::TickPlaying(float Dt)
{
	InvincibleTimer = FMath::Max(0.f, InvincibleTimer - Dt);
	if (FrightTimer > 0.f)
	{
		FrightTimer -= Dt;
		if (FrightTimer <= 0.f)
		{
			FrightTimer = 0.f;
			for (FPacGhost& Ghost : Ghosts)
			{
				Ghost.bFrightened = false;
			}
		}
	}
	else if (ModeIndex < NumModes)
	{
		// The scatter/chase clock pauses while ghosts are frightened.
		ModeTimer -= Dt;
		if (ModeTimer <= 0.f)
		{
			++ModeIndex;
			ModeTimer = ModeIndex < NumModes ? ModeDurations[ModeIndex] : 0.f;
			ReverseActiveGhosts();
		}
	}

	MovePac(Dt);
	EatAtPac();
	if (Phase != EPacGamePhase::Playing)
	{
		return;
	}
	CheckCollisions();
	if (Phase != EPacGamePhase::Playing || FreezeTimer > 0.f)
	{
		return;
	}
	for (int32 i = 0; i < Ghosts.Num(); ++i)
	{
		MoveGhost(i, Dt);
	}
	CheckCollisions();
}

void FPacGame::Advance(FVector2D& Pos, EPacDir& Dir, float Distance, TFunctionRef<void(const FIntPoint&, EPacDir&)> Decide) const
{
	for (int32 Guard = 0; Distance > 1e-6f && Guard < 16; ++Guard)
	{
		if (IsAtCenter(Pos))
		{
			Pos = FVector2D(FMath::RoundToDouble(Pos.X), FMath::RoundToDouble(Pos.Y));
			Decide(ToTile(Pos), Dir);
		}
		if (Dir == EPacDir::None)
		{
			return;
		}

		const FIntPoint Offset = DirOffset(Dir);
		// Coordinate along the movement axis, flipped so that moving forward always increases it.
		const double Along = Offset.X != 0 ? Pos.X * Offset.X : Pos.Y * Offset.Y;
		const double ToNextCenter = FMath::FloorToDouble(Along) + 1.0 - Along;

		if (Distance < ToNextCenter - 1e-6)
		{
			Pos += FVector2D(Offset.X, Offset.Y) * Distance;
			Distance = 0.f;
		}
		else
		{
			Pos += FVector2D(Offset.X, Offset.Y) * ToNextCenter;
			Pos = FVector2D(FMath::RoundToDouble(Pos.X), FMath::RoundToDouble(Pos.Y));
			Distance -= ToNextCenter;
			WrapTunnel(Pos);
		}
	}
}

void FPacGame::MovePac(float Dt)
{
	// Reversing is allowed at any time.
	if (QueuedDir != EPacDir::None && QueuedDir == Opposite(PacDir))
	{
		PacDir = QueuedDir;
	}

	// Cornering: a perpendicular turn is accepted a little before or after a tile center, and the
	// shark glides back onto the new lane while it moves, so turns never need pixel-perfect timing.
	if (QueuedDir != EPacDir::None && PacDir != EPacDir::None && QueuedDir != PacDir && QueuedDir != Opposite(PacDir))
	{
		const FIntPoint Nearest = ToTile(PacPos);
		const double OffCenter = FMath::Abs(PacPos.X - Nearest.X) + FMath::Abs(PacPos.Y - Nearest.Y);
		if (OffCenter > 1e-4 && OffCenter < CornerWindow && CanMove(Nearest, QueuedDir))
		{
			PacDir = QueuedDir;
		}
	}

	const float Speed = (FrightTimer > 0.f ? PacFrightSpeed : PacSpeed) * GetLevelSpeedScale();
	const float Distance = Speed * Dt;

	// Slide the cross-axis coordinate toward the lane center at the same pace as forward movement.
	if (PacDir != EPacDir::None)
	{
		double& Lane = DirOffset(PacDir).X != 0 ? PacPos.Y : PacPos.X;
		const double LaneCenter = FMath::RoundToDouble(Lane);
		Lane += FMath::Clamp<double>(LaneCenter - Lane, -Distance, Distance);
	}

	Advance(PacPos, PacDir, Distance, [this](const FIntPoint& Tile, EPacDir& Dir)
	{
		if (QueuedDir != EPacDir::None && CanMove(Tile, QueuedDir))
		{
			Dir = QueuedDir;
		}
		if (!CanMove(Tile, Dir))
		{
			Dir = EPacDir::None;
		}
	});

	if (PacDir != EPacDir::None)
	{
		PacFacing = PacDir;
	}
}

FIntPoint FPacGame::GetGhostTarget(int32 GhostIndex) const
{
	const FPacGhost& Ghost = Ghosts[GhostIndex];
	if (Ghost.State == EPacGhostState::Eaten)
	{
		return HouseExit;
	}

	const EPacPersonality Personality = GetPersonality(GhostIndex);
	const FIntPoint PacTile = ToTile(PacPos);

	// The Stalker never rests, and an angry Hunter (few dots left) skips the breaks too.
	const bool bAngryHunter = Personality == EPacPersonality::Hunter && PelletsLeft * 10 < LevelPelletTotal * 3;
	const bool bChase = (ModeIndex % 2) == 1 || Personality == EPacPersonality::Stalker || bAngryHunter;
	if (!bChase)
	{
		return Ghost.ScatterTarget;
	}

	const FIntPoint Facing = DirOffset(PacFacing);
	switch (Personality)
	{
	case EPacPersonality::Ambusher:
		return PacTile + Facing * 4;
	case EPacPersonality::Flanker:
	{
		const FIntPoint Pivot = PacTile + Facing * 2;
		const FIntPoint HunterTile = ToTile(Ghosts[Blinky].Pos);
		return Pivot + (Pivot - HunterTile);
	}
	case EPacPersonality::Shy:
	{
		const FIntPoint Delta = PacTile - ToTile(Ghost.Pos);
		return (Delta.X * Delta.X + Delta.Y * Delta.Y > 64) ? PacTile : Ghost.ScatterTarget;
	}
	case EPacPersonality::Mirror:
		// The point opposite the shark through the middle of the maze.
		return FIntPoint(Width - 1 - PacTile.X, Height - 1 - PacTile.Y);
	case EPacPersonality::Hunter:
	case EPacPersonality::Wanderer:
	case EPacPersonality::Sprinter:
	case EPacPersonality::Stalker:
	default:
		return PacTile;
	}
}

const TCHAR* FPacGame::GetPersonalityName(int32 GhostIndex)
{
	static const TCHAR* const Names[] = { TEXT("HUNTER"), TEXT("AMBUSHER"), TEXT("FLANKER"), TEXT("SHY"),
		TEXT("WANDERER"), TEXT("SPRINTER"), TEXT("MIRROR"), TEXT("STALKER") };
	return Names[static_cast<int32>(GetPersonality(GhostIndex))];
}

float FPacGame::GetPersonalitySpeed(int32 GhostIndex) const
{
	if (!Ghosts.IsValidIndex(GhostIndex))
	{
		return 1.f;
	}
	switch (GetPersonality(GhostIndex))
	{
	case EPacPersonality::Hunter:
		// Speeds up as the dots run out, like the arcade's "Cruise Elroy".
		return PelletsLeft * 10 < LevelPelletTotal * 3 ? 1.15f : 1.05f;
	case EPacPersonality::Wanderer:
		return 0.9f;
	case EPacPersonality::Sprinter:
		// 1.5 s dash, then 2 s rest.
		return FMath::Fmod(Ghosts[GhostIndex].PersonalityTimer, 3.5f) < 1.5f ? 1.4f : 0.6f;
	case EPacPersonality::Stalker:
		return 0.8f;
	default:
		return 1.f;
	}
}

EPacDir FPacGame::ChooseEyesDir(const FPacGhost& Ghost, const FIntPoint& Tile) const
{
	// Eaten eyes take the shortest way home. Aiming straight at the exit (as the arcade does) can circle a
	// sand bank forever in some of the random mazes. They keep going forward on ties, and may turn back.
	EPacDir Best = EPacDir::None;
	int32 BestCost = MAX_int32;
	for (EPacDir Dir : DecisionOrder)
	{
		if (!CanMove(Tile, Dir))
		{
			continue;
		}
		const FIntPoint Next = Neighbor(Tile, Dir);
		const int32 Dist = HomeDistance[Next.Y * Width + Next.X];
		if (Dist < 0)
		{
			continue;
		}
		const int32 Cost = Dist * 2 + (Dir == Opposite(Ghost.Dir) ? 1 : 0);
		if (Cost < BestCost)
		{
			BestCost = Cost;
			Best = Dir;
		}
	}
	return Best != EPacDir::None ? Best : Opposite(Ghost.Dir);
}

EPacDir FPacGame::ChooseGhostDir(int32 GhostIndex, const FIntPoint& Tile) const
{
	const FPacGhost& Ghost = Ghosts[GhostIndex];
	if (Ghost.State == EPacGhostState::Eaten && HomeDistance.Num() == Width * Height)
	{
		return ChooseEyesDir(Ghost, Tile);
	}

	TArray<EPacDir, TInlineAllocator<4>> Options;
	for (EPacDir Dir : DecisionOrder)
	{
		if (Dir != Opposite(Ghost.Dir) && CanMove(Tile, Dir))
		{
			Options.Add(Dir);
		}
	}
	if (Options.Num() == 0)
	{
		return Opposite(Ghost.Dir);
	}
	if (Ghost.bFrightened)
	{
		return Options[FMath::RandHelper(Options.Num())];
	}
	// The Wanderer mostly picks a random way at each junction, and only now and then turns toward the shark.
	if (GetPersonality(GhostIndex) == EPacPersonality::Wanderer && Ghost.State == EPacGhostState::Active && FMath::RandHelper(10) < 7)
	{
		return Options[FMath::RandHelper(Options.Num())];
	}

	const FIntPoint Target = GetGhostTarget(GhostIndex);
	EPacDir Best = Options[0];
	int32 BestDistSq = MAX_int32;
	for (EPacDir Dir : Options)
	{
		const FIntPoint Delta = Tile + DirOffset(Dir) - Target;
		const int32 DistSq = Delta.X * Delta.X + Delta.Y * Delta.Y;
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Dir;
		}
	}
	return Best;
}

void FPacGame::MoveGhost(int32 GhostIndex, float Dt)
{
	FPacGhost& Ghost = Ghosts[GhostIndex];
	switch (Ghost.State)
	{
	case EPacGhostState::InHouse:
		Ghost.ReleaseDelay -= Dt;
		if (Ghost.ReleaseDelay <= 0.f)
		{
			Ghost.State = EPacGhostState::Leaving;
		}
		return;

	case EPacGhostState::Leaving:
		if (MoveToward(Ghost.Pos, HouseExit, HouseSpeed * Dt))
		{
			Ghost.Pos = FVector2D(HouseExit.X, HouseExit.Y);
			Ghost.State = EPacGhostState::Active;
			Ghost.Dir = EPacDir::Left;
			Ghost.LastDecisionTile = HouseExit;
		}
		return;

	case EPacGhostState::Entering:
		if (MoveToward(Ghost.Pos, HouseCenter, EyesSpeed * 0.5f * Dt))
		{
			Ghost.State = EPacGhostState::Leaving;
			Ghost.bFrightened = false;
		}
		return;

	case EPacGhostState::Active:
	case EPacGhostState::Eaten:
		break;
	}

	float Speed = GhostSpeed * GetLevelSpeedScale();
	const FIntPoint Tile = ToTile(Ghost.Pos);
	if (Ghost.State == EPacGhostState::Eaten)
	{
		Speed = EyesSpeed;
	}
	else if (Tile.Y == 14 && (Tile.X <= 5 || Tile.X >= 22))
	{
		Speed = GhostTunnelSpeed;
	}
	else if (Ghost.bFrightened)
	{
		Speed = GhostFrightSpeed;
	}
	if (Ghost.State != EPacGhostState::Eaten)
	{
		Ghost.PersonalityTimer += Dt;
		Speed *= GhostSpeedScale;
		if (!Ghost.bFrightened)
		{
			Speed *= GetPersonalitySpeed(GhostIndex);
		}
	}

	// Only decide once per tile, even if a frame ends exactly on a tile center.
	if (!IsAtCenter(Ghost.Pos))
	{
		Ghost.LastDecisionTile = FIntPoint(-100, -100);
	}

	Advance(Ghost.Pos, Ghost.Dir, Speed * Dt, [this, &Ghost, GhostIndex](const FIntPoint& At, EPacDir& Dir)
	{
		if (At == Ghost.LastDecisionTile)
		{
			return;
		}
		Ghost.LastDecisionTile = At;
		if (Ghost.State == EPacGhostState::Eaten && At == HouseExit)
		{
			Ghost.State = EPacGhostState::Entering;
			Dir = EPacDir::None;
			return;
		}
		Dir = ChooseGhostDir(GhostIndex, At);
	});

	if (Ghost.State == EPacGhostState::Entering)
	{
		Ghost.Dir = EPacDir::Down;
	}
}

void FPacGame::EatAtPac()
{
	const FIntPoint Tile = ToTile(PacPos);
	if (Tile.Y < 0 || Tile.Y >= Height)
	{
		return;
	}
	const int32 X = WrapX(Tile.X);
	EPacTile& Cell = Tiles[Tile.Y * Width + X];
	if (Cell == EPacTile::Pellet)
	{
		Cell = EPacTile::Empty;
		--PelletsLeft;
		AddScore(10);
		Events.Add(EPacEvent::Pellet);
	}
	else if (Cell == EPacTile::PowerPellet)
	{
		Cell = EPacTile::Empty;
		--PelletsLeft;
		AddScore(50);
		StartFright();
		Events.Add(EPacEvent::PowerPellet);
	}

	if (PelletsLeft <= 0)
	{
		Phase = EPacGamePhase::LevelClear;
		PhaseTimer = LevelClearDuration;
		Events.Add(EPacEvent::LevelClear);
	}
}

void FPacGame::StartFright()
{
	FrightTimer = FMath::Max(6.f - (Level - 1), 2.f);
	GhostCombo = 0;
	for (FPacGhost& Ghost : Ghosts)
	{
		if (Ghost.State != EPacGhostState::Eaten && Ghost.State != EPacGhostState::Entering)
		{
			Ghost.bFrightened = true;
		}
	}
	ReverseActiveGhosts();
}

void FPacGame::ReverseActiveGhosts()
{
	for (FPacGhost& Ghost : Ghosts)
	{
		if (Ghost.State == EPacGhostState::Active)
		{
			Ghost.Dir = Opposite(Ghost.Dir);
		}
	}
}

void FPacGame::CheckCollisions()
{
	for (FPacGhost& Ghost : Ghosts)
	{
		if (Ghost.State != EPacGhostState::Active && Ghost.State != EPacGhostState::Leaving)
		{
			continue;
		}
		if (FVector2D::Distance(Ghost.Pos, PacPos) > CollisionDistance)
		{
			continue;
		}

		if (Ghost.bFrightened)
		{
			// Caught in the doorway on its way out: the eyes sink straight back into the den, since the
			// door would otherwise shut them out of the maze and in with nowhere to go.
			const bool bInDoorway = Ghost.State == EPacGhostState::Leaving;
			Ghost.State = bInDoorway ? EPacGhostState::Entering : EPacGhostState::Eaten;
			Ghost.Dir = bInDoorway ? EPacDir::Down : Ghost.Dir;
			Ghost.bFrightened = false;
			Ghost.LastDecisionTile = FIntPoint(-100, -100);
			++GhostCombo;
			const int32 Points = 100 << GhostCombo;	// doubles for each octopus caught: 200, 400, 800, 1600...
			AddScore(Points);
			Popups.Add({ Ghost.Pos, Points, 1.f });
			FreezeTimer = 0.6f;
			Events.Add(EPacEvent::GhostEaten);
		}
		else if (InvincibleTimer > 0.f)
		{
			continue;
		}
		else if (Lives > 1)
		{
			// Lose a life but keep swimming from here, flashing and untouchable for a few seconds.
			--Lives;
			InvincibleTimer = InvincibleDuration;
			Events.Add(EPacEvent::Hit);
			return;
		}
		else
		{
			// The last life: play the death and end the game.
			Phase = EPacGamePhase::Dying;
			PhaseTimer = DyingDuration;
			Events.Add(EPacEvent::Death);
			return;
		}
	}
}

void FPacGame::AddScore(int32 Points)
{
	Score += Points;
	HighScore = FMath::Max(HighScore, Score);
	if (!bExtraLifeAwarded && Score >= ExtraLifeScore)
	{
		bExtraLifeAwarded = true;
		++Lives;
		Events.Add(EPacEvent::ExtraLife);
	}
}
