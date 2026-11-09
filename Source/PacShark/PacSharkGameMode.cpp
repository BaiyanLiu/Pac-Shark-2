// Pac Shark - game mode that owns and ticks the Pac-Man simulation, plus the title, pause, settings and
// high score screens, sound and music.

#include "PacSharkGameMode.h"
#include "PacSharkHUD.h"
#include "Audio.h"
#include "Components/AudioComponent.h"
#include "FileMediaSource.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "MediaTexture.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "Sound/SoundWaveProcedural.h"

namespace PacSettings
{
	const TCHAR* const Section = TEXT("PacShark");
	const TCHAR* const SliderKeys[] = { TEXT(""), TEXT("OctopusSpeed"), TEXT("OctopusCount"), TEXT("PowerDots") };
	const TCHAR* const HighScoresKey = TEXT("HighScores");
	const TCHAR* const LastNameKey = TEXT("LastName");
	const TCHAR* const IntroSeenKey = TEXT("IntroSeen");

	// Octopus speed per slider step, as a multiple of arcade speed. 5 is arcade speed; the low end is much gentler.
	const float SpeedScales[] = { 0.25f, 0.4f, 0.55f, 0.75f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f };

	// Easy, Normal, Hard as { octopus speed, octopi, power dots }.
	const int32 Presets[][3] = { { 3, 2, 6 }, { 5, 4, 4 }, { 8, 6, 2 } };

	const TCHAR* const NameChars = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ");
}

namespace PacInput
{
	// True if any of the keys was just pressed. Remembers whether that came from a gamepad so hints can match.
	bool Pressed(APlayerController* PC, std::initializer_list<FKey> Keys, bool& bUsingGamepad)
	{
		for (const FKey& Key : Keys)
		{
			if (PC->WasInputKeyJustPressed(Key))
			{
				bUsingGamepad = Key.IsGamepadKey();
				return true;
			}
		}
		return false;
	}

	bool Up(APlayerController* PC, bool& bPad) { return Pressed(PC, { EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up, EKeys::Gamepad_LeftStick_Up }, bPad); }
	bool Down(APlayerController* PC, bool& bPad) { return Pressed(PC, { EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_LeftStick_Down }, bPad); }
	bool Left(APlayerController* PC, bool& bPad) { return Pressed(PC, { EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_LeftStick_Left }, bPad); }
	bool Right(APlayerController* PC, bool& bPad) { return Pressed(PC, { EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_LeftStick_Right }, bPad); }
	bool Confirm(APlayerController* PC, bool& bPad) { return Pressed(PC, { EKeys::Enter, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom }, bPad); }
	bool Back(APlayerController* PC, bool& bPad) { return Pressed(PC, { EKeys::BackSpace, EKeys::Gamepad_FaceButton_Right }, bPad); }
	bool Pause(APlayerController* PC, bool& bPad) { return Pressed(PC, { EKeys::P, EKeys::Escape, EKeys::Gamepad_Special_Right }, bPad); }
	bool Settings(APlayerController* PC, bool& bPad) { return Pressed(PC, { EKeys::Tab, EKeys::Gamepad_Special_Left }, bPad); }
}

FString APacSharkGameMode::GetArtPath(const FString& FileName)
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Art"), FileName);
}

APacSharkGameMode::APacSharkGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Everything is drawn by the HUD, so the player only needs a bare pawn that ignores input.
	DefaultPawnClass = APawn::StaticClass();
	HUDClass = APacSharkHUD::StaticClass();
}

void APacSharkGameMode::BeginPlay()
{
	Super::BeginPlay();

	const int32 Defaults[] = { 0, DefaultOctopusSpeed, DefaultOctopusCount, DefaultPowerDots };
	for (int32 Item = Setting_OctopusSpeed; Item <= Setting_PowerDots; ++Item)
	{
		int32 Saved = Defaults[Item];
		if (GConfig)
		{
			GConfig->GetInt(PacSettings::Section, PacSettings::SliderKeys[Item], Saved, GGameUserSettingsIni);
		}
		SetSliderValue(Item, Saved);
	}
	LoadHighScores();

	// Set up a board so the HUD has something valid to read; play starts from the title screen.
	Game.NewGame();
	LastPhase = Game.GetPhase();
	bInMainMenu = true;

	// Without a limit the game redraws hundreds of times a second for no visible gain, keeping the
	// GPU and fans busy. Cap it unless a limit was already chosen.
	if (IConsoleVariable* MaxFPS = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
	{
		if (MaxFPS->GetFloat() <= 0.f)
		{
			MaxFPS->Set(60.f, ECVF_SetByGameSetting);
		}
	}

	LoadMusic();
	LoadSounds();
	StartIntro();
}

void APacSharkGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	HandleInput();
	UpdateIntro(DeltaSeconds);
	if (!bInMainMenu && !bSettingsOpen && !bPaused && !bEnteringName)
	{
		Game.Tick(DeltaSeconds);
	}

	// A finished game that makes the table asks for initials before going back to the title screen.
	const EPacGamePhase Phase = Game.GetPhase();
	if (!bInMainMenu && Phase == EPacGamePhase::GameOver && LastPhase != EPacGamePhase::GameOver)
	{
		LastHighScoreRank = -1;
		if (QualifiesForHighScore(Game.GetScore()))
		{
			bEnteringName = true;
			NameCursor = 0;
			NameEntry = TEXT("AAA");
			if (GConfig)
			{
				GConfig->GetString(PacSettings::Section, PacSettings::LastNameKey, NameEntry, GGameUserSettingsIni);
			}
			NameEntry = NameEntry.Left(NameLength).RightPad(NameLength);
		}
	}
	LastPhase = Phase;

	PlayGameEventSounds();
	UpdateSounds();
	UpdateMusic();
}

