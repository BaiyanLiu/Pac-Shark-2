// Pac Shark - draws the whole game on the HUD canvas.
// Theme: a shark eats pearls among sand banks while flapjack octopi hunt it. Sprite art is loaded at
// runtime from <Project>/Art; if a file is missing, simple shapes are drawn in its place.

#include "PacSharkHUD.h"
#include "PacSharkGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "GlobalRenderResources.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"

namespace PacHUD
{
	const FLinearColor WaterTop(0.02f, 0.28f, 0.42f);
	const FLinearColor WaterBottom(0.0f, 0.04f, 0.12f);
	const FLinearColor BubbleColor(0.25f, 0.55f, 0.7f);
	const FLinearColor Coral(1.f, 0.45f, 0.38f);
	const FLinearColor Rock(0.03f, 0.09f, 0.14f);
	const FLinearColor Kelp(0.3f, 0.85f, 0.4f);
	const FLinearColor Plankton(0.6f, 0.95f, 1.f);
	const FLinearColor Pearl(0.97f, 0.95f, 0.9f);
	const FLinearColor SharkBody(0.45f, 0.55f, 0.66f);
	const FLinearColor SharkFin(0.36f, 0.45f, 0.56f);
	const FLinearColor SharkBelly(0.9f, 0.93f, 0.96f);
	const FLinearColor SharkEye(0.02f, 0.02f, 0.05f);
	const FLinearColor White(1.f, 1.f, 1.f);
	const FLinearColor Seafoam(0.55f, 1.f, 0.9f);
	const FLinearColor Sunny(1.f, 0.85f, 0.3f);
	const FLinearColor TextRed(1.f, 0.25f, 0.2f);
	const FLinearColor FrightBlue(0.2f, 0.2f, 1.f);
	const FLinearColor PupilBlue(0.13f, 0.2f, 0.85f);
	const FLinearColor OctopusColors[] = {
		FLinearColor(1.f, 0.25f, 0.15f),	// Blinky
		FLinearColor(1.f, 0.6f, 0.85f),		// Pinky
		FLinearColor(0.3f, 0.9f, 1.f),		// Inky
		FLinearColor(1.f, 0.65f, 0.25f),	// Clyde
		FLinearColor(0.45f, 1.f, 0.4f),		// extra octopi from the settings
		FLinearColor(0.7f, 0.45f, 1.f),
		FLinearColor(1.f, 1.f, 0.35f),
		FLinearColor(0.95f, 0.95f, 0.95f),
	};
	static_assert(UE_ARRAY_COUNT(OctopusColors) == FPacGame::MaxGhosts, "one color per possible octopus");

	// A new look every few levels: water tint, an overlay wash, sand colors and bubbles.
	struct FLevelTheme
	{
		const TCHAR* Name;
		FLinearColor Tint;
		FLinearColor Overlay;
		FLinearColor Sand;
		FLinearColor SandEdge;
		FLinearColor SandGrain;
		FLinearColor Bubble;
	};
	constexpr int32 LevelsPerTheme = 2;
	const FLevelTheme Themes[] = {
		{ TEXT("SHALLOW LAGOON"), FLinearColor(1.f, 1.f, 1.f), FLinearColor(0.f, 0.f, 0.f, 0.f),
			FLinearColor(0.93f, 0.82f, 0.58f), FLinearColor(0.72f, 0.58f, 0.34f), FLinearColor(0.8f, 0.68f, 0.45f), FLinearColor(0.25f, 0.55f, 0.7f) },
		{ TEXT("KELP FOREST"), FLinearColor(0.75f, 1.f, 0.75f), FLinearColor(0.05f, 0.35f, 0.12f, 0.2f),
			FLinearColor(0.82f, 0.84f, 0.55f), FLinearColor(0.5f, 0.6f, 0.3f), FLinearColor(0.65f, 0.72f, 0.4f), FLinearColor(0.35f, 0.7f, 0.45f) },
		{ TEXT("SUNSET REEF"), FLinearColor(1.f, 0.8f, 0.7f), FLinearColor(0.6f, 0.2f, 0.25f, 0.18f),
			FLinearColor(0.98f, 0.75f, 0.6f), FLinearColor(0.8f, 0.45f, 0.38f), FLinearColor(0.9f, 0.6f, 0.5f), FLinearColor(0.85f, 0.55f, 0.6f) },
		{ TEXT("TWILIGHT ZONE"), FLinearColor(0.7f, 0.6f, 0.95f), FLinearColor(0.15f, 0.05f, 0.3f, 0.3f),
			FLinearColor(0.75f, 0.68f, 0.85f), FLinearColor(0.48f, 0.38f, 0.62f), FLinearColor(0.62f, 0.55f, 0.75f), FLinearColor(0.55f, 0.45f, 0.85f) },
		{ TEXT("MIDNIGHT TRENCH"), FLinearColor(0.45f, 0.5f, 0.7f), FLinearColor(0.f, 0.02f, 0.1f, 0.4f),
			FLinearColor(0.55f, 0.6f, 0.7f), FLinearColor(0.3f, 0.36f, 0.48f), FLinearColor(0.45f, 0.5f, 0.6f), FLinearColor(0.2f, 0.4f, 0.6f) },
		{ TEXT("FROZEN SEA"), FLinearColor(0.85f, 0.95f, 1.f), FLinearColor(0.7f, 0.9f, 1.f, 0.14f),
			FLinearColor(0.9f, 0.96f, 1.f), FLinearColor(0.55f, 0.7f, 0.85f), FLinearColor(0.75f, 0.85f, 0.95f), FLinearColor(0.7f, 0.9f, 1.f) },
	};
	constexpr int32 NumThemes = UE_ARRAY_COUNT(Themes);

	// Levels 1-2 are the first theme, 3-4 the second, and so on, looping after the last.
	const FLevelTheme& ThemeForLevel(int32 Level)
	{
		return Themes[(FMath::Max(1, Level) - 1) / LevelsPerTheme % NumThemes];
	}

	// Rows reserved above and below the maze for score and lives.
	constexpr int32 TopRows = 3;
	constexpr int32 BottomRows = 2;
	constexpr int32 NumBubbles = 40;

	// Length of the normal shark sprite in tiles. The stuffed shark grows to FatSharkWidthTiles long, and its
	// height is FatSharkHeightTiles times the cell's height-to-width ratio.
	constexpr float SharkWidthTiles = 2.9f;
	constexpr float FatSharkWidthTiles = 3.05f;
	constexpr float FatSharkHeightTiles = 3.8f;
	constexpr int32 SharkCols = 5;
	constexpr int32 SharkRows = 3;
	constexpr int32 SharkRowRight = 0;
	constexpr int32 SharkRowLeft = 1;
	constexpr int32 SharkRowDeath = 2;
	// Width of an octopus sprite in tiles (its height follows the art).
	constexpr float OctopusWidthTiles = 1.5f;
	constexpr int32 OctopusCols = 4;
	constexpr int32 OctopusRows = 2;
	constexpr int32 OctopusRowEdible = 1;

	// Swim frames play forward then backward so the chomp loops without a jump.
	int32 PingPongFrame(float Phase, int32 NumFrames)
	{
		const int32 Cycle = 2 * NumFrames - 2;
		const int32 Step = ((FMath::FloorToInt(Phase) % Cycle) + Cycle) % Cycle;
		return Step < NumFrames ? Step : Cycle - Step;
	}

	FVector2D FacingToVector(EPacDir Dir)
	{
		switch (Dir)
		{
		case EPacDir::Up: return FVector2D(0.f, -1.f);
		case EPacDir::Down: return FVector2D(0.f, 1.f);
		case EPacDir::Left: return FVector2D(-1.f, 0.f);
		case EPacDir::Right: return FVector2D(1.f, 0.f);
		default: return FVector2D::ZeroVector;
		}
	}

	// Stable pseudo-random value in [0, 1) so bubbles keep their lanes from frame to frame.
	float Hash(float Seed)
	{
		return FMath::Frac(FMath::Sin(Seed * 12.9898f) * 43758.5453f);
	}

	// Local drawing space for the shark: X points forward along its heading, Y points toward its belly.
	struct FBodyFrame
	{
		FVector2D Center;
		FVector2D Forward;
		FVector2D Side;
		float Scale;

		FVector2D At(float X, float Y) const
		{
			return Center + Forward * (X * Scale) + Side * (Y * Scale);
		}
	};

	void AddTri(TArray<FCanvasUVTri>& Tris, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color)
	{
		FCanvasUVTri Tri;
		Tri.V0_Pos = A;
		Tri.V1_Pos = B;
		Tri.V2_Pos = C;
		Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = Color;
		Tris.Add(Tri);
	}

	// An axis-aligned rectangle as two triangles, with optional texture coordinates.
	void AddQuad(TArray<FCanvasUVTri>& Tris, float X, float Y, float W, float H, const FLinearColor& Color,
		const FVector2D& UV0 = FVector2D(0.f, 0.f), const FVector2D& UV1 = FVector2D(1.f, 1.f))
	{
		FCanvasUVTri A;
		A.V0_Pos = FVector2D(X, Y);			A.V0_UV = UV0;
		A.V1_Pos = FVector2D(X + W, Y);		A.V1_UV = FVector2D(UV1.X, UV0.Y);
		A.V2_Pos = FVector2D(X, Y + H);		A.V2_UV = FVector2D(UV0.X, UV1.Y);
		A.V0_Color = A.V1_Color = A.V2_Color = Color;
		FCanvasUVTri B = A;
		B.V0_Pos = FVector2D(X + W, Y);		B.V0_UV = FVector2D(UV1.X, UV0.Y);
		B.V1_Pos = FVector2D(X + W, Y + H);	B.V1_UV = UV1;
		B.V2_Pos = FVector2D(X, Y + H);		B.V2_UV = FVector2D(UV0.X, UV1.Y);
		Tris.Add(A);
		Tris.Add(B);
	}

	void SetTriColor(FCanvasUVTri& Tri, const FLinearColor& Color)
	{
		Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = Color;
	}

	// Triangle fan over an ellipse arc, angles in degrees measured in the body frame.
	void AddEllipseFan(TArray<FCanvasUVTri>& Tris, const FBodyFrame& Frame, float CX, float CY, float RX, float RY,
		float StartDeg, float EndDeg, int32 Segments, const FLinearColor& Color)
	{
		const FVector2D Hub = Frame.At(CX, CY);
		for (int32 i = 0; i < Segments; ++i)
		{
			const float A0 = FMath::DegreesToRadians(FMath::Lerp(StartDeg, EndDeg, float(i) / Segments));
			const float A1 = FMath::DegreesToRadians(FMath::Lerp(StartDeg, EndDeg, float(i + 1) / Segments));
			AddTri(Tris, Hub,
				Frame.At(CX + RX * FMath::Cos(A0), CY + RY * FMath::Sin(A0)),
				Frame.At(CX + RX * FMath::Cos(A1), CY + RY * FMath::Sin(A1)), Color);
		}
	}
}

void APacSharkHUD::BeginPlay()
{
	Super::BeginPlay();

	auto Load = [](const TCHAR* FileName) -> UTexture2D*
	{
		const FString Path = APacSharkGameMode::GetArtPath(FileName);
		if (!FPaths::FileExists(Path))
		{
			UE_LOG(LogTemp, Warning, TEXT("Pac Shark: sprite file not found: %s"), *Path);
			return nullptr;
		}
		return LoadArtTexture(Path);
	};

	SharkTexture = Load(TEXT("Shark.png"));
	OctopusTexture = Load(TEXT("Octopus.png"));
	DotTexture = Load(TEXT("Dot.png"));
	GameBackgroundTexture = Load(TEXT("GameBackground.png"));
	MainMenuTexture = Load(TEXT("MainMenu.png"));

	CrispFont = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/RobotoDistanceField.RobotoDistanceField"));
	if (!CrispFont)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pac Shark: distance field font not found, using the medium font"));
	}
}

