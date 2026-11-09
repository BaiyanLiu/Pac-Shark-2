// Pac Shark - game mode that owns and ticks the Pac-Man simulation, plus the title, pause, settings and
// high score screens, sound and music. Every screen works with the keyboard or a gamepad.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PacGame.h"
#include "PacSharkGameMode.generated.h"

class APlayerController;
class UAudioComponent;
class UFileMediaSource;
class UMediaPlayer;
class UMediaSoundComponent;
class UMediaTexture;
class UTexture;
class USoundWaveProcedural;

struct FPacHighScore
{
	FString Name;
	int32 Score = 0;
};

UCLASS()
class PACSHARK_API APacSharkGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APacSharkGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	const FPacGame& GetGame() const { return Game; }

	// Sprites, music, sounds and the intro video live as plain files in <Project>/Art, so the editor never
	// imports them and the packaged builds ship them as loose files.
	static FString GetArtPath(const FString& FileName);

	// Title screen shown at startup.
	enum EMenuItem : int32
	{
		Menu_Play,
		Menu_Settings,
		Menu_Quit,
		Menu_Count
	};

	// Pause menu, opened with P, Escape or the gamepad's Start button during a game.
	enum EPauseItem : int32
	{
		Pause_Resume,
		Pause_Restart,
		Pause_Settings,
		Pause_QuitToMenu,
		Pause_Count
	};

	// Settings screen. The game stays paused while it is open; BACK returns to wherever it was opened from.
	enum ESettingsItem : int32
	{
		Setting_Difficulty,
		Setting_OctopusSpeed,
		Setting_OctopusCount,
		Setting_PowerDots,
		Setting_ResetScores,
		Setting_Back,
		Setting_Count
	};
	static constexpr int32 MinOctopusSpeed = 1;
	static constexpr int32 MaxOctopusSpeed = 10;
	static constexpr int32 DefaultOctopusSpeed = 5;
	static constexpr int32 DefaultOctopusCount = 4;
	static constexpr int32 DefaultPowerDots = 4;

	// Difficulty presets set all three sliders at once. Any other combination shows as CUSTOM.
	enum EDifficulty : int32
	{
		Difficulty_Easy,
		Difficulty_Normal,
		Difficulty_Hard,
		Difficulty_Count,
		Difficulty_Custom = Difficulty_Count
	};
	int32 GetDifficulty() const;

	// Slider items: Setting_OctopusSpeed, Setting_OctopusCount and Setting_PowerDots.
	static bool IsSlider(int32 Item) { return Item >= Setting_OctopusSpeed && Item <= Setting_PowerDots; }
	int32 GetSliderValue(int32 Item) const;
	static int32 GetSliderMin(int32 Item);
	static int32 GetSliderMax(int32 Item);

	// Intro video played at startup before the title screen, on the first launch only.
	bool IsInIntro() const { return bInIntro; }
	UTexture* GetIntroTexture() const;
	float GetIntroElapsed() const { return IntroElapsed; }

	bool IsInMainMenu() const { return bInMainMenu; }
	int32 GetMenuSelection() const { return MenuSelection; }
	bool IsPaused() const { return bPaused; }
	int32 GetPauseSelection() const { return PauseSelection; }
	bool IsSettingsOpen() const { return bSettingsOpen; }
	int32 GetSettingsSelection() const { return SettingsSelection; }
	// RESET HIGH SCORES asks to be pressed a second time before it clears the table.
	bool IsResetScoresArmed() const { return bResetScoresArmed; }

	// High score table (best first), saved between runs. After a game that makes the table the player
	// enters three initials before it is saved.
	static constexpr int32 MaxHighScores = 10;
	static constexpr int32 NameLength = 3;
	const TArray<FPacHighScore>& GetHighScores() const { return HighScores; }
	int32 GetLastHighScoreRank() const { return LastHighScoreRank; }
	bool IsEnteringName() const { return bEnteringName; }
	const FString& GetNameEntry() const { return NameEntry; }
	int32 GetNameCursor() const { return NameCursor; }

	// True when the last input came from a gamepad, so on-screen hints can name its buttons.
	bool IsUsingGamepad() const { return bUsingGamepad; }
	// True on phones and tablets, or once the screen has been touched: shows the pause button and touch hints.
	bool IsUsingTouch() const { return bUsingTouch; }

