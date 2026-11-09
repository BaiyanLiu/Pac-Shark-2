// Pac Shark - draws the whole game on the HUD canvas: sprite art loaded from the project's Art folder,
// with simple shape drawing as a fallback if a sprite file is missing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Engine/Canvas.h"
#include "PacGame.h"
#include "PacSharkHUD.generated.h"

class APacSharkGameMode;
class UFont;
class UTexture2D;

// Something on screen that can be tapped. The HUD records these while drawing; the game mode looks up taps in them.
enum class EPacTouch : uint8
{
	None,
	MenuItem,		// Index: title screen item
	PauseItem,		// Index: pause menu item
	PauseButton,
	SettingsItem,	// Index: settings row (selects it; BACK closes)
	SettingsPrev,	// Index: settings row to step down
	SettingsNext,	// Index: settings row to step up
	SettingsTrack,	// Index: slider row; the box spans the track exactly
	NameUp,			// Index: initial to change
	NameDown,
	NameBox,
	NameDone
};

struct FPacTouchZone
{
	FBox2D Box = FBox2D(ForceInit);
	EPacTouch Action = EPacTouch::None;
	int32 Index = 0;
};

UCLASS()
class PACSHARK_API APacSharkHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	// The topmost tappable thing under Point from the last frame, or null.
	const FPacTouchZone* FindTouchZone(const FVector2D& Point) const;
	const FPacTouchZone* FindTouchZone(EPacTouch Action, int32 Index) const;

	// Size of the canvas the last frame was drawn on. Touch zones are in this space, which on phones can
	// differ from the touch input's pixel space (the game may render below the screen's resolution).
	FVector2D GetCanvasSize() const { return CanvasSize; }

	// Center of a maze tile on the canvas, as of the last frame drawn.
	FVector2D TileToScreen(const FVector2D& TilePos) const;