void APacSharkHUD::DrawHUD()
{
	Super::DrawHUD();

	const APacSharkGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APacSharkGameMode>() : nullptr;
	if (!GameMode || !Canvas)
	{
		return;
	}
	const FPacGame& Game = GameMode->GetGame();
	TimeSeconds = static_cast<float>(GetWorld()->GetRealTimeSeconds());
	TouchZones.Reset();

	const float ScreenW = Canvas->ClipX;
	const float ScreenH = Canvas->ClipY;
	CanvasSize = FVector2D(ScreenW, ScreenH);
	const int32 TotalRows = FPacGame::Height + PacHUD::TopRows + PacHUD::BottomRows;
	TileSize = FMath::Max(2.f, FMath::FloorToFloat(FMath::Min(ScreenW / FPacGame::Width, ScreenH / TotalRows)));
	Origin.X = FMath::FloorToFloat((ScreenW - FPacGame::Width * TileSize) * 0.5f);
	Origin.Y = FMath::FloorToFloat((ScreenH - TotalRows * TileSize) * 0.5f) + PacHUD::TopRows * TileSize;

	// Scale the engine font so a line of text is about one tile tall.
	float RefW = 0.f, RefH = 0.f;
	GetTextSize(TEXT("SCORE"), RefW, RefH, GEngine->GetMediumFont(), 1.f);
	const float TextScale = RefH > 0.f ? TileSize / RefH : 1.f;
	TextUnitHeight = RefH > 0.f ? RefH : 16.f;
	if (CrispFont && CrispFontUnitHeight <= 0.f)
	{
		// Measure the crisp font once so text can be sized to the same line height.
		float CrispW = 0.f;
		GetTextSize(TEXT("SCORE"), CrispW, CrispFontUnitHeight, CrispFont, 1.f);
	}

	if (GameMode->IsInIntro())
	{
		// Letterbox the video on black, keeping its shape.
		DrawRect(FLinearColor::Black, 0.f, 0.f, ScreenW, ScreenH);
		if (UTexture* Video = GameMode->GetIntroTexture())
		{
			const float VideoAspect = 16.f / 9.f;
			const float W = FMath::Min(ScreenW, ScreenH * VideoAspect);
			const FVector2D Size(W, W / VideoAspect);
			Canvas->K2_DrawTexture(Video, FVector2D(ScreenW, ScreenH) * 0.5f - Size * 0.5f, Size, FVector2D::ZeroVector, FVector2D::UnitVector, PacHUD::White, BLEND_Opaque);
		}
		if (GameMode->GetIntroElapsed() > 1.f)
		{
			const TCHAR* SkipText = GameMode->IsUsingTouch() ? TEXT("TAP TO SKIP") : GameMode->IsUsingGamepad() ? TEXT("A: SKIP") : TEXT("ENTER: SKIP");
			float SkipW = 0.f, SkipH = 0.f;
			TextSize(SkipText, SkipW, SkipH, TextScale * 0.7f);
			DrawTextLeft(SkipText, PacHUD::White, ScreenW - SkipW - TileSize, ScreenH - SkipH - TileSize, TextScale * 0.7f);
		}
		return;
	}

	if (GameMode->IsInMainMenu())
	{
		DrawMainMenu(*GameMode, ScreenW, ScreenH);
		if (GameMode->IsSettingsOpen())
		{
			DrawSettings(*GameMode, ScreenW, ScreenH, TextScale);
		}
		return;
	}

	UpdateTheme(Game.GetLevel());
	DrawOcean(ScreenW, ScreenH);
	DrawMaze(Game);

	const EPacGamePhase Phase = Game.GetPhase();
	const bool bShowOctopi = Phase == EPacGamePhase::Ready || Phase == EPacGamePhase::Playing
		|| (Phase == EPacGamePhase::Dying && Game.GetPhaseTimer() > FPacGame::DyingDuration - 0.4f);
	UpdateTrails(Game);
	DrawTrailBubbles();
	if (bShowOctopi)
	{
		for (int32 i = 0; i < Game.GetGhosts().Num(); ++i)
		{
			DrawOctopus(Game, i);
		}
	}
	if (Phase != EPacGamePhase::GameOver)
	{
		DrawPlayer(Game);
		DrawBiteBubbles();
	}

	for (const FPacPopup& Popup : Game.GetPopups())
	{
		const FVector2D At = TileToScreen(Popup.Pos);
		DrawTextCentered(FString::FromInt(Popup.Points), PacHUD::Seafoam, At.X, At.Y - TileSize * 0.5f, TextScale * 0.8f);
	}

	DrawOctopusLegend(Game, ScreenW, TextScale);

	// Score header.
	const float HeaderY = Origin.Y - PacHUD::TopRows * TileSize;
	const float MidX = Origin.X + FPacGame::Width * TileSize * 0.5f;
	DrawTextLeft(TEXT("SCORE"), PacHUD::Seafoam, Origin.X + TileSize, HeaderY, TextScale);
	DrawTextLeft(FString::FromInt(Game.GetScore()), PacHUD::White, Origin.X + TileSize, HeaderY + TileSize * 1.2f, TextScale);
	DrawTextCentered(TEXT("PAC SHARK"), PacHUD::Sunny, MidX, HeaderY, TextScale);
	const FString HighText = FString::Printf(TEXT("HIGH %d"), Game.GetHighScore());
	float HighW = 0.f, HighH = 0.f;
	TextSize(HighText, HighW, HighH, TextScale);
	DrawTextLeft(HighText, PacHUD::Seafoam, Origin.X + FPacGame::Width * TileSize - TileSize - HighW, HeaderY, TextScale);

	// Spare lives and level along the bottom.
	const float FooterY = Origin.Y + FPacGame::Height * TileSize + TileSize;
	const int32 SpareLives = Game.GetLives() - (Phase == EPacGamePhase::GameOver ? 0 : 1);
	for (int32 i = 0; i < SpareLives; ++i)
	{
		const FVector2D IconPos(Origin.X + TileSize * (2.f + i * 2.5f), FooterY);
		if (SharkTexture)
		{
			DrawSharkSprite(IconPos, TileSize * 2.f, 0.f, 0, PacHUD::SharkRowRight, 0.f, false, 1.f);
		}
		else
		{
			DrawShark(IconPos, TileSize * 0.6f, EPacDir::Right, 20.f, 0.f, false);
		}
	}
	const FString LevelText = FString::Printf(TEXT("LEVEL %d"), Game.GetLevel());
	float LevelW = 0.f, LevelH = 0.f;
	TextSize(LevelText, LevelW, LevelH, TextScale);
	DrawTextLeft(LevelText, PacHUD::Seafoam, Origin.X + FPacGame::Width * TileSize - TileSize - LevelW, FooterY - LevelH * 0.5f, TextScale);

	// Messages sit in the empty row under the octopus den, like the arcade.
	const float MessageY = Origin.Y + 17.f * TileSize;
	if (Phase == EPacGamePhase::Ready)
	{
		DrawTextShadowed(TEXT("READY!"), PacHUD::Sunny, MidX, MessageY, TextScale);
	}

	// Name the new area when a level starts a new theme (and at the start of every game).
	if (Phase == EPacGamePhase::GameOver)
	{
		BannerLevel = 0;
	}
	else if (Phase == EPacGamePhase::Ready && Game.GetLevel() != BannerLevel)
	{
		BannerLevel = Game.GetLevel();
		BannerTimeLeft = (BannerLevel - 1) % PacHUD::LevelsPerTheme == 0 ? 2.5f : 0.f;
	}
	if (BannerTimeLeft > 0.f)
	{
		BannerTimeLeft -= RenderDelta;
		const float Alpha = FMath::Clamp(BannerTimeLeft / 0.5f, 0.f, 1.f);
		const FLinearColor Color(PacHUD::Seafoam.R, PacHUD::Seafoam.G, PacHUD::Seafoam.B, Alpha);
		DrawTextShadowed(PacHUD::ThemeForLevel(BannerLevel).Name, Color, MidX, Origin.Y + 20.f * TileSize, TextScale * 0.9f);
	}
	else if (Phase == EPacGamePhase::GameOver)
	{
		DrawTextShadowed(TEXT("GAME  OVER"), PacHUD::TextRed, MidX, MessageY, TextScale);
		if (!GameMode->IsEnteringName())
		{
			DrawTextShadowed(GameMode->IsUsingTouch() ? TEXT("TAP TO SWIM AGAIN") : GameMode->IsUsingGamepad() ? TEXT("A: SWIM AGAIN    START: MENU") : TEXT("ENTER: SWIM AGAIN    P: MENU"),
				PacHUD::White, MidX, Origin.Y + 20.f * TileSize, TextScale * 0.8f);
		}
	}

	if (GameMode->IsEnteringName())
	{
		DrawNameEntry(*GameMode, ScreenW, ScreenH, TextScale);
	}
	else if (GameMode->IsSettingsOpen())
	{
		DrawSettings(*GameMode, ScreenW, ScreenH, TextScale);
	}
	else if (GameMode->IsPaused())
	{
		DrawPauseMenu(*GameMode, ScreenW, ScreenH, TextScale);
	}
	else
	{
		DrawTextCentered(GameMode->IsUsingTouch() ? TEXT("SWIPE TO SWIM") : GameMode->IsUsingGamepad() ? TEXT("START: PAUSE   BACK: SETTINGS") : TEXT("P: PAUSE   TAB: SETTINGS"),
			PacHUD::Seafoam, MidX, FooterY - TileSize * 0.35f, TextScale * 0.7f);
		if (GameMode->IsUsingTouch())
		{
			DrawPauseButton(ScreenW);
		}
	}
}

FVector2D APacSharkHUD::TileToScreen(const FVector2D& TilePos) const
{
	return Origin + (TilePos + FVector2D(0.5f, 0.5f)) * TileSize;
}

bool APacSharkHUD::GetFrightFlash(const FPacGame& Game, int32 GhostIndex, float& OutShiver) const
{
	// Warn for the last 3 seconds: flashing white, faster and faster, and a shiver in the final second.
	constexpr float WarnTime = 3.f;
	const float Left = Game.GetFrightTimeLeft();
	OutShiver = 0.f;
	if (Left >= WarnTime)
	{
		return false;
	}
	const float Urgency = 1.f - Left / WarnTime;	// 0 when the warning starts, 1 as it turns back
	const float FlashRate = FMath::Lerp(2.f, 9.f, Urgency * Urgency);
	if (Left < 1.f)
	{
		OutShiver = FMath::Sin(TimeSeconds * 60.f + GhostIndex * 2.f) * TileSize * 0.07f * (1.f - Left);
	}
	// Integrate the rising rate so the flashes speed up smoothly instead of stuttering.
	const float FlashPhase = (WarnTime - Left) * (2.f + (FlashRate - 2.f) / 3.f);
	return FMath::Frac(FlashPhase) < 0.5f;
}

void APacSharkHUD::DrawOctopusEyes(const FVector2D& SpriteCenter, const FVector2D& SpriteSize, const FVector2D& Look, float Scale)
{
	// Eye holes in the octopus sprite, as fractions of the cell (measured from Octopus.png).
	const FVector2D EyeSpots[] = { FVector2D(0.392f, 0.291f), FVector2D(0.616f, 0.294f) };
	const FVector2D TopLeft = SpriteCenter - SpriteSize * 0.5f;
	const float White = SpriteSize.X * 0.058f * Scale;
	const float Pupil = White * 0.55f;
	for (const FVector2D& Spot : EyeSpots)
	{
		// Scaled-up eyes (eyes only) keep their spacing around the sprite's middle.
		const FVector2D Base = TopLeft + Spot * SpriteSize;
		const FVector2D Eye = SpriteCenter + (Base - SpriteCenter) * FVector2D(Scale, 1.f);
		DrawCircle(Eye, White, PacHUD::White, 14);
		DrawCircle(Eye + Look * (White - Pupil) * 0.9f, Pupil, PacHUD::PupilBlue, 10);
	}
}

void APacSharkHUD::DrawWavySprite(UTexture2D* Texture, int32 Cols, int32 Rows, int32 Col, int32 Row, const FVector2D& Center,
	const FVector2D& Size, const FLinearColor& Tint, float WaveStart, float Amplitude, float WavePhase)
{
	const FVector2D CellUV(1.f / Cols, 1.f / Rows);
	const FVector2D CellOrigin(Col * CellUV.X, Row * CellUV.Y);
	const FVector2D TopLeft = Center - Size * 0.5f;

	// The head is one piece; the tentacles below WaveStart are thin rows, each pushed sideways a little
	// further than the row above so the sway grows toward the tips.
	const float Top = static_cast<float>(TopLeft.Y);
	const float HeadTop = FMath::RoundToFloat(Top);
	const float HeadH = FMath::RoundToFloat(Top + static_cast<float>(Size.Y) * WaveStart) - HeadTop;
	Canvas->K2_DrawTexture(Texture, FVector2D(TopLeft.X, HeadTop), FVector2D(Size.X, HeadH), CellOrigin, FVector2D(CellUV.X, CellUV.Y * WaveStart), Tint, BLEND_Translucent);

	constexpr int32 Strips = 10;
	const float StripH = (Top + static_cast<float>(Size.Y) - (HeadTop + HeadH)) / Strips;
	for (int32 i = 0; i < Strips; ++i)
	{
		const float T0 = float(i) / Strips;
		const float Depth = (i + 0.5f) / Strips;
		const float Offset = Amplitude * Depth * Depth * FMath::Sin(WavePhase - Depth * 3.5f);
		const float V0 = WaveStart + (1.f - WaveStart) * T0;
		const float V1 = WaveStart + (1.f - WaveStart) * (T0 + 1.f / Strips);
		// Snap row edges to whole pixels so rows meet exactly, with no gaps or doubled-up seams.
		const float Y0 = FMath::RoundToFloat(HeadTop + HeadH + i * StripH);
		const float Y1 = FMath::RoundToFloat(HeadTop + HeadH + (i + 1) * StripH);
		if (Y1 <= Y0)
		{
			continue;
		}
		Canvas->K2_DrawTexture(Texture, FVector2D(TopLeft.X + Offset, Y0), FVector2D(Size.X, Y1 - Y0),
			FVector2D(CellOrigin.X, CellOrigin.Y + CellUV.Y * V0), FVector2D(CellUV.X, CellUV.Y * (V1 - V0)), Tint, BLEND_Translucent);
	}
}

FVector2D APacSharkHUD::SpriteCellSize(UTexture2D* Texture, int32 Cols, int32 Rows) const
{
	return FVector2D(float(Texture->GetSizeX()) / Cols, float(Texture->GetSizeY()) / Rows);
}

void APacSharkHUD::DrawSprite(UTexture2D* Texture, int32 Cols, int32 Rows, int32 Col, int32 Row, const FVector2D& Center,
	const FVector2D& Size, const FLinearColor& Tint, float RotationDegrees, bool bMirror)
{
	FVector2D UVSize(1.f / Cols, 1.f / Rows);
	FVector2D UVPos(Col * UVSize.X, Row * UVSize.Y);
	if (bMirror)
	{
		UVPos.X += UVSize.X;
		UVSize.X = -UVSize.X;
	}
	Canvas->K2_DrawTexture(Texture, Center - Size * 0.5f, Size, UVPos, UVSize, Tint, BLEND_Translucent, RotationDegrees, FVector2D(0.5f, 0.5f));
}