private:
	void HandleInput();
	void HandleGameInput(APlayerController* PC);
	void HandleMenuInput(APlayerController* PC);
	void HandlePauseInput(APlayerController* PC);
	void HandleSettingsInput(APlayerController* PC);
	void HandleNameEntryInput(APlayerController* PC);
	void HandleTouch(APlayerController* PC);
	void HandleTap(APlayerController* PC, const FVector2D& Point);
	void ActivateMenuSelection(APlayerController* PC);
	void ActivatePauseSelection();
	void StepSetting(int32 Step);
	void SelectSetting(int32 Item);
	void ActivateResetScores();
	void CycleNameLetter(int32 Step);
	void OpenSettings();
	void CloseSettings();
	void SetOctopusSpeed(int32 NewSpeed);
	void SetSliderValue(int32 Item, int32 Value);
	void SaveSettings();
	void ApplyDifficulty(int32 Difficulty);
	void StartGame();
	void ReturnToMainMenu();

	void LoadHighScores();
	void SaveHighScores();
	bool QualifiesForHighScore(int32 Score) const;
	void FinishNameEntry();

	void LoadMusic();
	void UpdateMusic();
	void LoadSounds();
	void PlaySound(FName Name, float Volume = 1.f, float Pitch = 1.f);
	void PlayGameEventSounds();
	void UpdateSounds();

	void StartIntro();
	void UpdateIntro(float DeltaSeconds);
	void FinishIntro();

	FPacGame Game;

	bool bInMainMenu = true;
	int32 MenuSelection = Menu_Play;
	bool bPaused = false;
	int32 PauseSelection = Pause_Resume;
	bool bSettingsOpen = false;
	int32 SettingsSelection = Setting_Difficulty;
	bool bResetScoresArmed = false;
	int32 OctopusSpeed = DefaultOctopusSpeed;
	int32 OctopusCount = DefaultOctopusCount;
	int32 PowerDots = DefaultPowerDots;
	bool bUsingGamepad = false;
	bool bUsingTouch = PLATFORM_ANDROID || PLATFORM_IOS;

	// Touch: swipes steer the shark, taps press whatever the HUD drew under the finger, and a press on a
	// slider drags it.
	bool bTouchDown = false;
	bool bTouchMoved = false;
	FVector2D TouchOrigin = FVector2D::ZeroVector;
	FVector2D TouchSwipeStart = FVector2D::ZeroVector;
	FVector2D TouchLast = FVector2D::ZeroVector;
	FVector2D TouchCanvasOffset = FVector2D::ZeroVector;
	int32 DraggingSlider = -1;

	TArray<FPacHighScore> HighScores;
	int32 LastHighScoreRank = -1;
	bool bEnteringName = false;
	FString NameEntry;
	int32 NameCursor = 0;
	EPacGamePhase LastPhase = EPacGamePhase::GameOver;

	// Background music, streamed from <Project>/Art/GameMusic.wav and looped during play.
	UPROPERTY(Transient)
	TObjectPtr<USoundWaveProcedural> MusicWave;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicComponent;

	TArray<uint8> MusicPCM;

	// Sound effects from <Project>/Art/Sfx_*.wav: raw 16-bit samples, played through a fresh procedural wave each time.
	struct FPacSound
	{
		TArray<uint8> PCM;
		int32 SampleRate = 44100;
		int32 Channels = 1;
		float Duration = 0.f;
	};
	TMap<FName, FPacSound> Sounds;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> ActiveSounds;

	TArray<double> ActiveSoundEnds;
	bool bChompHigh = false;

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> IntroPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> IntroTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMediaSoundComponent> IntroSound;

	UPROPERTY(Transient)
	TObjectPtr<UFileMediaSource> IntroSource;

	bool bInIntro = false;
	bool bIntroStarted = false;
	float IntroElapsed = 0.f;
};