private:
	void DrawCover(UTexture2D* Texture, float ScreenW, float ScreenH, const FLinearColor& Tint = FLinearColor::White);
	void UpdateTheme(int32 Level);
	void DrawSprite(UTexture2D* Texture, int32 Cols, int32 Rows, int32 Col, int32 Row, const FVector2D& Center,
		const FVector2D& Size, const FLinearColor& Tint, float RotationDegrees = 0.f, bool bMirror = false);
	// Like DrawSprite, but the lower part of the cell is drawn in thin rows that sway side to side, so
	// tentacles ripple. WaveStart is where the sway begins (0 = top of the sprite, 1 = bottom).
	void DrawWavySprite(UTexture2D* Texture, int32 Cols, int32 Rows, int32 Col, int32 Row, const FVector2D& Center,
		const FVector2D& Size, const FLinearColor& Tint, float WaveStart, float Amplitude, float WavePhase);
	// How an edible octopus looks right now: whether it shows white, and a shiver offset as it is about to turn back.
	bool GetFrightFlash(const FPacGame& Game, int32 GhostIndex, float& OutShiver) const;
	FVector2D SpriteCellSize(UTexture2D* Texture, int32 Cols, int32 Rows) const;

	void DrawMainMenu(const APacSharkGameMode& GameMode, float ScreenW, float ScreenH);
	void DrawOcean(float ScreenW, float ScreenH);
	void DrawMaze(const FPacGame& Game);
	void BuildWallTris(const FPacGame& Game);
	// Draws a whole triangle list as one canvas item (translucent; white texture when Texture is null).
	void DrawTris(TArray<FCanvasUVTri>& Tris, UTexture2D* Texture);
	void DrawPlayer(const FPacGame& Game);
	void StartBigBiteEffects(const FVector2D& TilePos, bool bOctopus);
	void DrawBiteBubbles();
	void UpdateTrails(const FPacGame& Game);
	void DrawTrailBubbles();
	void DrawOctopusLegend(const FPacGame& Game, float ScreenW, float TextScale);
	// Swing bends the back of a swim-strip sprite side to side (fraction of the sprite's height; 0 = still).
	void DrawSharkSprite(const FVector2D& Center, float Width, float Fatness, int32 Frame, int32 Row, float RotationDegrees, bool bMirror, float Alpha,
		float Swing = 0.f);
	float DeathFloatRise(const FPacGame& Game, float Elapsed) const;
	void DrawShark(const FVector2D& Center, float Size, EPacDir Facing, float MouthDegrees, float Fatness, bool bBellyUp);
	void DrawOctopus(const FPacGame& Game, int32 GhostIndex);
	void DrawOctopusEyes(const FVector2D& SpriteCenter, const FVector2D& SpriteSize, const FVector2D& Look, float Scale);
	void DrawSettings(const APacSharkGameMode& GameMode, float ScreenW, float ScreenH, float TextScale);
	void AddTouchZone(float X, float Y, float W, float H, EPacTouch Action, int32 Index = 0);
	void AddTextTouchZone(const FString& Text, float CenterX, float TopY, float Scale, EPacTouch Action, int32 Index = 0);
	void DrawPauseButton(float ScreenW);
	void DrawArrow(const FVector2D& Center, float Size, bool bUp, const FLinearColor& Color);
	void DrawPanel(float ScreenW, float ScreenH, float PanelW, float PanelH, float& OutX, float& OutY);
	void DrawPauseMenu(const APacSharkGameMode& GameMode, float ScreenW, float ScreenH, float TextScale);
	void DrawNameEntry(const APacSharkGameMode& GameMode, float ScreenW, float ScreenH, float TextScale);
	void DrawHighScores(const APacSharkGameMode& GameMode, float CenterX, float TopY, float Scale, float LineH, int32 MaxRows);
	static UTexture2D* LoadArtTexture(const FString& Path);
	// Scale for the crisp font that gives the same line height as the medium font at Scale.
	float CrispFontScale(float Scale) const;
	void TextSize(const FString& Text, float& OutW, float& OutH, float Scale) const;
	void DrawCrispText(const FString& Text, const FLinearColor& Color, float X, float TopY, float Scale);
	void DrawCircle(const FVector2D& Center, float Radius, const FLinearColor& Color, int32 Sides = 16);
	void DrawTextCentered(const FString& Text, const FLinearColor& Color, float CenterX, float TopY, float Scale);
	void DrawTextLeft(const FString& Text, const FLinearColor& Color, float X, float TopY, float Scale);
	void DrawTextShadowed(const FString& Text, const FLinearColor& Color, float CenterX, float TopY, float Scale);

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> SharkTexture;		// 5 x 3: swim right, swim left, death

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> OctopusTexture;		// 4 x 2: normal, edible

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DotTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> GameBackgroundTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MainMenuTexture;

	TArray<FPacTouchZone> TouchZones;
	// Reused each frame for batched drawing, so the array keeps its memory.
	TArray<FCanvasUVTri> BatchTris;
	FVector2D CanvasSize = FVector2D::ZeroVector;

	float TileSize = 16.f;
	// Pixel height of a line of text at Scale 1 (the engine's medium font), used to size the real fonts.
	float TextUnitHeight = 16.f;
	// The engine's distance field font stays sharp at any scale. Its line height at Scale 1 is measured once.
	UPROPERTY(Transient)
	TObjectPtr<UFont> CrispFont;
	float CrispFontUnitHeight = 0.f;
	FVector2D Origin = FVector2D::ZeroVector;
	float TimeSeconds = 0.f;

	// 0 = small shark, 1 = big fat shark; eased toward the target every frame.
	float SharkFatness = 0.f;
	// Which swim strip is showing, and the tilt used for up/down so turns rotate smoothly.
	bool bSharkFacesLeft = true;
	float SharkAngle = 0.f;

	// Tail beat: the phase runs faster while swimming, and the swing eases between a lazy idle sway and a full stroke.
	float TailPhase = 0.f;
	float TailSwing = 0.f;
	FVector2D LastTailPacPos = FVector2D::ZeroVector;

	// Jaws: 0 = shut, 1 = wide open. They open as a dot comes up ahead and snap shut on each bite.
	float SharkMouthOpen = 0.f;
	float ChompTimeLeft = 0.f;
	float BitePulse = 0.f;		// brief swell on a big bite (power dot or octopus)
	int32 LastPelletsLeft = -1;
	float LastFrightLeft = 0.f;
	bool bWasFrozen = false;

	// Big bites release a burst of bubbles.
	struct FBiteBubble
	{
		FVector2D Pos;		// in tiles
		FVector2D Velocity;	// tiles per second
		float Radius = 0.2f;	// in tiles
		float Life = 0.f;
		float MaxLife = 1.f;
		float Wobble = 0.f;
	};
	TArray<FBiteBubble> BiteBubbles;

	// Faint wakes: behind the shark as it swims, and behind a dashing Sprinter octopus.
	TArray<FBiteBubble> TrailBubbles;
	FVector2D LastTrailPacPos = FVector2D(-100.f, -100.f);
	float SharkTrailDistance = 0.f;
	float OctopusTrailTime = 0.f;

	// Level theme colors, eased toward the current theme so a new area fades in.
	FLinearColor ThemeTint = FLinearColor::White;
	FLinearColor ThemeOverlay = FLinearColor::Transparent;
	FLinearColor ThemeSand = FLinearColor::White;
	FLinearColor ThemeSandEdge = FLinearColor::White;
	FLinearColor ThemeSandGrain = FLinearColor::White;
	FLinearColor ThemeBubble = FLinearColor::White;
	bool bThemeInitialized = false;
	int32 BannerLevel = 0;
	float BannerTimeLeft = 0.f;

	// Sand-bank triangles for the current maze, built once per maze (and screen size) and recolored only
	// when the theme colors change, instead of rebuilding thousands of triangles every frame.
	TArray<FCanvasUVTri> WallTris;
	TArray<bool> WallTriIsFill;
	TArray<FCanvasUVTri> GrainTris;
	int32 WallMazeId = -1;
	float WallTileSize = 0.f;
	FVector2D WallOrigin = FVector2D::ZeroVector;
	FLinearColor WallEdgeColor = FLinearColor::Transparent;
	FLinearColor WallFillColor = FLinearColor::Transparent;
	FLinearColor WallGrainColor = FLinearColor::Transparent;
	bool bWallColorsValid = false;

	// Octopi are drawn slightly behind their simulated positions so corners look rounded.
	TArray<FVector2D> SmoothedOctopusPos;
	// Where each octopus's pupils point, eased toward its heading so glances turn smoothly.
	TArray<FVector2D> SmoothedOctopusLook;
};