void APacSharkHUD::DrawCover(UTexture2D* Texture, float ScreenW, float ScreenH, const FLinearColor& Tint)
{
	// Scale to fill the whole screen, cropping whichever side overflows.
	const float TexW = Texture->GetSizeX();
	const float TexH = Texture->GetSizeY();
	const float Scale = FMath::Max(ScreenW / TexW, ScreenH / TexH);
	const FVector2D Size(TexW * Scale, TexH * Scale);
	DrawSprite(Texture, 1, 1, 0, 0, FVector2D(ScreenW * 0.5f, ScreenH * 0.5f), Size, Tint);
}

void APacSharkHUD::UpdateTheme(int32 Level)
{
	const PacHUD::FLevelTheme& Theme = PacHUD::ThemeForLevel(Level);
	const float Blend = bThemeInitialized ? 1.f - FMath::Exp(-2.5f * RenderDelta) : 1.f;
	bThemeInitialized = true;
	ThemeTint = FMath::Lerp(ThemeTint, Theme.Tint, Blend);
	ThemeOverlay = FMath::Lerp(ThemeOverlay, Theme.Overlay, Blend);
	ThemeSand = FMath::Lerp(ThemeSand, Theme.Sand, Blend);
	ThemeSandEdge = FMath::Lerp(ThemeSandEdge, Theme.SandEdge, Blend);
	ThemeSandGrain = FMath::Lerp(ThemeSandGrain, Theme.SandGrain, Blend);
	ThemeBubble = FMath::Lerp(ThemeBubble, Theme.Bubble, Blend);
}

void APacSharkHUD::DrawMainMenu(const APacSharkGameMode& GameMode, float ScreenW, float ScreenH)
{
	// The title art is shown whole, so the lettering and reef are never cut off on wide or tall screens.
	// Any leftover bars are filled by stretching the art's outermost pixels, so the water and reef run on
	// to the screen edges without a seam. The menu is laid out inside the art's rectangle.
	float AreaX = 0.f, AreaY = 0.f, AreaW = ScreenW, AreaH = ScreenH;
	if (MainMenuTexture)
	{
		const float TexW = MainMenuTexture->GetSizeX();
		const float TexH = MainMenuTexture->GetSizeY();
		const float Fit = FMath::Min(ScreenW / TexW, ScreenH / TexH);
		AreaW = FMath::RoundToFloat(TexW * Fit);
		AreaH = FMath::RoundToFloat(TexH * Fit);
		AreaX = FMath::FloorToFloat((ScreenW - AreaW) * 0.5f);
		// On tall screens the art sits on the bottom edge, so only open water is stretched above it.
		AreaY = FMath::FloorToFloat(ScreenH - AreaH);
		const float EdgeU = 2.f / TexW;
		const float EdgeV = 2.f / TexH;
		if (AreaX > 0.f)
		{
			Canvas->K2_DrawTexture(MainMenuTexture, FVector2D(0.f, AreaY), FVector2D(AreaX + 1.f, AreaH), FVector2D(0.f, 0.f), FVector2D(EdgeU, 1.f), PacHUD::White, BLEND_Opaque);
			Canvas->K2_DrawTexture(MainMenuTexture, FVector2D(AreaX + AreaW - 1.f, AreaY), FVector2D(ScreenW - AreaX - AreaW + 1.f, AreaH), FVector2D(1.f - EdgeU, 0.f), FVector2D(EdgeU, 1.f), PacHUD::White, BLEND_Opaque);
		}
		if (AreaY > 0.f)
		{
			Canvas->K2_DrawTexture(MainMenuTexture, FVector2D(AreaX, 0.f), FVector2D(AreaW, AreaY + 1.f), FVector2D(0.f, 0.f), FVector2D(1.f, EdgeV), PacHUD::White, BLEND_Opaque);
		}
		Canvas->K2_DrawTexture(MainMenuTexture, FVector2D(AreaX, AreaY), FVector2D(AreaW, AreaH), FVector2D::ZeroVector, FVector2D::UnitVector, PacHUD::White, BLEND_Translucent);
	}
	else
	{
		DrawOcean(ScreenW, ScreenH);
	}

	float RefW = 0.f, RefH = 0.f;
	GetTextSize(TEXT("PLAY"), RefW, RefH, GEngine->GetMediumFont(), 1.f);
	const float Scale = RefH > 0.f ? AreaH * 0.06f / RefH : 1.f;
	const float LineH = AreaH * 0.095f;

	// The title art leaves open water on the right under the "Pac Shark" lettering: the menu and the
	// high score table sit side by side there.
	const float MenuX = AreaX + AreaW * 0.5f;
	const float TableX = AreaX + AreaW * 0.8f;
	float Y = AreaY + AreaH * 0.32f;
	if (!MainMenuTexture)
	{
		DrawTextShadowed(TEXT("PAC SHARK"), PacHUD::White, AreaX + AreaW * 0.65f, AreaY + AreaH * 0.12f, Scale * 1.6f);
	}

	const TCHAR* const Items[] = { TEXT("PLAY"), TEXT("SETTINGS"), TEXT("QUIT") };
	for (int32 Item = 0; Item < APacSharkGameMode::Menu_Count; ++Item)
	{
		const bool bSelected = Item == GameMode.GetMenuSelection() && !GameMode.IsSettingsOpen();
		const FString Label = bSelected ? FString::Printf(TEXT("> %s <"), Items[Item]) : FString(Items[Item]);
		DrawTextShadowed(Label, bSelected ? PacHUD::Sunny : PacHUD::White, MenuX, Y, Scale);
		AddTextTouchZone(TEXT("> SETTINGS <"), MenuX, Y, Scale, EPacTouch::MenuItem, Item);
		Y += LineH;
	}
	DrawTextShadowed(GameMode.IsUsingTouch() ? TEXT("TAP TO SELECT") : GameMode.IsUsingGamepad() ? TEXT("D-PAD TO CHOOSE, A TO SELECT") : TEXT("UP/DOWN TO CHOOSE, ENTER TO SELECT"),
		PacHUD::Seafoam, MenuX, Y + LineH * 0.2f, Scale * 0.42f);

	DrawHighScores(GameMode, TableX, AreaY + AreaH * 0.3f, Scale * 0.45f, AreaH * 0.036f, APacSharkGameMode::MaxHighScores);
}

void APacSharkHUD::DrawHighScores(const APacSharkGameMode& GameMode, float CenterX, float TopY, float Scale, float LineH, int32 MaxRows)
{
	DrawTextShadowed(TEXT("HIGH SCORES"), PacHUD::Sunny, CenterX, TopY, Scale * 1.15f);
	const TArray<FPacHighScore>& Scores = GameMode.GetHighScores();
	const int32 NewRank = GameMode.GetLastHighScoreRank();
	float Y = TopY + LineH * 1.5f;

	// Three columns (rank, initials, score) lined up by measuring the widest content.
	float RowW = 0.f, RowH = 0.f;
	TextSize(TEXT("10.  WWW  0000000"), RowW, RowH, Scale);
	const float LeftX = CenterX - RowW * 0.5f;
	float RankW = 0.f;
	TextSize(TEXT("10."), RankW, RowH, Scale);
	const float NameX = LeftX + RankW * 1.4f;
	const float ScoreRight = CenterX + RowW * 0.5f;
	const float Shadow = FMath::Max(1.f, Scale * 1.5f);
	auto DrawCell = [this, Shadow, Scale](const FString& Text, const FLinearColor& Color, float X, float CellY, bool bRightAlign)
	{
		float W = 0.f, H = 0.f;
		if (bRightAlign)
		{
			TextSize(Text, W, H, Scale);
		}
		DrawTextLeft(Text, FLinearColor(0.f, 0.f, 0.f, 0.8f), X - W + Shadow, CellY + Shadow, Scale);
		DrawTextLeft(Text, Color, X - W, CellY, Scale);
	};

	for (int32 Rank = 0; Rank < MaxRows; ++Rank)
	{
		const bool bHasEntry = Scores.IsValidIndex(Rank);
		const bool bNew = Rank == NewRank;
		const bool bFlash = bNew && FMath::Frac(TimeSeconds * 2.f) < 0.5f;
		const FLinearColor Color = bFlash ? PacHUD::White : bNew ? PacHUD::Sunny : bHasEntry ? PacHUD::Seafoam : PacHUD::BubbleColor;
		DrawCell(FString::Printf(TEXT("%d."), Rank + 1), Color, NameX - RankW * 0.4f, Y, true);
		DrawCell(bHasEntry ? Scores[Rank].Name : FString(TEXT("---")), Color, NameX, Y, false);
		DrawCell(bHasEntry ? FString::FromInt(Scores[Rank].Score) : FString(TEXT("-")), Color, ScoreRight, Y, true);
		Y += LineH;
	}
}

void APacSharkHUD::DrawOcean(float ScreenW, float ScreenH)
{
	if (GameBackgroundTexture)
	{
		DrawCover(GameBackgroundTexture, ScreenW, ScreenH, ThemeTint);
	}
	else
	{
		// Vertical gradient from sunlit water at the top to the deep below.
		constexpr int32 Bands = 32;
		const float BandH = ScreenH / Bands;
		for (int32 i = 0; i < Bands; ++i)
		{
			const FLinearColor Color = FMath::Lerp(PacHUD::WaterTop, PacHUD::WaterBottom, float(i) / (Bands - 1)) * ThemeTint;
			DrawRect(Color, 0.f, FMath::FloorToFloat(i * BandH), ScreenW, FMath::CeilToFloat(BandH) + 1.f);
		}
	}

	// The theme's wash of color over the whole scene.
	if (ThemeOverlay.A > 0.005f)
	{
		DrawRect(ThemeOverlay, 0.f, 0.f, ScreenW, ScreenH);
	}

	// Bubbles drift up behind the reef.
	for (int32 i = 0; i < PacHUD::NumBubbles; ++i)
	{
		const float Lane = PacHUD::Hash(i + 1.f);
		const float Speed = 0.04f + 0.06f * PacHUD::Hash(i + 101.f);
		const float Phase = PacHUD::Hash(i + 201.f);
		const float Radius = TileSize * (0.12f + 0.25f * PacHUD::Hash(i + 301.f));
		const float Rise = FMath::Frac(TimeSeconds * Speed + Phase);
		const float X = Lane * ScreenW + FMath::Sin(TimeSeconds * 1.5f + i) * TileSize * 0.4f;
		const float Y = ScreenH + Radius - Rise * (ScreenH + 2.f * Radius);
		DrawCircle(FVector2D(X, Y), Radius, ThemeBubble, 12);
		DrawCircle(FVector2D(X - Radius * 0.35f, Y - Radius * 0.35f), Radius * 0.3f, PacHUD::Plankton, 8);
	}
}

void APacSharkHUD::DrawTris(TArray<FCanvasUVTri>& Tris, UTexture2D* Texture)
{
	if (Tris.Num() == 0)
	{
		return;
	}
	// One canvas item for the whole list. The list is lent to the item and taken back, so it isn't copied.
	FCanvasTriangleItem Item(FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector,
		Texture && Texture->GetResource() ? static_cast<const FTexture*>(Texture->GetResource()) : GWhiteTexture);
	Item.TriangleList = MoveTemp(Tris);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
	Tris = MoveTemp(Item.TriangleList);
}

void APacSharkHUD::BuildWallTris(const FPacGame& Game)
{
	// Sand banks: fill each wall tile with sand, with a darker packed-sand rim along edges that face
	// open water, then sprinkle a few grains so it reads as sand rather than flat color. The walls only
	// change with a new maze or screen size, so this runs once per maze; colors are filled in by DrawMaze.
	WallTris.Reset();
	WallTriIsFill.Reset();
	GrainTris.Reset();
	const float T = TileSize;
	const float Line = FMath::Max(1.f, FMath::RoundToFloat(T * 0.2f));
	const float Grain = FMath::Max(1.f, FMath::RoundToFloat(T * 0.1f));
	auto WallAt = [&Game](int32 TX, int32 TY)
	{
		return TX < 0 || TX >= FPacGame::Width || Game.IsWall(TX, TY);
	};
	auto AddWallQuad = [this](float X, float Y, float W, float H, bool bFill)
	{
		const int32 Before = WallTris.Num();
		PacHUD::AddQuad(WallTris, X, Y, W, H, FLinearColor::White);
		for (int32 i = Before; i < WallTris.Num(); ++i)
		{
			WallTriIsFill.Add(bFill);
		}
	};

	for (int32 Y = 0; Y < FPacGame::Height; ++Y)
	{
		for (int32 X = 0; X < FPacGame::Width; ++X)
		{
			if (!Game.IsWall(X, Y))
			{
				continue;
			}
			const bool bL = WallAt(X - 1, Y);
			const bool bR = WallAt(X + 1, Y);
			const bool bU = WallAt(X, Y - 1);
			const bool bD = WallAt(X, Y + 1);
			const float PX = Origin.X + X * T;
			const float PY = Origin.Y + Y * T;
			const float InL = bL ? 0.f : Line;
			const float InR = bR ? 0.f : Line;
			const float InU = bU ? 0.f : Line;
			const float InD = bD ? 0.f : Line;
			AddWallQuad(PX, PY, T, T, false);
			AddWallQuad(PX + InL, PY + InU, T - InL - InR, T - InU - InD, true);

			// Close the little gaps at inside corners.
			if (bL && bU && !WallAt(X - 1, Y - 1)) { AddWallQuad(PX, PY, Line, Line, false); }
			if (bR && bU && !WallAt(X + 1, Y - 1)) { AddWallQuad(PX + T - Line, PY, Line, Line, false); }
			if (bL && bD && !WallAt(X - 1, Y + 1)) { AddWallQuad(PX, PY + T - Line, Line, Line, false); }
			if (bR && bD && !WallAt(X + 1, Y + 1)) { AddWallQuad(PX + T - Line, PY + T - Line, Line, Line, false); }

			// Grains stay inside their own tile's sand, so drawing them all after the walls looks the same.
			for (int32 g = 0; g < 3; ++g)
			{
				const float GX = PacHUD::Hash(X * 31.f + Y * 17.f + g * 7.f);
				const float GY = PacHUD::Hash(X * 13.f + Y * 29.f + g * 11.f + 0.5f);
				PacHUD::AddQuad(GrainTris, PX + Line + GX * (T - 2.f * Line - Grain), PY + Line + GY * (T - 2.f * Line - Grain), Grain, Grain, FLinearColor::White);
			}
		}
	}

	WallMazeId = Game.GetMazeId();
	WallTileSize = TileSize;
	WallOrigin = Origin;
	bWallColorsValid = false;
}

