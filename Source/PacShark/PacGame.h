// Pac Shark - pure game simulation for a classic tile-based Pac-Man.
// No UObjects here: the game mode ticks it and the HUD draws it.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

enum class EPacDir : uint8
{
	None,
	Up,
	Down,
	Left,
	Right
};

enum class EPacTile : uint8
{
	Empty,
	Wall,
	Door,
	Pellet,
	PowerPellet
};

enum class EPacGhostState : uint8
{
	InHouse,	// waiting inside the ghost house
	Leaving,	// scripted walk out through the door
	Active,		// roaming the maze (chase / scatter / frightened)
	Eaten,		// eyes heading back to the house
	Entering	// eyes walking down through the door
};

enum class EPacGamePhase : uint8
{
	Ready,
	Playing,
	Dying,
	LevelClear,
	GameOver
};

// Things that just happened, for sound effects. The game mode drains them every frame.
enum class EPacEvent : uint8
{
	Pellet,
	PowerPellet,
	GhostEaten,
	Death,
	Hit,		// caught with lives to spare: lose one and keep swimming, briefly invincible
	LevelClear,
	ExtraLife
};

// Each octopus slot has its own way of hunting. The first four are the arcade's; the rest are new.
enum class EPacPersonality : uint8
{
	Hunter,		// heads straight for the shark, and speeds up when few dots are left
	Ambusher,	// aims four tiles ahead of the shark to cut it off
	Flanker,	// uses the Hunter's position to pincer the shark from the other side
	Shy,		// chases from afar but backs off to its corner when close
	Wanderer,	// drifts about at random, only sometimes turning toward the shark
	Sprinter,	// chases in bursts: a fast dash, then a slow rest
	Mirror,		// mirrors the shark's position across the maze
	Stalker		// slow, but never takes a break from the chase
};

struct FPacGhost
{
	FVector2D Pos = FVector2D::ZeroVector;
	EPacDir Dir = EPacDir::Left;
	EPacGhostState State = EPacGhostState::InHouse;
	bool bFrightened = false;
	float ReleaseDelay = 0.f;
	FIntPoint ScatterTarget = FIntPoint(0, 0);
	FIntPoint LastDecisionTile = FIntPoint(-100, -100);
	float PersonalityTimer = 0.f;
};

struct FPacPopup
{
	FVector2D Pos = FVector2D::ZeroVector;
	int32 Points = 0;
	float TimeLeft = 0.f;
};

class FPacGame
{
public:
	static constexpr int32 Width = 28;
	static constexpr int32 Height = 31;

	// Ghost indices, also used by the HUD to pick colors.
	static constexpr int32 Blinky = 0;
	static constexpr int32 Pinky = 1;
	static constexpr int32 Inky = 2;
	static constexpr int32 Clyde = 3;

	void NewGame();
	void Tick(float DeltaSeconds);

	// Remembers the direction the player wants; Pac-Man turns as soon as the maze allows it.
	void QueueDirection(EPacDir Dir) { QueuedDir = Dir; }

	// Multiplies how fast the ghosts roam (not the eyes returning home). 1 = arcade speed.
	void SetGhostSpeedScale(float Scale) { GhostSpeedScale = Scale; }

	// How many ghosts and power pellets each level has. Takes effect from the next level or new game.
	static constexpr int32 MinGhosts = 1;
	static constexpr int32 MaxGhosts = 8;
	static constexpr int32 MaxPowerPellets = 8;
	void SetGhostCount(int32 Count) { GhostCount = FMath::Clamp(Count, MinGhosts, MaxGhosts); }
	void SetPowerPelletCount(int32 Count) { PowerPelletCount = FMath::Clamp(Count, 0, MaxPowerPellets); }

	static EPacPersonality GetPersonality(int32 GhostIndex) { return static_cast<EPacPersonality>(GhostIndex % 8); }
	static const TCHAR* GetPersonalityName(int32 GhostIndex);
	// How much faster (above 1) or slower than normal this octopus is moving right now because of its personality.
	float GetPersonalitySpeed(int32 GhostIndex) const;

	EPacTile GetTile(int32 X, int32 Y) const;
	bool IsWall(int32 X, int32 Y) const { return GetTile(X, Y) == EPacTile::Wall; }