void APacSharkGameMode::StartIntro()
{
	bool bIntroSeen = false;
	if (GConfig)
	{
		GConfig->GetBool(PacSettings::Section, PacSettings::IntroSeenKey, bIntroSeen, GGameUserSettingsIni);
	}
	if (bIntroSeen)
	{
		return;
	}

	const FString Path = FPaths::ConvertRelativePathToFull(GetArtPath(TEXT("Intro.mp4")));
	if (!FPaths::FileExists(Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("Pac Shark: intro video not found: %s"), *Path);
		return;
	}

	IntroPlayer = NewObject<UMediaPlayer>(this);
	IntroPlayer->PlayOnOpen = true;
	IntroPlayer->SetLooping(false);

	IntroTexture = NewObject<UMediaTexture>(this);
	IntroTexture->AutoClear = true;
	IntroTexture->ClearColor = FLinearColor::Black;
	IntroTexture->SetMediaPlayer(IntroPlayer);
	IntroTexture->UpdateResource();

	IntroSound = NewObject<UMediaSoundComponent>(this);
	IntroSound->bAllowSpatialization = false;
	IntroSound->SetMediaPlayer(IntroPlayer);
	IntroSound->RegisterComponent();
	IntroSound->Start();

	IntroSource = NewObject<UFileMediaSource>(this);
	IntroSource->SetFilePath(Path);

	bInIntro = IntroPlayer->OpenSource(IntroSource);
	bIntroStarted = false;
	IntroElapsed = 0.f;
}

void APacSharkGameMode::UpdateIntro(float DeltaSeconds)
{
	if (!bInIntro)
	{
		return;
	}
	IntroElapsed += DeltaSeconds;
	if (IntroPlayer->IsPlaying())
	{
		bIntroStarted = true;
	}

	// Finished once it has played and stopped. If it never starts (no video decoder available),
	// give up after a few seconds and go to the title screen.
	const bool bEnded = bIntroStarted && !IntroPlayer->IsPlaying() && !IntroPlayer->IsBuffering() && !IntroPlayer->IsPreparing();
	const bool bFailed = !bIntroStarted && IntroElapsed > 5.f;
	if (bEnded || bFailed)
	{
		FinishIntro();
	}
}

void APacSharkGameMode::FinishIntro()
{
	if (bInIntro && GConfig)
	{
		GConfig->SetBool(PacSettings::Section, PacSettings::IntroSeenKey, true, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	bInIntro = false;
	if (IntroPlayer)
	{
		IntroPlayer->Close();
	}
	if (IntroSound)
	{
		IntroSound->Stop();
	}
}

UTexture* APacSharkGameMode::GetIntroTexture() const
{
	return IntroTexture;
}

void APacSharkGameMode::LoadMusic()
{
	// The track ships as a plain 16-bit PCM .wav next to the sprites, so no editor import is needed.
	const FString Path = GetArtPath(TEXT("GameMusic.wav"));
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("Pac Shark: music file not found: %s"), *Path);
		return;
	}

	FWaveModInfo WaveInfo;
	if (!WaveInfo.ReadWaveInfo(FileData.GetData(), FileData.Num()) || *WaveInfo.pBitsPerSample != 16)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pac Shark: %s is not a 16-bit PCM wav"), *Path);
		return;
	}
	MusicPCM.Append(WaveInfo.SampleDataStart, WaveInfo.SampleDataSize);

	MusicWave = NewObject<USoundWaveProcedural>(this);
	MusicWave->SetSampleRate(*WaveInfo.pSamplesPerSec);
	MusicWave->NumChannels = *WaveInfo.pChannels;
	MusicWave->Duration = INDEFINITELY_LOOPING_DURATION;
	MusicWave->SoundGroup = SOUNDGROUP_Music;
	MusicWave->bLooping = false;

	MusicComponent = UGameplayStatics::CreateSound2D(this, MusicWave, 1.f, 1.f, 0.f, nullptr, false, false);
}