void APacSharkHUD::DrawMaze(const FPacGame& Game)
{
	// Flash the walls white while the level-clear pause runs.
	const bool bFlash = Game.GetPhase() == EPacGamePhase::LevelClear && FMath::Frac(Game.GetPhaseTimer() * 2.f) < 0.5f;
	const FLinearColor EdgeColor = bFlash ? PacHUD::White : ThemeSandEdge;
	const FLinearColor FillColor = bFlash ? PacHUD::White : ThemeSand;
	const float T = TileSize;
	const float Line = FMath::Max(1.f, FMath::RoundToFloat(T * 0.2f));

	if (WallMazeId != Game.GetMazeId() || WallTileSize != TileSize || WallOrigin != Origin)
	{
		BuildWallTris(Game);
	}
	// Recolor only when the colors change (theme fade, level-clear flash).
	if (!bWallColorsValid || EdgeColor != WallEdgeColor || FillColor != WallFillColor)
	{
		for (int32 i = 0; i < WallTris.Num(); ++i)
		{
			PacHUD::SetTriColor(WallTris[i], WallTriIsFill[i] ? FillColor : EdgeColor);
		}
		WallEdgeColor = EdgeColor;
		WallFillColor = FillColor;
	}
	if (!bWallColorsValid || ThemeSandGrain != WallGrainColor)
	{
		for (FCanvasUVTri& Tri : GrainTris)
		{
			PacHUD::SetTriColor(Tri, ThemeSandGrain);
		}
		WallGrainColor = ThemeSandGrain;
	}
	bWallColorsValid = true;
	DrawTris(WallTris, nullptr);
	if (!bFlash)
	{
		DrawTris(GrainTris, nullptr);
	}

	// Pearls are the dots and the power pellets (big, pulsing), and a kelp strand closes the octopus den.
	// Pearl sprites are batched like the sand; power pellet halos are drawn first so they sit underneath.
	TArray<FCanvasUVTri>& Tris = BatchTris;
	Tris.Reset();
	const float PearlPulse = Game.GetPhase() == EPacGamePhase::Playing ? 0.85f + 0.15f * FMath::Sin(TimeSeconds * 6.f) : 1.f;
	for (int32 Y = 0; Y < FPacGame::Height; ++Y)
	{
		for (int32 X = 0; X < FPacGame::Width; ++X)
		{
			const EPacTile Tile = Game.GetTile(X, Y);
			if (Tile == EPacTile::Empty || Tile == EPacTile::Wall)
			{
				continue;
			}
			const FVector2D Center = TileToScreen(FVector2D(X, Y));
			if (Tile == EPacTile::Pellet)
			{
				const FVector2D Drift(0.f, FMath::Sin(TimeSeconds * 2.f + X * 0.7f + Y) * T * 0.04f);
				if (DotTexture)
				{
					const float D = T * 0.4f;
					PacHUD::AddQuad(Tris, Center.X + Drift.X - D * 0.5f, Center.Y + Drift.Y - D * 0.5f, D, D, PacHUD::White);
				}
				else
				{
					DrawCircle(Center + Drift, FMath::Max(1.5f, T * 0.13f), PacHUD::Plankton, 8);
				}
			}
			else if (Tile == EPacTile::PowerPellet)
			{
				const float R = T * 0.45f * PearlPulse;
				if (DotTexture)
				{
					DrawCircle(Center, R * 1.15f, PacHUD::BubbleColor, 20);
					PacHUD::AddQuad(Tris, Center.X - R, Center.Y - R, 2.f * R, 2.f * R, PacHUD::White);
				}
				else
				{
					DrawCircle(Center, R, PacHUD::Pearl, 20);
					DrawCircle(Center + FVector2D(-R * 0.35f, -R * 0.35f), R * 0.3f, PacHUD::White, 10);
				}
			}
			else if (Tile == EPacTile::Door)
			{
				const float Sway = FMath::Sin(TimeSeconds * 3.f + X) * Line * 0.5f;
				DrawRect(PacHUD::Kelp, Origin.X + X * T, Center.Y - Line * 0.5f + Sway, T, Line);
			}
		}
	}
	if (DotTexture && DotTexture->GetResource())
	{
		DrawTris(Tris, DotTexture);
	}

	// A scallop shell on the sand at each side of the den's kelp door, leaning outward.
	int32 DoorRow = -1, DoorLeft = FPacGame::Width, DoorRight = -1;
	for (int32 Y = 0; Y < FPacGame::Height && DoorRow < 0; ++Y)
	{
		for (int32 X = 0; X < FPacGame::Width; ++X)
		{
			if (Game.GetTile(X, Y) == EPacTile::Door)
			{
				DoorRow = Y;
				DoorLeft = FMath::Min(DoorLeft, X);
				DoorRight = FMath::Max(DoorRight, X);
			}
		}
	}
	if (DoorRow >= 0)
	{
		Tris.Reset();
		const FLinearColor ShellLight = bFlash ? PacHUD::White : FLinearColor(1.f, 0.56f, 0.6f);
		const FLinearColor ShellDark = bFlash ? PacHUD::White : FLinearColor(1.f, 0.36f, 0.46f);
		const FLinearColor ShellRim = bFlash ? PacHUD::White : FLinearColor(0.85f, 0.2f, 0.32f);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const int32 X = Side == 0 ? DoorLeft - 1 : DoorRight + 1;
			const FVector2D Hinge = TileToScreen(FVector2D(X, DoorRow)) + FVector2D(Side == 0 ? T * 0.12f : -T * 0.12f, T * 0.38f);
			const float Lean = FMath::DegreesToRadians(Side == 0 ? -18.f : 18.f);
			const float Radius = T * 1.05f;
			auto At = [&](float Angle, float Dist)
			{
				// Angle 0 points straight up from the hinge, then the whole shell leans outward.
				const float A = Angle + Lean;
				return Hinge + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * Dist;
			};

			// Fan of ribs with a scalloped edge: alternating light and dark wedges, outlined by a darker rim.
			constexpr int32 Ribs = 7;
			const float Spread = FMath::DegreesToRadians(62.f);
			for (int32 r = 0; r < Ribs; ++r)
			{
				const float A0 = -Spread + 2.f * Spread * r / Ribs;
				const float A1 = -Spread + 2.f * Spread * (r + 1) / Ribs;
				const float AM = (A0 + A1) * 0.5f;
				PacHUD::AddTri(Tris, Hinge, At(A0, Radius * 1.06f), At(AM, Radius * 1.14f), ShellRim);
				PacHUD::AddTri(Tris, Hinge, At(AM, Radius * 1.14f), At(A1, Radius * 1.06f), ShellRim);
				const FLinearColor Rib = r % 2 == 0 ? ShellLight : ShellDark;
				PacHUD::AddTri(Tris, Hinge, At(A0, Radius * 0.95f), At(AM, Radius * 1.03f), Rib);
				PacHUD::AddTri(Tris, Hinge, At(AM, Radius * 1.03f), At(A1, Radius * 0.95f), Rib);
			}
			// The little wings either side of the hinge.
			const FVector2D Up = At(0.f, 1.f) - Hinge;
			const FVector2D Across(-Up.Y, Up.X);
			const float Wing = T * 0.27f;
			PacHUD::AddTri(Tris, Hinge - Across * Wing * 1.2f, Hinge + Across * Wing * 1.2f, Hinge + Up * Wing * 1.4f + Across * Wing, ShellDark);
			PacHUD::AddTri(Tris, Hinge - Across * Wing * 1.2f, Hinge + Up * Wing * 1.4f + Across * Wing, Hinge + Up * Wing * 1.4f - Across * Wing, ShellDark);
		}
		Canvas->K2_DrawTriangle(nullptr, Tris);
	}
}

void APacSharkHUD::DrawPlayer(const FPacGame& Game)
{
	const EPacGamePhase Phase = Game.GetPhase();

	// Small shark normally, big fat shark while a pearl is active. It wobbles between the two as the pearl wears off.
	const float FrightLeft = Game.GetFrightTimeLeft();
	float TargetFatness = FrightLeft > 0.f ? 1.f : 0.f;
	if (FrightLeft > 0.f && FrightLeft < 1.5f)
	{
		TargetFatness = FMath::Frac(TimeSeconds * 4.f) < 0.5f ? 1.f : 0.3f;
	}
	if (Phase == EPacGamePhase::Dying || Phase == EPacGamePhase::Ready)
	{
		TargetFatness = 0.f;
	}
	SharkFatness = FMath::FInterpTo(SharkFatness, TargetFatness, RenderDelta, 10.f);

	// Keep the last left/right strip and tilt it for up/down. Turns ease into the new angle.
	const EPacDir Facing = Game.GetPacFacing();
	if (Facing == EPacDir::Left && !bSharkFacesLeft)
	{
		bSharkFacesLeft = true;
		SharkAngle = -SharkAngle;
	}
	else if (Facing == EPacDir::Right && bSharkFacesLeft)
	{
		bSharkFacesLeft = false;
		SharkAngle = -SharkAngle;
	}
	float TargetAngle = 0.f;
	if (Facing == EPacDir::Up)
	{
		TargetAngle = bSharkFacesLeft ? 90.f : -90.f;
	}
	else if (Facing == EPacDir::Down)
	{
		TargetAngle = bSharkFacesLeft ? -90.f : 90.f;
	}
	SharkAngle = Phase == EPacGamePhase::Ready ? TargetAngle : FMath::FInterpTo(SharkAngle, TargetAngle, RenderDelta, 16.f);

	// Bites: a dot eaten is a quick snap; a power dot or an octopus is a bigger, slower chomp with a swell.
	const int32 PelletsLeft = Game.GetPelletsLeft();
	const bool bFrozen = Game.IsFrozen();
	if (Phase == EPacGamePhase::Playing || Phase == EPacGamePhase::LevelClear)
	{
		const bool bAtePowerDot = FrightLeft > LastFrightLeft + 0.5f;
		const bool bAteOctopus = bFrozen && !bWasFrozen;
		if (bAtePowerDot || bAteOctopus)
		{
			ChompTimeLeft = 0.22f;
			BitePulse = 1.f;
			StartBigBiteEffects(Game.GetPacPos(), bAteOctopus);
		}
		else if (LastPelletsLeft >= 0 && PelletsLeft < LastPelletsLeft)
		{
			ChompTimeLeft = 0.1f;
		}
	}
	LastPelletsLeft = PelletsLeft;
	LastFrightLeft = FrightLeft;
	bWasFrozen = bFrozen;
	ChompTimeLeft = FMath::Max(0.f, ChompTimeLeft - RenderDelta);
	BitePulse = FMath::Max(0.f, BitePulse - RenderDelta * 4.f);

	// Between bites the jaws open wide when food is just ahead, and rest slightly open otherwise.
	float TargetOpen = 0.2f;
	if (Phase == EPacGamePhase::Playing)
	{
		const FIntPoint Tile(FMath::RoundToInt(Game.GetPacPos().X), FMath::RoundToInt(Game.GetPacPos().Y));
		const FIntPoint Ahead = PacHUD::FacingToVector(Facing).IntPoint();
		for (int32 Step = 1; Step <= 2; ++Step)
		{
			const EPacTile Next = Game.GetTile(Tile.X + Ahead.X * Step, Tile.Y + Ahead.Y * Step);
			if (Next == EPacTile::Pellet || Next == EPacTile::PowerPellet)
			{
				TargetOpen = Step == 1 ? 1.f : 0.7f;
				break;
			}
			if (Next == EPacTile::Wall)
			{
				break;
			}
		}
		if (Game.GetFrightTimeLeft() > 0.f)
		{
			TargetOpen = FMath::Max(TargetOpen, 0.6f);	// hungry for octopus
		}
	}
	if (ChompTimeLeft > 0.f)
	{
		TargetOpen = 0.f;
	}
	SharkMouthOpen = FMath::FInterpTo(SharkMouthOpen, TargetOpen, RenderDelta, ChompTimeLeft > 0.f ? 40.f : 14.f);

	// The tail beats quickly while the shark swims and sways lazily while it waits.
	const FVector2D Pos = Game.GetPacPos();
	const bool bSwimming = Phase == EPacGamePhase::Playing && !bFrozen && FVector2D::Distance(Pos, LastTailPacPos) > 1e-4;
	LastTailPacPos = Pos;
	TailPhase += RenderDelta * (bSwimming ? 14.f : 4.f);
	TailSwing = FMath::FInterpTo(TailSwing, bSwimming ? 1.f : 0.35f, RenderDelta, 6.f);

	if (Pos.X < -0.4 || Pos.X > FPacGame::Width - 0.6)
	{
		return;	// inside the side tunnel
	}
	// Just caught: blink while invincible.
	const float Invincible = Game.GetInvincibleTimeLeft();
	if (Phase == EPacGamePhase::Playing && Invincible > 0.f && FMath::Fmod(Invincible, 0.2f) < 0.1f)
	{
		return;
	}
	FVector2D Center = TileToScreen(Pos);

	if (SharkTexture)
	{
		const float Width = TileSize * PacHUD::SharkWidthTiles;
		if (Phase == EPacGamePhase::Dying)
		{
			// Play the belly-up death strip, then float up to the ceiling and fade out.
			const float Elapsed = FPacGame::DyingDuration - Game.GetPhaseTimer();
			const int32 Frame = FMath::Min(PacHUD::SharkCols - 1, FMath::FloorToInt(Elapsed / 0.15f));
			Center.Y -= DeathFloatRise(Game, Elapsed) * TileSize;
			const float Wobble = Elapsed > 0.6f ? FMath::Sin(Elapsed * 5.f) * 4.f : 0.f;
			const float Alpha = 1.f - FMath::Clamp((Elapsed - (FPacGame::DyingDuration - 0.4f)) / 0.4f, 0.f, 1.f);
			DrawSharkSprite(Center, Width, SharkFatness, Frame, PacHUD::SharkRowDeath, Wobble, bSharkFacesLeft, Alpha);
			return;
		}
		// The swim strip runs from jaws wide open (first frame) to shut (last frame).
		const int32 Frame = FMath::Clamp(FMath::RoundToInt((1.f - SharkMouthOpen) * (PacHUD::SharkCols - 1)), 0, PacHUD::SharkCols - 1);
		DrawSharkSprite(Center, Width * (1.f + 0.15f * FMath::Sin(BitePulse * PI)), SharkFatness, Frame,
			bSharkFacesLeft ? PacHUD::SharkRowLeft : PacHUD::SharkRowRight, SharkAngle, false, 1.f, TailSwing);
		return;
	}

	// Fallback: the shape-drawn shark.
	const float Size = TileSize * FMath::Lerp(0.95f, 1.15f, SharkFatness);
	if (Phase == EPacGamePhase::Dying)
	{
		const float Elapsed = FPacGame::DyingDuration - Game.GetPhaseTimer();
		Center.Y -= DeathFloatRise(Game, Elapsed) * TileSize;
		const float Shrink = 1.f - FMath::Clamp((Elapsed - (FPacGame::DyingDuration - 0.4f)) / 0.4f, 0.f, 1.f);
		if (Shrink > 0.02f)
		{
			DrawShark(Center, Size * Shrink, Facing, 25.f, SharkFatness, Elapsed > 0.3f);
		}
		return;
	}
	const bool bStill = Phase == EPacGamePhase::Ready;
	const float MaxMouth = FMath::Lerp(32.f, 40.f, SharkFatness);
	const float Mouth = bStill ? 3.f : 3.f + MaxMouth * SharkMouthOpen;
	DrawShark(Center, Size, Facing, Mouth, SharkFatness, false);
}