	EPacGamePhase GetPhase() const { return Phase; }
	float GetPhaseTimer() const { return PhaseTimer; }
	FVector2D GetPacPos() const { return PacPos; }
	EPacDir GetPacFacing() const { return PacFacing; }
	const TArray<FPacGhost>& GetGhosts() const { return Ghosts; }
	const TArray<FPacPopup>& GetPopups() const { return Popups; }
	float GetFrightTimeLeft() const { return FrightTimer; }
	bool IsFrozen() const { return FreezeTimer > 0.f; }
	// Seconds of invincibility left after being caught; the shark flashes and octopi pass through it.
	float GetInvincibleTimeLeft() const { return InvincibleTimer; }
	int32 GetScore() const { return Score; }
	int32 GetHighScore() const { return HighScore; }
	int32 GetLives() const { return Lives; }
	int32 GetLevel() const { return Level; }
	int32 GetPelletsLeft() const { return PelletsLeft; }
	// Changes whenever a new maze is built, so the HUD knows when to rebuild its cached wall drawing.
	int32 GetMazeId() const { return MazeId; }
	void SetHighScore(int32 InHighScore) { HighScore = FMath::Max(HighScore, InHighScore); }
	// Forgets the saved best; a game in progress keeps its own score as the best so far.
	void ClearHighScore(bool bKeepCurrentScore) { HighScore = bKeepCurrentScore ? Score : 0; }

	// Returns and clears the events since the last call.
	TArray<EPacEvent> TakeEvents()
	{
		TArray<EPacEvent> Out = MoveTemp(Events);
		Events.Reset();
		return Out;
	}

	static constexpr float ReadyDuration = 2.f;
	static constexpr float DyingDuration = 3.f;	// long enough for the dead shark to float up to the ceiling
	static constexpr float LevelClearDuration = 2.f;
	static constexpr float InvincibleDuration = 3.f;

private:
	void StartLevel();
	void ResetActors();
	void TickPlaying(float Dt);
	void MovePac(float Dt);
	void MoveGhost(int32 GhostIndex, float Dt);
	void EatAtPac();
	void CheckCollisions();
	void StartFright();
	void AddScore(int32 Points);
	void ReverseActiveGhosts();

	EPacDir ChooseGhostDir(int32 GhostIndex, const FIntPoint& Tile) const;
	FIntPoint GetGhostTarget(int32 GhostIndex) const;
	EPacDir ChooseEyesDir(const FPacGhost& Ghost, const FIntPoint& Tile) const;
	bool CanMove(const FIntPoint& Tile, EPacDir Dir) const;
	float GetLevelSpeedScale() const;

	// Moves along the maze by Distance tiles. Decide runs at every tile center reached.
	void Advance(FVector2D& Pos, EPacDir& Dir, float Distance, TFunctionRef<void(const FIntPoint&, EPacDir&)> Decide) const;

	// Fills HomeDistance: steps from every open tile to the tile above the den door.
	void BuildHomeDistances();

	TArray<EPacTile> Tiles;
	// Shortest walking distance from each tile to the den exit, so eaten eyes always take a real route
	// home instead of aiming in a straight line (which can circle forever in some mazes). -1 = unreachable.
	TArray<int32> HomeDistance;
	int32 MazeId = 0;
	int32 PelletsLeft = 0;
	int32 LevelPelletTotal = 1;

	FVector2D PacPos = FVector2D::ZeroVector;
	EPacDir PacDir = EPacDir::None;
	EPacDir PacFacing = EPacDir::Left;
	EPacDir QueuedDir = EPacDir::None;

	TArray<FPacGhost> Ghosts;
	TArray<FPacPopup> Popups;
	TArray<EPacEvent> Events;

	EPacGamePhase Phase = EPacGamePhase::GameOver;
	float PhaseTimer = 0.f;
	float FrightTimer = 0.f;
	float FreezeTimer = 0.f;
	float InvincibleTimer = 0.f;
	int32 GhostCombo = 0;
	int32 ModeIndex = 0;
	float ModeTimer = 0.f;

	int32 Score = 0;
	int32 HighScore = 0;
	int32 Lives = 0;
	int32 Level = 1;
	bool bExtraLifeAwarded = false;
	float GhostSpeedScale = 1.f;
	int32 GhostCount = 4;
	int32 PowerPelletCount = 4;
};