void APacSharkGameMode::UpdateMusic()
{
	if (!MusicComponent || !MusicWave || MusicPCM.Num() == 0)
	{
		return;
	}

	// Plays during a game and pauses on the title screen, in menus and once the game is over.
	const bool bShouldPlay = !bInMainMenu && !bSettingsOpen && !bPaused && !bEnteringName && !bInIntro
		&& Game.GetPhase() != EPacGamePhase::GameOver;
	if (bShouldPlay && !MusicComponent->IsPlaying())
	{
		MusicComponent->Play();
	}
	MusicComponent->SetPaused(!bShouldPlay);

	// Loop by keeping at least one copy of the track queued ahead of playback.
	if (MusicWave->GetAvailableAudioByteCount() < MusicPCM.Num())
	{
		MusicWave->QueueAudio(MusicPCM.GetData(), MusicPCM.Num());
	}
}

void APacSharkGameMode::LoadSounds()
{
	const TCHAR* const Names[] = { TEXT("Chomp"), TEXT("Power"), TEXT("EatOctopus"), TEXT("Death"), TEXT("LevelClear"),
		TEXT("ExtraLife"), TEXT("MenuMove"), TEXT("MenuSelect") };
	for (const TCHAR* Name : Names)
	{
		const FString Path = GetArtPath(FString::Printf(TEXT("Sfx_%s.wav"), Name));
		TArray<uint8> FileData;
		if (!FFileHelper::LoadFileToArray(FileData, *Path))
		{
			UE_LOG(LogTemp, Warning, TEXT("Pac Shark: sound file not found: %s"), *Path);
			continue;
		}
		FWaveModInfo WaveInfo;
		if (!WaveInfo.ReadWaveInfo(FileData.GetData(), FileData.Num()) || *WaveInfo.pBitsPerSample != 16)
		{
			UE_LOG(LogTemp, Warning, TEXT("Pac Shark: %s is not a 16-bit PCM wav"), *Path);
			continue;
		}
		FPacSound& Sound = Sounds.Add(FName(Name));
		Sound.PCM.Append(WaveInfo.SampleDataStart, WaveInfo.SampleDataSize);
		Sound.SampleRate = *WaveInfo.pSamplesPerSec;
		Sound.Channels = *WaveInfo.pChannels;
		Sound.Duration = float(Sound.PCM.Num()) / float(FMath::Max(1, Sound.SampleRate * Sound.Channels * 2));
	}
}

void APacSharkGameMode::PlaySound(FName Name, float Volume, float Pitch)
{
	const FPacSound* Sound = Sounds.Find(Name);
	if (!Sound || Sound->PCM.Num() == 0)
	{
		return;
	}

	// A procedural wave plays exactly the samples queued on it, so each play gets its own short-lived wave.
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(Sound->SampleRate);
	Wave->NumChannels = Sound->Channels;
	Wave->Duration = Sound->Duration;
	Wave->SoundGroup = SOUNDGROUP_Effects;
	Wave->bLooping = false;
	Wave->QueueAudio(Sound->PCM.GetData(), Sound->PCM.Num());

	if (UAudioComponent* Component = UGameplayStatics::SpawnSound2D(this, Wave, Volume, Pitch))
	{
		ActiveSounds.Add(Component);
		ActiveSoundEnds.Add(GetWorld()->GetRealTimeSeconds() + Sound->Duration / FMath::Max(0.1f, Pitch) + 0.1);
	}
}

void APacSharkGameMode::UpdateSounds()
{
	// Procedural waves never report that they finished, so stop each one once its samples have played.
	const double Now = GetWorld()->GetRealTimeSeconds();
	for (int32 i = ActiveSounds.Num() - 1; i >= 0; --i)
	{
		if (Now >= ActiveSoundEnds[i] || !IsValid(ActiveSounds[i]))
		{
			if (IsValid(ActiveSounds[i]))
			{
				ActiveSounds[i]->Stop();
			}
			ActiveSounds.RemoveAt(i);
			ActiveSoundEnds.RemoveAt(i);
		}
	}
}

void APacSharkGameMode::PlayGameEventSounds()
{
	for (EPacEvent Event : Game.TakeEvents())
	{
		switch (Event)
		{
		case EPacEvent::Pellet:
			// Alternate two pitches for the classic "waka" rhythm.
			bChompHigh = !bChompHigh;
			PlaySound(TEXT("Chomp"), 0.3f, bChompHigh ? 1.15f : 1.f);
			break;
		case EPacEvent::PowerPellet:
			PlaySound(TEXT("Power"));
			break;
		case EPacEvent::GhostEaten:
			PlaySound(TEXT("EatOctopus"));
			break;
		case EPacEvent::Death:
			PlaySound(TEXT("Death"));
			break;
		case EPacEvent::Hit:
			// A shorter, higher take on the death sound: the shark keeps swimming.
			PlaySound(TEXT("Death"), 0.7f, 1.6f);
			break;
		case EPacEvent::LevelClear:
			PlaySound(TEXT("LevelClear"));
			break;
		case EPacEvent::ExtraLife:
			PlaySound(TEXT("ExtraLife"));
			break;
		default:
			break;
		}
	}
}