float APacSharkHUD::DeathFloatRise(const FPacGame& Game, float Elapsed) const
{
	// After the death strip the dead shark drifts up, speeding up like something buoyant,
	// until its back bumps the first wall above it. Returns how many tiles it has risen.
	const float FloatDelay = 0.6f;
	const float T = Elapsed - FloatDelay;
	if (T <= 0.f)
	{
		return 0.f;
	}

	const FVector2D Pos = Game.GetPacPos();
	const int32 X = FMath::RoundToInt(Pos.X);
	int32 Y = FMath::RoundToInt(Pos.Y);
	while (Y > 0 && Game.GetTile(X, Y - 1) != EPacTile::Wall && Game.GetTile(X, Y - 1) != EPacTile::Door)
	{
		--Y;
	}
	// The ceiling's underside is half a tile above that row's center; stop with the shark's back just touching it.
	const float Ceiling = FMath::Max(0.f, static_cast<float>(Pos.Y) - (Y - 0.5f) - 0.45f);

	// Long climbs speed up more, so the shark always reaches the ceiling within MaxClimbTime and is seen resting there.
	const float Speed = 2.f;
	const float MaxClimbTime = 1.5f;
	const float Accel = FMath::Max(3.f, (Ceiling - Speed * MaxClimbTime) / (MaxClimbTime * MaxClimbTime));
	const float Rise = Speed * T + Accel * T * T;
	if (Rise < Ceiling)
	{
		return Rise;
	}

	// A small bounce off the ceiling, then it rests there.
	const float HitTime = (-Speed + FMath::Sqrt(Speed * Speed + 4.f * Accel * Ceiling)) / (2.f * Accel);
	const float SinceHit = T - HitTime;
	return FMath::Max(0.f, Ceiling - 0.2f * FMath::Exp(-5.f * SinceHit) * FMath::Abs(FMath::Sin(SinceHit * 9.f)));
}

void APacSharkHUD::StartBigBiteEffects(const FVector2D& TilePos, bool bOctopus)
{
	// Bubbles burst out in every direction, then drift upward as they slow down.
	const int32 Count = bOctopus ? 18 : 12;
	for (int32 i = 0; i < Count; ++i)
	{
		FBiteBubble& Bubble = BiteBubbles.AddDefaulted_GetRef();
		const float Angle = 2.f * PI * (i + FMath::FRand() * 0.6f) / Count;
		const float Speed = FMath::FRandRange(2.f, bOctopus ? 6.f : 4.5f);
		Bubble.Pos = TilePos;
		Bubble.Velocity = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Speed;
		Bubble.Radius = FMath::FRandRange(0.1f, bOctopus ? 0.32f : 0.25f);
		Bubble.MaxLife = Bubble.Life = FMath::FRandRange(0.6f, 1.1f);
		Bubble.Wobble = FMath::FRand() * 2.f * PI;
	}
}

void APacSharkHUD::UpdateTrails(const FPacGame& Game)
{
	const bool bPlaying = Game.GetPhase() == EPacGamePhase::Playing && !Game.IsFrozen();

	// The shark leaves a bubble every third of a tile it swims, just behind its tail.
	const FVector2D PacPos = Game.GetPacPos();
	const double Moved = FVector2D::Distance(PacPos, LastTrailPacPos);
	LastTrailPacPos = PacPos;
	if (bPlaying && Moved < 1.0)
	{
		SharkTrailDistance += static_cast<float>(Moved);
		const FVector2D Behind = -PacHUD::FacingToVector(Game.GetPacFacing());
		while (SharkTrailDistance >= 0.33f)
		{
			SharkTrailDistance -= 0.33f;
			FBiteBubble& Bubble = TrailBubbles.AddDefaulted_GetRef();
			const FVector2D Side(-Behind.Y, Behind.X);
			Bubble.Pos = PacPos + Behind * (1.2f + 0.1f * SharkFatness) + Side * FMath::FRandRange(-0.25f, 0.25f);
			Bubble.Velocity = Behind * FMath::FRandRange(0.2f, 0.6f) + FVector2D(0.f, -0.3f);
			Bubble.Radius = FMath::FRandRange(0.05f, 0.12f);
			Bubble.MaxLife = Bubble.Life = FMath::FRandRange(0.5f, 0.9f);
			Bubble.Wobble = FMath::FRand() * 2.f * PI;
		}
	}

	// A dashing Sprinter streams bubbles too, so its bursts are easy to spot.
	OctopusTrailTime += RenderDelta;
	if (bPlaying && OctopusTrailTime >= 0.05f)
	{
		OctopusTrailTime = 0.f;
		const TArray<FPacGhost>& Ghosts = Game.GetGhosts();
		for (int32 i = 0; i < Ghosts.Num(); ++i)
		{
			const FPacGhost& Ghost = Ghosts[i];
			if (Ghost.State != EPacGhostState::Active || Ghost.bFrightened || Game.GetPersonalitySpeed(i) < 1.3f)
			{
				continue;
			}
			const FVector2D Behind = -PacHUD::FacingToVector(Ghost.Dir);
			FBiteBubble& Bubble = TrailBubbles.AddDefaulted_GetRef();
			Bubble.Pos = Ghost.Pos + Behind * 0.7f + FVector2D(FMath::FRandRange(-0.2f, 0.2f), FMath::FRandRange(-0.2f, 0.2f));
			Bubble.Velocity = Behind * 0.8f + FVector2D(0.f, -0.3f);
			Bubble.Radius = FMath::FRandRange(0.06f, 0.13f);
			Bubble.MaxLife = Bubble.Life = FMath::FRandRange(0.35f, 0.6f);
			Bubble.Wobble = FMath::FRand() * 2.f * PI;
		}
	}
}

void APacSharkHUD::DrawTrailBubbles()
{
	for (int32 i = TrailBubbles.Num() - 1; i >= 0; --i)
	{
		FBiteBubble& Bubble = TrailBubbles[i];
		Bubble.Life -= RenderDelta;
		if (Bubble.Life <= 0.f)
		{
			TrailBubbles.RemoveAtSwap(i);
			continue;
		}
		Bubble.Velocity *= FMath::Exp(-3.f * RenderDelta);
		Bubble.Velocity.Y -= 1.2f * RenderDelta;
		Bubble.Pos += Bubble.Velocity * RenderDelta;
		const float Alpha = Bubble.Life / Bubble.MaxLife;
		const FVector2D Center = TileToScreen(Bubble.Pos + FVector2D(FMath::Sin(TimeSeconds * 7.f + Bubble.Wobble) * 0.05f, 0.f));
		DrawCircle(Center, Bubble.Radius * TileSize * (1.2f - 0.4f * Alpha), FLinearColor(ThemeBubble.R, ThemeBubble.G, ThemeBubble.B, 0.4f * Alpha), 10);
	}
}

void APacSharkHUD::DrawOctopusLegend(const FPacGame& Game, float ScreenW, float TextScale)
{
	// When the screen is wide enough, list each octopus's personality beside the maze.
	const float Right = Origin.X + FPacGame::Width * TileSize;
	if (ScreenW - Right < TileSize * 10.f || Game.GetGhosts().Num() == 0)
	{
		return;
	}
	const float X = Right + TileSize * 1.5f;
	float Y = Origin.Y + TileSize;
	DrawTextLeft(TEXT("OCTOPI"), PacHUD::Sunny, X, Y, TextScale * 0.8f);
	Y += TileSize * 1.6f;
	for (int32 i = 0; i < Game.GetGhosts().Num(); ++i)
	{
		const FLinearColor Color = PacHUD::OctopusColors[i];
		DrawCircle(FVector2D(X + TileSize * 0.4f, Y + TileSize * 0.45f), TileSize * 0.4f, Color, 14);
		DrawTextLeft(FPacGame::GetPersonalityName(i), Color, X + TileSize * 1.2f, Y, TextScale * 0.7f);
		Y += TileSize * 1.3f;
	}
}

void APacSharkHUD::DrawBiteBubbles()
{
	for (int32 i = BiteBubbles.Num() - 1; i >= 0; --i)
	{
		FBiteBubble& Bubble = BiteBubbles[i];
		Bubble.Life -= RenderDelta;
		if (Bubble.Life <= 0.f)
		{
			BiteBubbles.RemoveAtSwap(i);
			continue;
		}
		// Water drag slows the burst, buoyancy lifts the bubbles, and they wobble side to side.
		Bubble.Velocity *= FMath::Exp(-4.f * RenderDelta);
		Bubble.Velocity.Y -= 3.f * RenderDelta;
		Bubble.Pos += Bubble.Velocity * RenderDelta;
		const float Sway = FMath::Sin(TimeSeconds * 9.f + Bubble.Wobble) * 0.06f;

		const float Alpha = FMath::Clamp(Bubble.Life / Bubble.MaxLife * 1.5f, 0.f, 1.f);
		const FVector2D Center = TileToScreen(Bubble.Pos + FVector2D(Sway, 0.f));
		const float R = Bubble.Radius * TileSize;
		DrawCircle(Center, R, FLinearColor(ThemeBubble.R, ThemeBubble.G, ThemeBubble.B, 0.65f * Alpha), 14);
		DrawCircle(Center - FVector2D(R * 0.35f, R * 0.35f), R * 0.3f, FLinearColor(1.f, 1.f, 1.f, 0.8f * Alpha), 8);
	}
}

