// Minimal stand-ins for the few Unreal Engine types the pure simulation (PacGame.h/.cpp) uses, so the
// game logic and its tests build with a plain C++ compiler (see the top of EdgeCaseTests.cpp).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <climits>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef char TCHAR;
#define TEXT(x) x
#define MAX_int32 INT_MAX
#define UE_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#ifndef PI
#define PI 3.14159265358979323846f
#endif

// The same random numbers on every compiler and C library, so the replay checksum in the tests is portable.
// rand()/srand() in the game code and the tests both land here.
inline uint32_t& PacTestRandState() { static uint32_t State = 1; return State; }
inline void PacTestSrand(unsigned Seed) { PacTestRandState() = Seed; }
inline int PacTestRand() { uint32_t& S = PacTestRandState(); S = S * 1103515245u + 12345u; return (int)((S >> 16) & 0x7fff); }
#define srand PacTestSrand
#define rand PacTestRand
#undef RAND_MAX
#define RAND_MAX 0x7fff

template <typename T> std::remove_reference_t<T>&& MoveTemp(T&& V) { return static_cast<std::remove_reference_t<T>&&>(V); }

struct FMath
{
	template <typename T> static T Min(T A, T B) { return A < B ? A : B; }
	template <typename T> static T Max(T A, T B) { return A > B ? A : B; }
	template <typename T> static T Abs(T A) { return A < 0 ? -A : A; }
	template <typename T> static T Clamp(T V, T Lo, T Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
	static int32 RoundToInt(double V) { return (int32)std::floor(V + 0.5); }
	static double RoundToDouble(double V) { return std::floor(V + 0.5); }
	static double FloorToDouble(double V) { return std::floor(V); }
	static int32 FloorToInt(double V) { return (int32)std::floor(V); }
	static float Fmod(float A, float B) { return std::fmod(A, B); }
	static float Frac(float V) { return V - std::floor(V); }
	static float Sqrt(float V) { return std::sqrt(V); }
	static float Exp(float V) { return std::exp(V); }
	static float Sin(float V) { return std::sin(V); }
	static float FRand() { return (float)rand() / ((float)RAND_MAX + 1.f); }
	static int32 RandHelper(int32 N) { return N > 0 ? Min((int32)(FRand() * N), N - 1) : 0; }
};

struct FIntPoint
{
	int32 X = 0, Y = 0;
	FIntPoint() = default;
	FIntPoint(int32 InX, int32 InY) : X(InX), Y(InY) {}
	FIntPoint operator+(const FIntPoint& O) const { return { X + O.X, Y + O.Y }; }
	FIntPoint operator-(const FIntPoint& O) const { return { X - O.X, Y - O.Y }; }
	FIntPoint operator*(int32 S) const { return { X * S, Y * S }; }
	bool operator==(const FIntPoint& O) const { return X == O.X && Y == O.Y; }
	bool operator!=(const FIntPoint& O) const { return !(*this == O); }
};

struct FVector2D
{
	double X = 0, Y = 0;
	FVector2D() = default;
	FVector2D(double InX, double InY) : X(InX), Y(InY) {}
	FVector2D operator+(const FVector2D& O) const { return { X + O.X, Y + O.Y }; }
	FVector2D operator-(const FVector2D& O) const { return { X - O.X, Y - O.Y }; }
	FVector2D operator*(double S) const { return { X * S, Y * S }; }
	FVector2D& operator+=(const FVector2D& O) { X += O.X; Y += O.Y; return *this; }
	bool operator==(const FVector2D& O) const { return X == O.X && Y == O.Y; }
	double Size() const { return std::sqrt(X * X + Y * Y); }
	static double Distance(const FVector2D& A, const FVector2D& B) { return (A - B).Size(); }
	static const FVector2D ZeroVector;
};
inline const FVector2D FVector2D::ZeroVector(0, 0);

template <int32 N> struct TInlineAllocator {};
template <typename T> void Swap(T& A, T& B) { std::swap(A, B); }

// std::vector<bool> has no real references, so bools are stored in a one-byte box.
template <typename T> struct TArrayElem { T V{}; TArrayElem() = default; TArrayElem(const T& In) : V(In) {} };

template <typename T, typename Alloc = void>
class TArray
{
	using E = std::conditional_t<std::is_same_v<T, bool>, TArrayElem<bool>, T>;
	static T& Ref(E& e) { if constexpr (std::is_same_v<T, bool>) return e.V; else return e; }
	static const T& Ref(const E& e) { if constexpr (std::is_same_v<T, bool>) return e.V; else return e; }
public:
	TArray() = default;
	int32 Num() const { return (int32)Data.size(); }
	bool IsValidIndex(int32 i) const { return i >= 0 && i < Num(); }
	T& operator[](int32 i) { return Ref(Data[i]); }
	const T& operator[](int32 i) const { return Ref(Data[i]); }
	int32 Add(const T& V) { Data.push_back(V); return Num() - 1; }
	T& AddDefaulted_GetRef() { Data.emplace_back(); return Ref(Data.back()); }
	void SetNum(int32 N) { Data.resize(N); }
	void Init(const T& V, int32 N) { Data.assign(N, E(V)); }
	void Reset() { Data.clear(); }
	void Empty() { Data.clear(); }
	void Reserve(int32 N) { Data.reserve(N); }
	void RemoveAt(int32 i) { Data.erase(Data.begin() + i); }
	void RemoveAtSwap(int32 i) { std::swap(Data[i], Data.back()); Data.pop_back(); }
	T& Last() { return Ref(Data.back()); }
	T Pop() { T V = Ref(Data.back()); Data.pop_back(); return V; }
	auto begin() { return Data.begin(); }
	auto end() { return Data.end(); }
	auto begin() const { return Data.begin(); }
	auto end() const { return Data.end(); }
private:
	std::vector<E> Data;
};

template <typename Sig> using TFunctionRef = std::function<Sig>;