void APacSharkGameMode::LoadHighScores()
{
	HighScores.Reset();
	TArray<FString> Lines;
	if (GConfig)
	{
		GConfig->GetArray(PacSettings::Section, PacSettings::HighScoresKey, Lines, GGameUserSettingsIni);
	}
	for (const FString& Line : Lines)
	{
		FString Name, ScoreText;
		if (Line.Split(TEXT(","), &Name, &ScoreText) && ScoreText.IsNumeric())
		{
			FPacHighScore Entry;
			Entry.Name = Name.Left(NameLength);
			Entry.Score = FCString::Atoi(*ScoreText);
			HighScores.Add(Entry);
		}
	}
	HighScores.Sort([](const FPacHighScore& A, const FPacHighScore& B) { return A.Score > B.Score; });
	if (HighScores.Num() > MaxHighScores)
	{
		HighScores.SetNum(MaxHighScores);
	}
	if (HighScores.Num() > 0)
	{
		Game.SetHighScore(HighScores[0].Score);
	}
}

void APacSharkGameMode::SaveHighScores()
{
	if (!GConfig)
	{
		return;
	}
	TArray<FString> Lines;
	for (const FPacHighScore& Entry : HighScores)
	{
		Lines.Add(FString::Printf(TEXT("%s,%d"), *Entry.Name, Entry.Score));
	}
	GConfig->SetArray(PacSettings::Section, PacSettings::HighScoresKey, Lines, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

bool APacSharkGameMode::QualifiesForHighScore(int32 Score) const
{
	return Score > 0 && (HighScores.Num() < MaxHighScores || Score > HighScores.Last().Score);
}

void APacSharkGameMode::FinishNameEntry()
{
	FPacHighScore Entry;
	Entry.Name = NameEntry.TrimEnd();
	if (Entry.Name.IsEmpty())
	{
		Entry.Name = TEXT("???");
	}
	Entry.Score = Game.GetScore();

	// Ties go below the existing entry, so the earlier score keeps its place.
	int32 Rank = 0;
	while (Rank < HighScores.Num() && HighScores[Rank].Score >= Entry.Score)
	{
		++Rank;
	}
	HighScores.Insert(Entry, Rank);
	if (HighScores.Num() > MaxHighScores)
	{
		HighScores.SetNum(MaxHighScores);
	}
	SaveHighScores();
	if (GConfig)
	{
		GConfig->SetString(PacSettings::Section, PacSettings::LastNameKey, *NameEntry, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	bEnteringName = false;
	ReturnToMainMenu();
	LastHighScoreRank = Rank;
}

void APacSharkGameMode::StartGame()
{
	Game.NewGame();
	LastPhase = Game.GetPhase();
	bInMainMenu = false;
	bSettingsOpen = false;
	bPaused = false;
	bEnteringName = false;
	LastHighScoreRank = -1;
}

void APacSharkGameMode::ReturnToMainMenu()
{
	bPaused = false;
	bSettingsOpen = false;
	bInMainMenu = true;
	MenuSelection = Menu_Play;
	LastHighScoreRank = -1;
}

void APacSharkGameMode::OpenSettings()
{
	bSettingsOpen = true;
	SettingsSelection = Setting_Difficulty;
	bResetScoresArmed = false;
}

void APacSharkGameMode::CloseSettings()
{
	// Back to whatever was showing underneath: the title screen, the pause menu or the game.
	bSettingsOpen = false;
}

void APacSharkGameMode::SetOctopusSpeed(int32 NewSpeed)
{
	OctopusSpeed = FMath::Clamp(NewSpeed, MinOctopusSpeed, MaxOctopusSpeed);
	// 1 -> 25% of arcade speed, 5 -> arcade speed, 10 -> 150%.
	Game.SetGhostSpeedScale(PacSettings::SpeedScales[OctopusSpeed - MinOctopusSpeed]);
}

int32 APacSharkGameMode::GetSliderValue(int32 Item) const
{
	switch (Item)
	{
	case Setting_OctopusSpeed: return OctopusSpeed;
	case Setting_OctopusCount: return OctopusCount;
	case Setting_PowerDots: return PowerDots;
	default: return 0;
	}
}

int32 APacSharkGameMode::GetSliderMin(int32 Item)
{
	switch (Item)
	{
	case Setting_OctopusSpeed: return MinOctopusSpeed;
	case Setting_OctopusCount: return FPacGame::MinGhosts;
	default: return 0;
	}
}

int32 APacSharkGameMode::GetSliderMax(int32 Item)
{
	switch (Item)
	{
	case Setting_OctopusSpeed: return MaxOctopusSpeed;
	case Setting_OctopusCount: return FPacGame::MaxGhosts;
	case Setting_PowerDots: return FPacGame::MaxPowerPellets;
	default: return 0;
	}
}

void APacSharkGameMode::SetSliderValue(int32 Item, int32 Value)
{
	// Counts apply from the next level or new game; speed applies straight away.
	Value = FMath::Clamp(Value, GetSliderMin(Item), GetSliderMax(Item));
	switch (Item)
	{
	case Setting_OctopusSpeed:
		SetOctopusSpeed(Value);
		break;
	case Setting_OctopusCount:
		OctopusCount = Value;
		Game.SetGhostCount(Value);
		break;
	case Setting_PowerDots:
		PowerDots = Value;
		Game.SetPowerPelletCount(Value);
		break;
	default:
		break;
	}
}

void APacSharkGameMode::SaveSettings()
{
	if (!GConfig)
	{
		return;
	}
	for (int32 Item = Setting_OctopusSpeed; Item <= Setting_PowerDots; ++Item)
	{
		GConfig->SetInt(PacSettings::Section, PacSettings::SliderKeys[Item], GetSliderValue(Item), GGameUserSettingsIni);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
}

int32 APacSharkGameMode::GetDifficulty() const
{
	for (int32 Difficulty = 0; Difficulty < Difficulty_Count; ++Difficulty)
	{
		const int32* Preset = PacSettings::Presets[Difficulty];
		if (OctopusSpeed == Preset[0] && OctopusCount == Preset[1] && PowerDots == Preset[2])
		{
			return Difficulty;
		}
	}
	return Difficulty_Custom;
}

void APacSharkGameMode::ApplyDifficulty(int32 Difficulty)
{
	const int32* Preset = PacSettings::Presets[FMath::Clamp(Difficulty, 0, Difficulty_Count - 1)];
	SetSliderValue(Setting_OctopusSpeed, Preset[0]);
	SetSliderValue(Setting_OctopusCount, Preset[1]);
	SetSliderValue(Setting_PowerDots, Preset[2]);
}

void APacSharkGameMode::HandleInput()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	HandleTouch(PC);

	if (bInIntro)
	{
		if (PacInput::Confirm(PC, bUsingGamepad) || PacInput::Pause(PC, bUsingGamepad) || PacInput::Settings(PC, bUsingGamepad)
			|| PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
		{
			FinishIntro();
		}
		return;
	}

	if (bEnteringName)
	{
		HandleNameEntryInput(PC);
	}
	else if (bSettingsOpen)
	{
		HandleSettingsInput(PC);
	}
	else if (bInMainMenu)
	{
		HandleMenuInput(PC);
	}
	else if (bPaused)
	{
		HandlePauseInput(PC);
	}
	else
	{
		HandleGameInput(PC);
	}
}

void APacSharkGameMode::HandleGameInput(APlayerController* PC)
{
	if (PacInput::Pause(PC, bUsingGamepad))
	{
		bPaused = true;
		PauseSelection = Pause_Resume;
		PlaySound(TEXT("MenuSelect"), 0.7f);
		return;
	}
	if (PacInput::Settings(PC, bUsingGamepad))
	{
		OpenSettings();
		return;
	}

	if (PacInput::Up(PC, bUsingGamepad))
	{
		Game.QueueDirection(EPacDir::Up);
	}
	else if (PacInput::Down(PC, bUsingGamepad))
	{
		Game.QueueDirection(EPacDir::Down);
	}
	else if (PacInput::Left(PC, bUsingGamepad))
	{
		Game.QueueDirection(EPacDir::Left);
	}
	else if (PacInput::Right(PC, bUsingGamepad))
	{
		Game.QueueDirection(EPacDir::Right);
	}

	if (Game.GetPhase() == EPacGamePhase::GameOver && PacInput::Confirm(PC, bUsingGamepad))
	{
		PlaySound(TEXT("MenuSelect"));
		StartGame();
	}
}

void APacSharkGameMode::HandleMenuInput(APlayerController* PC)
{
	if (PacInput::Settings(PC, bUsingGamepad))
	{
		OpenSettings();
		return;
	}
	if (PacInput::Up(PC, bUsingGamepad))
	{
		MenuSelection = (MenuSelection + Menu_Count - 1) % Menu_Count;
		PlaySound(TEXT("MenuMove"));
	}
	else if (PacInput::Down(PC, bUsingGamepad))
	{
		MenuSelection = (MenuSelection + 1) % Menu_Count;
		PlaySound(TEXT("MenuMove"));
	}

	if (PacInput::Confirm(PC, bUsingGamepad))
	{
		ActivateMenuSelection(PC);
	}
}

void APacSharkGameMode::ActivateMenuSelection(APlayerController* PC)
{
	PlaySound(TEXT("MenuSelect"));
	switch (MenuSelection)
	{
	case Menu_Play:
		StartGame();
		break;
	case Menu_Settings:
		OpenSettings();
		break;
	case Menu_Quit:
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
		break;
	default:
		break;
	}
}

void APacSharkGameMode::HandlePauseInput(APlayerController* PC)
{
	if (PacInput::Pause(PC, bUsingGamepad) || PacInput::Back(PC, bUsingGamepad))
	{
		bPaused = false;
		return;
	}
	if (PacInput::Settings(PC, bUsingGamepad))
	{
		OpenSettings();
		return;
	}
	if (PacInput::Up(PC, bUsingGamepad))
	{
		PauseSelection = (PauseSelection + Pause_Count - 1) % Pause_Count;
		PlaySound(TEXT("MenuMove"));
	}
	else if (PacInput::Down(PC, bUsingGamepad))
	{
		PauseSelection = (PauseSelection + 1) % Pause_Count;
		PlaySound(TEXT("MenuMove"));
	}

	if (PacInput::Confirm(PC, bUsingGamepad))
	{
		ActivatePauseSelection();
	}
}

void APacSharkGameMode::ActivatePauseSelection()
{
	PlaySound(TEXT("MenuSelect"));
	switch (PauseSelection)
	{
	case Pause_Resume:
		bPaused = false;
		break;
	case Pause_Restart:
		StartGame();
		break;
	case Pause_Settings:
		OpenSettings();
		break;
	case Pause_QuitToMenu:
		ReturnToMainMenu();
		break;
	default:
		break;
	}
}

void APacSharkGameMode::HandleSettingsInput(APlayerController* PC)
{
	if (PacInput::Settings(PC, bUsingGamepad) || PacInput::Back(PC, bUsingGamepad) || PacInput::Pause(PC, bUsingGamepad))
	{
		CloseSettings();
		return;
	}
	if (PacInput::Up(PC, bUsingGamepad))
	{
		SelectSetting((SettingsSelection + Setting_Count - 1) % Setting_Count);
		PlaySound(TEXT("MenuMove"));
	}
	else if (PacInput::Down(PC, bUsingGamepad))
	{
		SelectSetting((SettingsSelection + 1) % Setting_Count);
		PlaySound(TEXT("MenuMove"));
	}

	int32 Step = 0;
	if (PacInput::Left(PC, bUsingGamepad))
	{
		Step = -1;
	}
	else if (PacInput::Right(PC, bUsingGamepad))
	{
		Step = 1;
	}
	if (Step != 0)
	{
		StepSetting(Step);
	}

	if (PacInput::Confirm(PC, bUsingGamepad))
	{
		if (SettingsSelection == Setting_Back)
		{
			PlaySound(TEXT("MenuSelect"));
			CloseSettings();
		}
		else if (SettingsSelection == Setting_ResetScores)
		{
			ActivateResetScores();
		}
	}
}

void APacSharkGameMode::SelectSetting(int32 Item)
{
	if (Item != SettingsSelection)
	{
		bResetScoresArmed = false;
	}
	SettingsSelection = Item;
}

void APacSharkGameMode::ActivateResetScores()
{
	PlaySound(TEXT("MenuSelect"));
	if (!bResetScoresArmed)
	{
		bResetScoresArmed = true;
		return;
	}
	bResetScoresArmed = false;
	HighScores.Reset();
	SaveHighScores();
	LastHighScoreRank = -1;
	Game.ClearHighScore(!bInMainMenu);
}

void APacSharkGameMode::StepSetting(int32 Step)
{
	if (SettingsSelection == Setting_Difficulty)
	{
		// From CUSTOM, left goes to Easy and right to Hard; otherwise step through the presets.
		const int32 Current = GetDifficulty();
		const int32 Next = Current == Difficulty_Custom ? (Step < 0 ? Difficulty_Easy : Difficulty_Hard)
			: FMath::Clamp(Current + Step, 0, Difficulty_Count - 1);
		ApplyDifficulty(Next);
	}
	else if (IsSlider(SettingsSelection))
	{
		SetSliderValue(SettingsSelection, GetSliderValue(SettingsSelection) + Step);
	}
	SaveSettings();
	PlaySound(TEXT("MenuMove"));
}

void APacSharkGameMode::CycleNameLetter(int32 Step)
{
	const FString Chars = PacSettings::NameChars;
	int32 Index = 0;
	Chars.FindChar(NameEntry[NameCursor], Index);
	Index = (FMath::Max(0, Index) + Step + Chars.Len()) % Chars.Len();
	NameEntry[NameCursor] = Chars[Index];
	PlaySound(TEXT("MenuMove"));
}

void APacSharkGameMode::HandleNameEntryInput(APlayerController* PC)
{
	// Letters and digits can also be typed straight in. Arrow keys and the gamepad pick letters, so W/A/S/D
	// are free to be typed here. The key list is built once rather than looked up by name every frame.
	static const TArray<TPair<FKey, TCHAR>> TypedKeys = []
	{
		static const TCHAR* const DigitKeyNames[] = { TEXT("Zero"), TEXT("One"), TEXT("Two"), TEXT("Three"), TEXT("Four"),
			TEXT("Five"), TEXT("Six"), TEXT("Seven"), TEXT("Eight"), TEXT("Nine") };
		TArray<TPair<FKey, TCHAR>> Keys;
		for (TCHAR Char = 'A'; Char <= 'Z'; ++Char)
		{
			Keys.Emplace(FKey(FName(*FString::Chr(Char))), Char);
		}
		for (TCHAR Char = '0'; Char <= '9'; ++Char)
		{
			Keys.Emplace(FKey(FName(DigitKeyNames[Char - '0'])), Char);
		}
		return Keys;
	}();
	for (const TPair<FKey, TCHAR>& Typed : TypedKeys)
	{
		if (PC->WasInputKeyJustPressed(Typed.Key))
		{
			const TCHAR Char = Typed.Value;
			bUsingGamepad = false;
			NameEntry[NameCursor] = Char;
			NameCursor = FMath::Min(NameCursor + 1, NameLength - 1);
			PlaySound(TEXT("MenuMove"));
			return;
		}
	}

	bool bPad = bUsingGamepad;
	if (PacInput::Pressed(PC, { EKeys::Up, EKeys::Gamepad_DPad_Up, EKeys::Gamepad_LeftStick_Up }, bPad))
	{
		CycleNameLetter(1);
	}
	else if (PacInput::Pressed(PC, { EKeys::Down, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_LeftStick_Down }, bPad))
	{
		CycleNameLetter(-1);
	}
	else if (PacInput::Pressed(PC, { EKeys::Left, EKeys::BackSpace, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_LeftStick_Left, EKeys::Gamepad_FaceButton_Right }, bPad))
	{
		NameCursor = FMath::Max(0, NameCursor - 1);
		PlaySound(TEXT("MenuMove"));
	}
	else if (PacInput::Pressed(PC, { EKeys::Right, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_LeftStick_Right }, bPad))
	{
		NameCursor = FMath::Min(NameLength - 1, NameCursor + 1);
		PlaySound(TEXT("MenuMove"));
	}
	else if (PacInput::Pressed(PC, { EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_Special_Right }, bPad))
	{
		// Confirm moves to the next letter; on the last letter it saves.
		if (NameCursor < NameLength - 1)
		{
			++NameCursor;
			PlaySound(TEXT("MenuMove"));
		}
		else
		{
			PlaySound(TEXT("MenuSelect"));
			FinishNameEntry();
		}
	}
	bUsingGamepad = bPad;
}

void APacSharkGameMode::HandleTouch(APlayerController* PC)
{
	float X = 0.f, Y = 0.f;
	bool bDown = false;
	PC->GetInputTouchState(ETouchIndex::Touch1, X, Y, bDown);

	int32 ViewW = 0, ViewH = 0;
	PC->GetViewportSize(ViewW, ViewH);
	const APacSharkHUD* HUD = Cast<APacSharkHUD>(PC->GetHUD());

	// On phones with a camera cutout the game is drawn inside the safe area (a black strip covers the cutout),
	// but touches are measured from the screen edge. Shift touches by the safe-area padding so they land in
	// the canvas space the HUD drew its buttons in. Looked up once per press, since it only changes on rotation.
	if (bDown && !bTouchDown)
	{
		TouchCanvasOffset = FVector2D::ZeroVector;
		if (HUD && ViewW > 0 && HUD->GetCanvasSize().X > 0.f && HUD->GetCanvasSize().X < ViewW - 0.5f)
		{
			FDisplayMetrics Metrics;
			FDisplayMetrics::RebuildDisplayMetrics(Metrics);
			TouchCanvasOffset = FVector2D(Metrics.TitleSafePaddingSize.X, Metrics.TitleSafePaddingSize.Y);
		}
	}
	const FVector2D Point = FVector2D(X, Y) - TouchCanvasOffset;
	const float SwipeDistance = FMath::Max(12.f, FMath::Min(ViewW, ViewH) * 0.04f);

	if (bDown && !bTouchDown)
	{
		bTouchDown = true;
		bTouchMoved = false;
		bUsingTouch = true;
		bUsingGamepad = false;
		TouchOrigin = TouchSwipeStart = TouchLast = Point;
		DraggingSlider = -1;
		if (bSettingsOpen && HUD)
		{
			const FPacTouchZone* Zone = HUD->FindTouchZone(Point);
			if (Zone && Zone->Action == EPacTouch::SettingsTrack)
			{
				DraggingSlider = Zone->Index;
				SelectSetting(Zone->Index);
			}
		}
	}

	if (bDown)
	{
		TouchLast = Point;
		if (FVector2D::Distance(Point, TouchOrigin) > SwipeDistance)
		{
			bTouchMoved = true;
		}

		if (DraggingSlider >= 0 && HUD)
		{
			// Set the slider to wherever the finger is along its track.
			if (const FPacTouchZone* Track = HUD->FindTouchZone(EPacTouch::SettingsTrack, DraggingSlider))
			{
				const float Fraction = static_cast<float>(FMath::Clamp((Point.X - Track->Box.Min.X) / FMath::Max(1.0, Track->Box.Max.X - Track->Box.Min.X), 0.0, 1.0));
				const int32 MinValue = GetSliderMin(DraggingSlider);
				const int32 Value = MinValue + FMath::RoundToInt(Fraction * (GetSliderMax(DraggingSlider) - MinValue));
				if (Value != GetSliderValue(DraggingSlider))
				{
					SetSliderValue(DraggingSlider, Value);
					PlaySound(TEXT("MenuMove"), 0.6f);
				}
			}
		}
		else if (!bInIntro && !bInMainMenu && !bPaused && !bSettingsOpen && !bEnteringName)
		{
			// Each swipe of the minimum length turns the shark; keep the finger down and swipe again to turn again.
			const FVector2D Delta = Point - TouchSwipeStart;
			if (Delta.Size() >= SwipeDistance)
			{
				if (FMath::Abs(Delta.X) > FMath::Abs(Delta.Y))
				{
					Game.QueueDirection(Delta.X > 0.f ? EPacDir::Right : EPacDir::Left);
				}
				else
				{
					Game.QueueDirection(Delta.Y > 0.f ? EPacDir::Down : EPacDir::Up);
				}
				TouchSwipeStart = Point;
			}
		}
	}
	else if (bTouchDown)
	{
		bTouchDown = false;
		if (DraggingSlider >= 0)
		{
			DraggingSlider = -1;
			SaveSettings();
		}
		else if (!bTouchMoved)
		{
			HandleTap(PC, TouchLast);
		}
	}
}

void APacSharkGameMode::HandleTap(APlayerController* PC, const FVector2D& Point)
{
	if (bInIntro)
	{
		FinishIntro();
		return;
	}

	const APacSharkHUD* HUD = Cast<APacSharkHUD>(PC->GetHUD());
	const FPacTouchZone* Zone = HUD ? HUD->FindTouchZone(Point) : nullptr;
	if (!Zone)
	{
		if (bInMainMenu || bPaused || bSettingsOpen || bEnteringName)
		{
			return;
		}
		// Tapping anywhere after a game ends starts another one.
		if (Game.GetPhase() == EPacGamePhase::GameOver)
		{
			PlaySound(TEXT("MenuSelect"));
			StartGame();
			return;
		}
		// During play a tap steers the shark toward the side of it that was tapped.
		if (HUD)
		{
			const FVector2D Delta = Point - HUD->TileToScreen(Game.GetPacPos());
			if (FMath::Abs(Delta.X) > FMath::Abs(Delta.Y))
			{
				Game.QueueDirection(Delta.X > 0.f ? EPacDir::Right : EPacDir::Left);
			}
			else if (Delta.Y != 0.f)
			{
				Game.QueueDirection(Delta.Y > 0.f ? EPacDir::Down : EPacDir::Up);
			}
		}
		return;
	}

	switch (Zone->Action)
	{
	case EPacTouch::MenuItem:
		MenuSelection = Zone->Index;
		ActivateMenuSelection(PC);
		break;
	case EPacTouch::PauseItem:
		PauseSelection = Zone->Index;
		ActivatePauseSelection();
		break;
	case EPacTouch::PauseButton:
		bPaused = true;
		PauseSelection = Pause_Resume;
		PlaySound(TEXT("MenuSelect"), 0.7f);
		break;
	case EPacTouch::SettingsItem:
		SelectSetting(Zone->Index);
		if (SettingsSelection == Setting_Back)
		{
			PlaySound(TEXT("MenuSelect"));
			CloseSettings();
		}
		else if (SettingsSelection == Setting_ResetScores)
		{
			ActivateResetScores();
		}
		else
		{
			PlaySound(TEXT("MenuMove"));
		}
		break;
	case EPacTouch::SettingsPrev:
	case EPacTouch::SettingsNext:
		SelectSetting(Zone->Index);
		StepSetting(Zone->Action == EPacTouch::SettingsPrev ? -1 : 1);
		break;
	case EPacTouch::NameUp:
	case EPacTouch::NameDown:
		NameCursor = Zone->Index;
		CycleNameLetter(Zone->Action == EPacTouch::NameUp ? 1 : -1);
		break;
	case EPacTouch::NameBox:
		NameCursor = Zone->Index;
		PlaySound(TEXT("MenuMove"));
		break;
	case EPacTouch::NameDone:
		PlaySound(TEXT("MenuSelect"));
		FinishNameEntry();
		break;
	default:
		break;
	}
}