void APacSharkHUD::DrawSharkSprite(const FVector2D& Center, float Width, float Fatness, int32 Frame, int32 Row, float RotationDegrees, bool bMirror, float Alpha,
	float Swing)
{
	// Fatness stretches the sprite, more in height than length, so the shark looks stuffed.
	const FVector2D Cell = SpriteCellSize(SharkTexture, PacHUD::SharkCols, PacHUD::SharkRows);
	// The fat size is fixed in tiles, so a bigger normal shark doesn't make the stuffed one bigger too.
	const float FatW = PacHUD::FatSharkWidthTiles / PacHUD::SharkWidthTiles;
	const float FatH = PacHUD::FatSharkHeightTiles / PacHUD::SharkWidthTiles;
	const float W = Width * FMath::Lerp(1.f, FatW, Fatness);
	const float H = Width * (Cell.Y / Cell.X) * FMath::Lerp(1.f, FatH, Fatness);
	const FLinearColor Tint(1.f, 1.f, 1.f, Alpha);
	if (Swing <= 0.f || bMirror || Row == PacHUD::SharkRowDeath || !SharkTexture->GetResource())
	{
		DrawSprite(SharkTexture, PacHUD::SharkCols, PacHUD::SharkRows, Frame, Row, Center, FVector2D(W, H), Tint, RotationDegrees, bMirror);
		return;
	}

	// The sprite becomes a strip of slices along its length. Slices toward the tail are pushed sideways
	// by a wave that grows toward the tip, so the tail beats while the head stays steady.
	const FVector2D CellUV(1.f / PacHUD::SharkCols, 1.f / PacHUD::SharkRows);
	const FVector2D CellOrigin(Frame * CellUV.X, Row * CellUV.Y);
	const bool bTailOnLeft = Row == PacHUD::SharkRowRight;
	const float Rad = FMath::DegreesToRadians(RotationDegrees);
	const float Cos = FMath::Cos(Rad);
	const float Sin = FMath::Sin(Rad);
	auto ToScreen = [&](float X, float Y)
	{
		return FVector2D(Center.X + X * Cos - Y * Sin, Center.Y + X * Sin + Y * Cos);
	};

	constexpr int32 Slices = 16;
	constexpr float TailLength = 0.5f;	// back half of the sprite bends
	const float Amplitude = H * 0.2f * Swing;
	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(Slices * 2);
	FVector2D PrevTop = Center;
	FVector2D PrevBottom = Center;
	float PrevU = 0.f;
	for (int32 i = 0; i <= Slices; ++i)
	{
		const float U = float(i) / Slices;	// 0 = left edge of the cell, 1 = right edge
		const float FromTail = bTailOnLeft ? U : 1.f - U;
		const float Bend = FMath::Square(FMath::Max(0.f, (TailLength - FromTail) / TailLength));
		const float Offset = Amplitude * Bend * FMath::Sin(TailPhase - FromTail * 5.f);
		const float X = (U - 0.5f) * W;
		const FVector2D Top = ToScreen(X, -H * 0.5f + Offset);
		const FVector2D Bottom = ToScreen(X, H * 0.5f + Offset);
		if (i > 0)
		{
			const FVector2D UV0(CellOrigin.X + CellUV.X * PrevU, CellOrigin.Y);
			const FVector2D UV1(CellOrigin.X + CellUV.X * U, CellOrigin.Y);
			const FVector2D UV0B(UV0.X, CellOrigin.Y + CellUV.Y);
			const FVector2D UV1B(UV1.X, CellOrigin.Y + CellUV.Y);

			FCanvasUVTri A;
			A.V0_Pos = PrevTop; A.V0_UV = UV0;
			A.V1_Pos = Top; A.V1_UV = UV1;
			A.V2_Pos = PrevBottom; A.V2_UV = UV0B;
			A.V0_Color = A.V1_Color = A.V2_Color = Tint;
			Tris.Add(A);

			FCanvasUVTri B;
			B.V0_Pos = Top; B.V0_UV = UV1;
			B.V1_Pos = Bottom; B.V1_UV = UV1B;
			B.V2_Pos = PrevBottom; B.V2_UV = UV0B;
			B.V0_Color = B.V1_Color = B.V2_Color = Tint;
			Tris.Add(B);
		}
		PrevTop = Top;
		PrevBottom = Bottom;
		PrevU = U;
	}

	FCanvasTriangleItem Item(Tris, SharkTexture->GetResource());
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void APacSharkHUD::DrawShark(const FVector2D& Center, float Size, EPacDir Facing, float MouthDegrees, float Fatness, bool bBellyUp)
{
	using namespace PacHUD;

	// Left mirrors the right-facing shark so it never swims upside down; up and down are rotations.
	FBodyFrame Frame{ Center, FVector2D(1.f, 0.f), FVector2D(0.f, 1.f), Size };
	switch (Facing)
	{
	case EPacDir::Left: Frame.Forward = FVector2D(-1.f, 0.f); break;
	case EPacDir::Up: Frame.Forward = FVector2D(0.f, -1.f); Frame.Side = FVector2D(1.f, 0.f); break;
	case EPacDir::Down: Frame.Forward = FVector2D(0.f, 1.f); Frame.Side = FVector2D(-1.f, 0.f); break;
	default: break;
	}
	if (bBellyUp)
	{
		Frame.Side = -Frame.Side;
	}

	const float Wag = FMath::Sin(TimeSeconds * 12.f) * 0.12f;
	const float BodyA = FMath::Lerp(0.95f, 1.f, Fatness);
	const float BodyB = FMath::Lerp(0.42f, 0.75f, Fatness);
	const float Girth = BodyB / 0.5f;	// fins and eye ride on the body outline

	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(64);

	// Tail fin.
	AddTri(Tris, Frame.At(-0.8f, 0.f), Frame.At(-1.3f, -0.55f + Wag), Frame.At(-1.02f, 0.05f), SharkFin);
	AddTri(Tris, Frame.At(-0.8f, 0.f), Frame.At(-1.2f, 0.42f + Wag), Frame.At(-1.02f, -0.05f), SharkFin);
	// Dorsal fin.
	AddTri(Tris, Frame.At(-0.38f, -0.4f * Girth), Frame.At(0.15f, -0.42f * Girth), Frame.At(-0.32f, -0.4f * Girth - 0.58f), SharkFin);

	// Body with the jaws cut out at the front, then a pale belly underneath.
	AddEllipseFan(Tris, Frame, 0.f, 0.f, BodyA, BodyB, MouthDegrees, 360.f - MouthDegrees, 28, SharkBody);
	AddEllipseFan(Tris, Frame, 0.f, 0.04f, BodyA * 0.92f, BodyB * 0.78f, MouthDegrees, 180.f, 14, SharkBelly);

	// Pectoral fin.
	AddTri(Tris, Frame.At(-0.02f, 0.32f * Girth), Frame.At(0.3f, 0.38f * Girth), Frame.At(-0.24f, 0.32f * Girth + 0.5f), SharkFin);

	// Teeth along both jaws once the mouth is open wide enough to show them.
	if (MouthDegrees > 12.f)
	{
		const float Rad = FMath::DegreesToRadians(MouthDegrees);
		const FVector2D Upper(BodyA * FMath::Cos(Rad), -BodyB * FMath::Sin(Rad));
		for (int32 k = 1; k <= 2; ++k)
		{
			const float F = 0.45f + 0.25f * k;
			const FVector2D U = Upper * F;
			const FVector2D L(U.X, -U.Y);
			AddTri(Tris, Frame.At(U.X - 0.07f, U.Y * 0.98f), Frame.At(U.X + 0.07f, U.Y * 1.02f), Frame.At(U.X, U.Y + 0.13f), White);
			AddTri(Tris, Frame.At(L.X - 0.07f, L.Y * 0.98f), Frame.At(L.X + 0.07f, L.Y * 1.02f), Frame.At(L.X, L.Y - 0.13f), White);
		}
	}

	Canvas->K2_DrawTriangle(nullptr, Tris);

	// Eye with a glint.
	const FVector2D Eye = Frame.At(0.42f, -0.27f * Girth);
	DrawCircle(Eye, Size * 0.09f, SharkEye, 10);
	DrawCircle(Eye + FVector2D(-Size * 0.03f, -Size * 0.03f), Size * 0.03f, White, 6);
}

void APacSharkHUD::DrawOctopus(const FPacGame& Game, int32 GhostIndex)
{
	const FPacGhost& Ghost = Game.GetGhosts()[GhostIndex];
	if (Ghost.Pos.X < -0.4 || Ghost.Pos.X > FPacGame::Width - 0.6)
	{
		return;	// inside the side tunnel
	}

	// Ease the drawn position toward the simulated one so 90-degree turns look rounded. Big jumps
	// (tunnel wrap, level reset) snap instead.
	if (SmoothedOctopusPos.Num() != Game.GetGhosts().Num())
	{
		SmoothedOctopusPos.Init(FVector2D(-100.f, -100.f), Game.GetGhosts().Num());
	}
	FVector2D& Smoothed = SmoothedOctopusPos[GhostIndex];
	if (FVector2D::Distance(Smoothed, Ghost.Pos) > 1.5)
	{
		Smoothed = Ghost.Pos;
	}
	else
	{
		Smoothed = FMath::Lerp(Smoothed, Ghost.Pos, 1.f - FMath::Exp(-30.f * RenderDelta));
	}

	const FVector2D Center = TileToScreen(Smoothed);
	const float R = TileSize * 0.68f;
	const bool bEyesOnly = Ghost.State == EPacGhostState::Eaten || Ghost.State == EPacGhostState::Entering;
	const float Phase = TimeSeconds * 5.f + GhostIndex * 1.7f;

	// Pupils follow the heading, easing round so a turn reads as a glance.
	if (SmoothedOctopusLook.Num() != Game.GetGhosts().Num())
	{
		SmoothedOctopusLook.Init(FVector2D::ZeroVector, Game.GetGhosts().Num());
	}
	FVector2D& Look = SmoothedOctopusLook[GhostIndex];
	Look = FMath::Lerp(Look, PacHUD::FacingToVector(Ghost.Dir), 1.f - FMath::Exp(-14.f * RenderDelta));

	if (OctopusTexture && bEyesOnly)
	{
		// Eaten: just the eyes, bigger, darting back to the den with a trail of bubbles behind them.
		const FVector2D Cell = SpriteCellSize(OctopusTexture, PacHUD::OctopusCols, PacHUD::OctopusRows);
		const FVector2D Size(TileSize * PacHUD::OctopusWidthTiles, TileSize * PacHUD::OctopusWidthTiles * Cell.Y / Cell.X);
		const FVector2D Heading = PacHUD::FacingToVector(Ghost.Dir);
		for (int32 k = 1; k <= 3; ++k)
		{
			const float Age = FMath::Frac(TimeSeconds * 3.f + k / 3.f + GhostIndex * 0.37f);
			const FVector2D Side(-Heading.Y, Heading.X);
			const FVector2D At = Center - Heading * (TileSize * (0.4f + Age * 1.1f)) + Side * (FMath::Sin(TimeSeconds * 8.f + k * 2.f) * TileSize * 0.15f)
				+ FVector2D(0.f, -Age * TileSize * 0.3f);
			const float BubbleR = TileSize * (0.1f + 0.06f * k) * (1.f - Age * 0.4f);
			DrawCircle(At, BubbleR, FLinearColor(ThemeBubble.R, ThemeBubble.G, ThemeBubble.B, 0.7f * (1.f - Age)), 12);
		}
		const float Bob = FMath::Sin(TimeSeconds * 10.f + GhostIndex) * TileSize * 0.05f;
		// Push the eyes down to the sprite's middle so they sit where the body was.
		DrawOctopusEyes(Center + FVector2D(0.f, Bob + Size.Y * 0.2f), Size, Look, 1.7f);
		return;
	}

	if (OctopusTexture && !bEyesOnly)
	{
		// The sprites are white silhouettes, so each octopus is tinted its own color.
		FLinearColor Tint = PacHUD::OctopusColors[GhostIndex];
		int32 Row = 0;
		float Shiver = 0.f;
		if (Ghost.bFrightened)
		{
			Tint = GetFrightFlash(Game, GhostIndex, Shiver) ? PacHUD::White : PacHUD::FrightBlue;
			Row = PacHUD::OctopusRowEdible;
		}
		const FVector2D Cell = SpriteCellSize(OctopusTexture, PacHUD::OctopusCols, PacHUD::OctopusRows);
		const float W = TileSize * PacHUD::OctopusWidthTiles;
		const float H = W * Cell.Y / Cell.X;
		const float Bob = FMath::Sin(TimeSeconds * 4.f + GhostIndex * 1.7f) * TileSize * 0.07f;
		const int32 Frame = PacHUD::PingPongFrame(TimeSeconds * 8.f + GhostIndex * 1.3f, PacHUD::OctopusCols);
		// Scared octopi wriggle faster and wider.
		const float WaveSpeed = Ghost.bFrightened ? 11.f : 6.f;
		const float WaveAmp = TileSize * (Ghost.bFrightened ? 0.14f : 0.09f);
		DrawWavySprite(OctopusTexture, PacHUD::OctopusCols, PacHUD::OctopusRows, Frame, Row, Center + FVector2D(Shiver, Bob), FVector2D(W, H), Tint,
			0.45f, WaveAmp, TimeSeconds * WaveSpeed + GhostIndex * 2.1f);
		if (!Ghost.bFrightened)
		{
			// The sprite's eye holes get eyeballs that look where the octopus is going. Scared octopi keep
			// the sprite's worried eyes.
			DrawOctopusEyes(Center + FVector2D(Shiver, Bob), FVector2D(W, H), Look, 1.f);
		}
		return;
	}

	// Flapjack octopus: a squashed pancake dome, two flapping ear fins and a scalloped webbed skirt.
	const float Squish = 1.f + 0.07f * FMath::Sin(Phase);
	const FVector2D Base = Center + FVector2D(0.f, R * 0.3f);
	const float DomeW = R * Squish;
	const float DomeH = R * 0.72f / Squish;

	if (!bEyesOnly)
	{
		FLinearColor Body = PacHUD::OctopusColors[GhostIndex];
		if (Ghost.bFrightened)
		{
			float Shiver = 0.f;
			Body = GetFrightFlash(Game, GhostIndex, Shiver) ? PacHUD::White : PacHUD::FrightBlue;
		}

		// Ear fins sit on the upper sides of the dome and flap.
		const float Flap = FMath::Sin(Phase * 1.6f) * R * 0.08f;
		for (float Side : { -1.f, 1.f })
		{
			DrawCircle(Base + FVector2D(Side * DomeW * 0.62f, -DomeH * 0.8f + Flap), R * 0.2f, Body, 10);
		}

		const PacHUD::FBodyFrame Frame{ Base, FVector2D(1.f, 0.f), FVector2D(0.f, 1.f), 1.f };
		TArray<FCanvasUVTri> Tris;
		PacHUD::AddEllipseFan(Tris, Frame, 0.f, 0.f, DomeW, DomeH, 180.f, 360.f, 18, Body);
		Canvas->K2_DrawTriangle(nullptr, Tris);

		// Webbed skirt: a band under the dome with a row of bobbing scallops.
		DrawRect(Body, Base.X - DomeW, Base.Y - 1.f, 2.f * DomeW, R * 0.14f);
		constexpr int32 Scallops = 5;
		for (int32 k = 0; k < Scallops; ++k)
		{
			const float X = Base.X - DomeW * 0.8f + DomeW * 1.6f * k / (Scallops - 1);
			const float Bob = FMath::Sin(Phase * 1.3f + k * 1.2f) * R * 0.06f;
			DrawCircle(FVector2D(X, Base.Y + R * 0.14f + Bob), R * 0.2f, Body, 10);
		}

		if (Ghost.bFrightened)
		{
			const FLinearColor Face = Body == PacHUD::White ? PacHUD::TextRed : PacHUD::Plankton;
			DrawRect(Face, Center.X - R * 0.4f, Center.Y - R * 0.2f, R * 0.2f, R * 0.2f);
			DrawRect(Face, Center.X + R * 0.2f, Center.Y - R * 0.2f, R * 0.2f, R * 0.2f);
			DrawRect(Face, Center.X - R * 0.4f, Center.Y + R * 0.15f, R * 0.8f, R * 0.08f);
			return;
		}
	}

	for (float Side : { -1.f, 1.f })
	{
		const FVector2D EyeCenter = Base + FVector2D(Side * R * 0.34f, -DomeH * 0.45f);
		DrawCircle(EyeCenter, R * 0.24f, PacHUD::White, 12);
		DrawCircle(EyeCenter + Look * (R * 0.11f), R * 0.12f, PacHUD::PupilBlue, 10);
	}
}

void APacSharkHUD::DrawPanel(float ScreenW, float ScreenH, float PanelW, float PanelH, float& OutX, float& OutY)
{
	// Dim whatever is underneath, then draw a reef-rock panel with a coral border. Taps only reach the panel.
	TouchZones.Reset();
	DrawRect(FLinearColor(0.f, 0.02f, 0.06f, 0.75f), 0.f, 0.f, ScreenW, ScreenH);
	OutX = FMath::FloorToFloat((ScreenW - PanelW) * 0.5f);
	OutY = FMath::FloorToFloat((ScreenH - PanelH) * 0.5f);
	const float Border = FMath::Max(2.f, FMath::RoundToFloat(TileSize * 0.2f));
	DrawRect(PacHUD::Coral, OutX - Border, OutY - Border, PanelW + 2.f * Border, PanelH + 2.f * Border);
	DrawRect(PacHUD::Rock, OutX, OutY, PanelW, PanelH);
}

void APacSharkHUD::DrawSettings(const APacSharkGameMode& GameMode, float ScreenW, float ScreenH, float TextScale)
{
	const float T = TileSize;
	const float PanelW = T * 22.f;
	const float PanelH = T * 26.f;
	float PX = 0.f, PY = 0.f;
	DrawPanel(ScreenW, ScreenH, PanelW, PanelH, PX, PY);

	const float MidX = PX + PanelW * 0.5f;
	const int32 Selected = GameMode.GetSettingsSelection();
	auto ItemColor = [Selected](int32 Item)
	{
		return Item == Selected ? PacHUD::Sunny : PacHUD::White;
	};
	const float Left = PX + T * 2.f;
	const float Right = PX + PanelW - T * 2.f;

	DrawTextCentered(TEXT("SETTINGS"), PacHUD::Sunny, MidX, PY + T, TextScale * 1.3f);

	// Difficulty presets: a left/right picker that sets all three sliders.
	float RowY = PY + T * 3.6f;
	{
		const int32 Item = APacSharkGameMode::Setting_Difficulty;
		const bool bSel = Selected == Item;
		DrawTextLeft(FString(bSel ? TEXT("> ") : TEXT("  ")) + TEXT("DIFFICULTY"), ItemColor(Item), Left, RowY, TextScale);
		const TCHAR* const Names[] = { TEXT("EASY"), TEXT("NORMAL"), TEXT("HARD"), TEXT("CUSTOM") };
		const FString ValueText = FString::Printf(TEXT("< %s >"), Names[GameMode.GetDifficulty()]);
		float ValueW = 0.f, ValueH = 0.f;
		TextSize(ValueText, ValueW, ValueH, TextScale);
		DrawTextLeft(ValueText, ItemColor(Item), Right - ValueW, RowY, TextScale);
		AddTouchZone(Left, RowY - T * 0.5f, Right - ValueW - Left, T * 2.f, EPacTouch::SettingsItem, Item);
		AddTouchZone(Right - ValueW - T, RowY - T * 0.5f, ValueW * 0.5f + T, T * 2.f, EPacTouch::SettingsPrev, Item);
		AddTouchZone(Right - ValueW * 0.5f, RowY - T * 0.5f, ValueW * 0.5f + T, T * 2.f, EPacTouch::SettingsNext, Item);
		RowY += T * 2.6f;
	}

	// Sliders: octopus speed, octopus count and power dot count.
	const float TrackH = FMath::Max(2.f, T * 0.3f);
	for (int32 Item = APacSharkGameMode::Setting_OctopusSpeed; Item <= APacSharkGameMode::Setting_PowerDots; ++Item)
	{
		const int32 Value = GameMode.GetSliderValue(Item);
		const int32 MinValue = APacSharkGameMode::GetSliderMin(Item);
		const int32 MaxValue = APacSharkGameMode::GetSliderMax(Item);
		const bool bSliderSelected = Selected == Item;

		const TCHAR* Name = TEXT("POWER DOTS");
		FString ValueText = FString::FromInt(Value);
		if (Item == APacSharkGameMode::Setting_OctopusSpeed)
		{
			Name = TEXT("OCTOPUS SPEED");
			ValueText += Value <= 2 ? TEXT("  GENTLE") : Value <= 4 ? TEXT("  SLOW") : Value <= 6 ? TEXT("  NORMAL") : Value <= 8 ? TEXT("  FAST") : TEXT("  FRENZY");
		}
		else if (Item == APacSharkGameMode::Setting_OctopusCount)
		{
			Name = TEXT("OCTOPI");
		}
		DrawTextLeft(FString(bSliderSelected ? TEXT("> ") : TEXT("  ")) + Name, ItemColor(Item), Left, RowY, TextScale);
		float ValueW = 0.f, ValueH = 0.f;
		TextSize(ValueText, ValueW, ValueH, TextScale);
		DrawTextLeft(ValueText, ItemColor(Item), Right - ValueW, RowY, TextScale);

		const float TrackY = RowY + T * 2.f;
		const float Alpha = float(Value - MinValue) / float(MaxValue - MinValue);
		const float KnobX = FMath::Lerp(Left, Right, Alpha);
		DrawRect(PacHUD::WaterBottom, Left, TrackY - TrackH * 0.5f, Right - Left, TrackH);
		DrawRect(PacHUD::Seafoam, Left, TrackY - TrackH * 0.5f, KnobX - Left, TrackH);
		for (int32 Step = MinValue; Step <= MaxValue; ++Step)
		{
			const float TickX = FMath::Lerp(Left, Right, float(Step - MinValue) / float(MaxValue - MinValue));
			DrawRect(PacHUD::Seafoam, FMath::RoundToFloat(TickX) - 1.f, TrackY + TrackH, 2.f, T * 0.35f);
		}
		DrawCircle(FVector2D(KnobX, TrackY), T * 0.55f, bSliderSelected ? PacHUD::Sunny : PacHUD::Pearl, 20);
		AddTouchZone(Left, RowY - T * 0.5f, Right - Left, T * 1.6f, EPacTouch::SettingsItem, Item);
		// The track zone spans exactly Left..Right so a finger's X maps straight to a value; it is tall for easy grabbing.
		AddTouchZone(Left, TrackY - T * 1.1f, Right - Left, T * 2.2f, EPacTouch::SettingsTrack, Item);
		RowY += T * 3.8f;
	}
	if (!GameMode.IsInMainMenu())
	{
		DrawTextCentered(TEXT("OCTOPI AND POWER DOTS CHANGE ON THE NEXT LEVEL"), PacHUD::Seafoam, MidX, RowY - T * 0.6f, TextScale * 0.5f);
	}

	// Clearing the high scores takes a second press, so one stray tap cannot wipe them.
	const int32 ResetItem = APacSharkGameMode::Setting_ResetScores;
	const bool bResetArmed = GameMode.IsResetScoresArmed();
	const TCHAR* ResetLabel = bResetArmed ? TEXT("PRESS AGAIN TO CLEAR") : TEXT("RESET HIGH SCORES");
	DrawTextCentered(Selected == ResetItem ? FString::Printf(TEXT("> %s <"), ResetLabel) : FString(ResetLabel),
		bResetArmed ? PacHUD::Coral : ItemColor(ResetItem), MidX, RowY + T * 0.8f, TextScale * 0.8f);
	AddTextTouchZone(TEXT("> PRESS AGAIN TO CLEAR <"), MidX, RowY + T * 0.8f, TextScale * 0.8f, EPacTouch::SettingsItem, ResetItem);
	RowY += T * 2.2f;

	const int32 BackItem = APacSharkGameMode::Setting_Back;
	DrawTextCentered(Selected == BackItem ? TEXT("> BACK <") : TEXT("BACK"), ItemColor(BackItem), MidX, RowY + T * 0.8f, TextScale);
	AddTextTouchZone(TEXT("> BACK <"), MidX, RowY + T * 0.8f, TextScale, EPacTouch::SettingsItem, BackItem);

	DrawTextCentered(GameMode.IsUsingTouch() ? TEXT("TAP < > OR DRAG THE SLIDERS") : GameMode.IsUsingGamepad() ? TEXT("D-PAD SELECT AND ADJUST   B BACK") : TEXT("UP/DOWN SELECT   LEFT/RIGHT ADJUST   TAB BACK"),
		PacHUD::Seafoam, MidX, PY + PanelH - T * 1.6f, TextScale * 0.55f);
}

void APacSharkHUD::DrawPauseMenu(const APacSharkGameMode& GameMode, float ScreenW, float ScreenH, float TextScale)
{
	const float T = TileSize;
	const float PanelW = T * 18.f;
	const float PanelH = T * 15.f;
	float PX = 0.f, PY = 0.f;
	DrawPanel(ScreenW, ScreenH, PanelW, PanelH, PX, PY);

	const float MidX = PX + PanelW * 0.5f;
	DrawTextCentered(TEXT("PAUSED"), PacHUD::Sunny, MidX, PY + T, TextScale * 1.3f);
	const TCHAR* const Items[] = { TEXT("RESUME"), TEXT("RESTART"), TEXT("SETTINGS"), TEXT("QUIT TO MENU") };
	static_assert(UE_ARRAY_COUNT(Items) == APacSharkGameMode::Pause_Count, "one label per pause item");
	float Y = PY + T * 4.f;
	for (int32 Item = 0; Item < APacSharkGameMode::Pause_Count; ++Item)
	{
		const bool bSelected = Item == GameMode.GetPauseSelection();
		const FString Label = bSelected ? FString::Printf(TEXT("> %s <"), Items[Item]) : FString(Items[Item]);
		DrawTextCentered(Label, bSelected ? PacHUD::Sunny : PacHUD::White, MidX, Y, TextScale);
		AddTextTouchZone(TEXT("> QUIT TO MENU <"), MidX, Y, TextScale, EPacTouch::PauseItem, Item);
		Y += T * 2.f;
	}
	DrawTextCentered(GameMode.IsUsingTouch() ? TEXT("TAP TO SELECT") : GameMode.IsUsingGamepad() ? TEXT("A SELECT   START RESUME") : TEXT("ENTER SELECT   P RESUME"),
		PacHUD::Seafoam, MidX, PY + PanelH - T * 1.6f, TextScale * 0.55f);
}

void APacSharkHUD::DrawNameEntry(const APacSharkGameMode& GameMode, float ScreenW, float ScreenH, float TextScale)
{
	const float T = TileSize;
	const float PanelW = T * 22.f;
	const bool bTouch = GameMode.IsUsingTouch();
	const float PanelH = T * (bTouch ? 20.f : 15.f);
	float PX = 0.f, PY = 0.f;
	DrawPanel(ScreenW, ScreenH, PanelW, PanelH, PX, PY);

	const float MidX = PX + PanelW * 0.5f;
	DrawTextCentered(TEXT("NEW HIGH SCORE!"), PacHUD::Sunny, MidX, PY + T, TextScale * 1.3f);
	DrawTextCentered(FString::FromInt(GameMode.GetGame().GetScore()), PacHUD::White, MidX, PY + T * 3.2f, TextScale * 1.1f);
	DrawTextCentered(TEXT("ENTER YOUR INITIALS"), PacHUD::Seafoam, MidX, PY + T * 5.4f, TextScale * 0.7f);

	// Three letter boxes; the one being edited is highlighted and blinks.
	const FString& Name = GameMode.GetNameEntry();
	const float BoxW = T * 2.6f;
	const float BoxH = T * 3.f;
	const float Gap = T * 0.8f;
	const float TotalW = APacSharkGameMode::NameLength * BoxW + (APacSharkGameMode::NameLength - 1) * Gap;
	const float BoxY = PY + T * (bTouch ? 8.5f : 7.f);
	for (int32 i = 0; i < APacSharkGameMode::NameLength; ++i)
	{
		const float BoxX = FMath::RoundToFloat(MidX - TotalW * 0.5f + i * (BoxW + Gap));
		const bool bCurrent = i == GameMode.GetNameCursor();
		DrawRect(bCurrent ? PacHUD::Sunny : PacHUD::BubbleColor, BoxX, BoxY + BoxH - T * 0.25f, BoxW, T * 0.25f);
		if (bCurrent)
		{
			DrawRect(FLinearColor(1.f, 0.85f, 0.3f, 0.15f), BoxX, BoxY, BoxW, BoxH);
		}
		const FString Letter = Name.IsValidIndex(i) ? Name.Mid(i, 1) : FString(TEXT(" "));
		const bool bShow = !bCurrent || FMath::Frac(TimeSeconds * 2.f) < 0.75f;
		if (bShow)
		{
			DrawTextCentered(Letter, bCurrent ? PacHUD::Sunny : PacHUD::White, BoxX + BoxW * 0.5f, BoxY + T * 0.3f, TextScale * 2.f);
		}
		AddTouchZone(BoxX, BoxY, BoxW, BoxH, EPacTouch::NameBox, i);
		if (bTouch)
		{
			// Arrows above and below each letter step it through the alphabet.
			const float ArrowSize = T * 0.9f;
			DrawArrow(FVector2D(BoxX + BoxW * 0.5f, BoxY - T * 0.9f), ArrowSize, true, PacHUD::Seafoam);
			DrawArrow(FVector2D(BoxX + BoxW * 0.5f, BoxY + BoxH + T * 0.9f), ArrowSize, false, PacHUD::Seafoam);
			AddTouchZone(BoxX, BoxY - T * 2.f, BoxW, T * 2.f, EPacTouch::NameUp, i);
			AddTouchZone(BoxX, BoxY + BoxH, BoxW, T * 2.f, EPacTouch::NameDown, i);
		}
	}

	if (bTouch)
	{
		const float DoneY = BoxY + BoxH + T * 2.6f;
		DrawTextCentered(TEXT("> DONE <"), PacHUD::Sunny, MidX, DoneY, TextScale * 1.1f);
		AddTextTouchZone(TEXT("> DONE <"), MidX, DoneY, TextScale * 1.1f, EPacTouch::NameDone);
	}

	DrawTextCentered(bTouch ? TEXT("TAP THE ARROWS TO PICK LETTERS") : GameMode.IsUsingGamepad() ? TEXT("UP/DOWN LETTER   LEFT/RIGHT MOVE   A NEXT / SAVE")
		: TEXT("TYPE OR UP/DOWN   LEFT/RIGHT MOVE   ENTER NEXT / SAVE"),
		PacHUD::Seafoam, MidX, PY + PanelH - T * 1.6f, TextScale * 0.5f);
}

const FPacTouchZone* APacSharkHUD::FindTouchZone(const FVector2D& Point) const
{
	for (int32 i = TouchZones.Num() - 1; i >= 0; --i)
	{
		if (TouchZones[i].Box.IsInside(Point))
		{
			return &TouchZones[i];
		}
	}
	return nullptr;
}

const FPacTouchZone* APacSharkHUD::FindTouchZone(EPacTouch Action, int32 Index) const
{
	return TouchZones.FindByPredicate([Action, Index](const FPacTouchZone& Zone) { return Zone.Action == Action && Zone.Index == Index; });
}

void APacSharkHUD::AddTouchZone(float X, float Y, float W, float H, EPacTouch Action, int32 Index)
{
	FPacTouchZone& Zone = TouchZones.AddDefaulted_GetRef();
	Zone.Box = FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H));
	Zone.Action = Action;
	Zone.Index = Index;
}

void APacSharkHUD::AddTextTouchZone(const FString& Text, float CenterX, float TopY, float Scale, EPacTouch Action, int32 Index)
{
	// Sized from the widest label in the list (passed in as Text) plus a finger's width of padding.
	float W = 0.f, H = 0.f;
	TextSize(Text, W, H, Scale);
	const float Pad = TileSize * 0.6f;
	AddTouchZone(CenterX - W * 0.5f - Pad, TopY - Pad * 0.5f, W + 2.f * Pad, H + Pad, Action, Index);
}

void APacSharkHUD::DrawPauseButton(float ScreenW)
{
	// A round "II" button in the top-right corner, for touch screens.
	const float R = TileSize * 1.1f;
	const FVector2D Center(ScreenW - R * 1.6f, R * 1.6f);
	DrawCircle(Center, R, FLinearColor(0.f, 0.05f, 0.1f, 0.6f), 24);
	const float BarW = R * 0.25f;
	const float BarH = R * 0.9f;
	DrawRect(PacHUD::White, Center.X - BarW * 1.5f, Center.Y - BarH * 0.5f, BarW, BarH);
	DrawRect(PacHUD::White, Center.X + BarW * 0.5f, Center.Y - BarH * 0.5f, BarW, BarH);
	AddTouchZone(Center.X - R * 1.5f, Center.Y - R * 1.5f, R * 3.f, R * 3.f, EPacTouch::PauseButton);
}

void APacSharkHUD::DrawArrow(const FVector2D& Center, float Size, bool bUp, const FLinearColor& Color)
{
	TArray<FCanvasUVTri> Tris;
	const float Dir = bUp ? -1.f : 1.f;
	PacHUD::AddTri(Tris, Center + FVector2D(0.f, Dir * Size * 0.5f), Center + FVector2D(-Size * 0.6f, -Dir * Size * 0.4f),
		Center + FVector2D(Size * 0.6f, -Dir * Size * 0.4f), Color);
	Canvas->K2_DrawTriangle(nullptr, Tris);
}

void APacSharkHUD::DrawCircle(const FVector2D& Center, float Radius, const FLinearColor& Color, int32 Sides)
{
	Canvas->K2_DrawPolygon(nullptr, Center, FVector2D(Radius, Radius), Sides, Color);
}

void APacSharkHUD::DrawTextCentered(const FString& Text, const FLinearColor& Color, float CenterX, float TopY, float Scale)
{
	float W = 0.f, H = 0.f;
	TextSize(Text, W, H, Scale);
	DrawCrispText(Text, Color, CenterX - W * 0.5f, TopY, Scale);
}

void APacSharkHUD::DrawTextLeft(const FString& Text, const FLinearColor& Color, float X, float TopY, float Scale)
{
	DrawCrispText(Text, Color, X, TopY, Scale);
}

float APacSharkHUD::CrispFontScale(float Scale) const
{
	return TextUnitHeight * Scale / CrispFontUnitHeight;
}

void APacSharkHUD::TextSize(const FString& Text, float& OutW, float& OutH, float Scale) const
{
	if (!CrispFont || CrispFontUnitHeight <= 0.f)
	{
		GetTextSize(Text, OutW, OutH, GEngine->GetMediumFont(), Scale);
		return;
	}
	GetTextSize(Text, OutW, OutH, CrispFont, CrispFontScale(Scale));
}

void APacSharkHUD::DrawCrispText(const FString& Text, const FLinearColor& Color, float X, float TopY, float Scale)
{
	// Scaling the engine's small bitmap font up makes it blurry, so draw with the distance field font,
	// which keeps sharp edges at any size, snapped to whole pixels.
	if (!CrispFont || CrispFontUnitHeight <= 0.f)
	{
		DrawText(Text, Color, X, TopY, GEngine->GetMediumFont(), Scale);
		return;
	}
	DrawText(Text, Color, FMath::RoundToFloat(X), FMath::RoundToFloat(TopY), CrispFont, CrispFontScale(Scale));
}

UTexture2D* APacSharkHUD::LoadArtTexture(const FString& Path)
{
	FImage Image;
	if (!FImageUtils::LoadImage(*Path, Image))
	{
		UE_LOG(LogTemp, Warning, TEXT("Pac Shark: could not read image %s"), *Path);
		return nullptr;
	}
	Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	const int32 Width = Image.SizeX;
	const int32 Height = Image.SizeY;
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Width * Height);
	FMemory::Memcpy(Pixels.GetData(), Image.RawData.GetData(), Pixels.Num() * sizeof(FColor));

	// Spread edge colors into the fully transparent pixels so filtering never pulls in a dark or white fringe.
	TArray<bool> Filled;
	Filled.SetNumUninitialized(Pixels.Num());
	for (int32 i = 0; i < Pixels.Num(); ++i)
	{
		Filled[i] = Pixels[i].A != 0;
	}
	for (int32 Pass = 0; Pass < 4; ++Pass)
	{
		TArray<FColor> Next = Pixels;
		TArray<bool> NextFilled = Filled;
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				if (Filled[Y * Width + X])
				{
					continue;
				}
				int32 R = 0, G = 0, B = 0, Count = 0;
				for (int32 NY = FMath::Max(0, Y - 1); NY <= FMath::Min(Height - 1, Y + 1); ++NY)
				{
					for (int32 NX = FMath::Max(0, X - 1); NX <= FMath::Min(Width - 1, X + 1); ++NX)
					{
						if (Filled[NY * Width + NX])
						{
							const FColor& N = Pixels[NY * Width + NX];
							R += N.R;
							G += N.G;
							B += N.B;
							++Count;
						}
					}
				}
				if (Count > 0)
				{
					Next[Y * Width + X] = FColor(R / Count, G / Count, B / Count, 0);
					NextFilled[Y * Width + X] = true;
				}
			}
		}
		Pixels = MoveTemp(Next);
		Filled = MoveTemp(NextFilled);
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture)
	{
		return nullptr;
	}
	Texture->SRGB = true;
	Texture->Filter = TF_Trilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;

	// Build a full mip chain (alpha-weighted box filter) so sprites drawn much smaller than the file stay smooth
	// instead of shimmering.
	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	int32 MipW = Width, MipH = Height;
	for (int32 MipIndex = 0;; ++MipIndex)
	{
		FTexture2DMipMap* Mip = nullptr;
		if (MipIndex == 0)
		{
			Mip = &PlatformData->Mips[0];
		}
		else
		{
			Mip = new FTexture2DMipMap();
			Mip->SizeX = MipW;
			Mip->SizeY = MipH;
			Mip->SizeZ = 1;
			PlatformData->Mips.Add(Mip);
		}
		Mip->BulkData.Lock(LOCK_READ_WRITE);
		void* Data = Mip->BulkData.Realloc(int64(MipW) * MipH * sizeof(FColor));
		FMemory::Memcpy(Data, Pixels.GetData(), int64(MipW) * MipH * sizeof(FColor));
		Mip->BulkData.Unlock();

		if (MipW == 1 && MipH == 1)
		{
			break;
		}
		const int32 NextW = FMath::Max(1, MipW / 2);
		const int32 NextH = FMath::Max(1, MipH / 2);
		TArray<FColor> Smaller;
		Smaller.SetNumUninitialized(NextW * NextH);
		for (int32 Y = 0; Y < NextH; ++Y)
		{
			for (int32 X = 0; X < NextW; ++X)
			{
				float R = 0.f, G = 0.f, B = 0.f, A = 0.f, PlainR = 0.f, PlainG = 0.f, PlainB = 0.f;
				int32 Count = 0;
				for (int32 DY = 0; DY < 2; ++DY)
				{
					for (int32 DX = 0; DX < 2; ++DX)
					{
						const int32 SX = FMath::Min(X * 2 + DX, MipW - 1);
						const int32 SY = FMath::Min(Y * 2 + DY, MipH - 1);
						const FColor& P = Pixels[SY * MipW + SX];
						R += P.R * P.A; G += P.G * P.A; B += P.B * P.A; A += P.A;
						PlainR += P.R; PlainG += P.G; PlainB += P.B;
						++Count;
					}
				}
				FColor& Out = Smaller[Y * NextW + X];
				if (A > 0.f)
				{
					Out = FColor(FMath::RoundToInt(R / A), FMath::RoundToInt(G / A), FMath::RoundToInt(B / A), FMath::RoundToInt(A / Count));
				}
				else
				{
					Out = FColor(FMath::RoundToInt(PlainR / Count), FMath::RoundToInt(PlainG / Count), FMath::RoundToInt(PlainB / Count), 0);
				}
			}
		}
		Pixels = MoveTemp(Smaller);
		MipW = NextW;
		MipH = NextH;
	}

	Texture->UpdateResource();
	return Texture;
}

void APacSharkHUD::DrawTextShadowed(const FString& Text, const FLinearColor& Color, float CenterX, float TopY, float Scale)
{
	const float Offset = FMath::Max(1.f, Scale * 1.5f);
	DrawTextCentered(Text, FLinearColor(0.f, 0.f, 0.f, 0.8f), CenterX + Offset, TopY + Offset, Scale);
	DrawTextCentered(Text, Color, CenterX, TopY, Scale);
}
