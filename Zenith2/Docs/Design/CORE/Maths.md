# CORE design: numerics, maths types, hashing and randomness

This file is part of the CORE design document family (entry: `Zenith2/Docs/Design/CORE.md`). It fixes Zenith2's numerical conventions, units and coordinate systems; the exact layouts and operation sets of `Zenith_Vector2`, `Zenith_Vector3`, `Zenith_Vector4`, `Zenith_Matrix3`, `Zenith_Matrix4`, `Zenith_Quaternion`, `Zenith_Colour` and the bounds types; `Zenith_Hash64` on both D-SEL-HASH (open, Stage 1) paths; the fixed-algorithm SHA-256 `Zenith_Sha256`, which D-SEL-HASH never reaches; the counter-based `Zenith_RandomSource`; `Zenith_PersistentID` generation and text forms; and the deterministic transcendental functions with the CRT maths mode of ruling X314-FMA3.

## Scope of this file

| Topic | Fixed elsewhere | Specified here |
|---|---|---|
| half-float, octahedral and QTangent codecs | ZenithVertexFormat, `Zenith2/Docs/Rendering.md` → "Vertex data format" (`Zenith_F32ToF16`, `Zenith_F16ToF32`) | nothing: CORE provides no half type and no vertex codec |
| floating-point environment | `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" (subsection "Floating-point environment"; owner ZenithJobs) | the rules CORE maths obeys under it |
| no floating-point contraction, no RTTI | `Zenith2/Docs/Architecture/BuildSystem.md` → "Compile and link flags" | nothing |
| script random key and derivations | `Zenith2/Docs/Scripting.md` → "Determinism rules and randomness" | the shared derivation functions Scripting uses |
| reflected vector layouts | `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file" | the C++ types that match them |
| persistent-ID semantics and text form | `Zenith2/Docs/Contracts.md` → "Handle kinds" (`Zenith_PersistentID`) | generation, parsing and formatting functions |
| run-time selection of FMA3 implementations of the CRT's transcendental functions on x64 Windows | `Zenith2/Docs/Architecture.md` → "Determinism guarantee and mechanisms" ([Z2-N-ARCH-888], [Z2-N-ARCH-889]) | the deterministic functions every simulation-affecting and tracked-output computation uses instead, and `Zenith_ApplyCrtMathMode` ("Deterministic transcendental functions") |
| digests that SCENE's derived authoring IDs and document journals and PLAT's patch mount store or check | `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Derivation"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Patch mount verification" | the one SHA-256 implementation both use ("Zenith_Sha256") |
| axis, handedness and unit conversion on import | CONTENT (inventory item Z2-I-CONTENT-11, `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1"), D-SEL-FBX (open, Stage 4) | the engine convention that import converts into |

## Numerical conventions

<!-- elaboration -->
| Rule | Detail |
|---|---|
| scalar types | `float` (IEEE 754 binary32) for every simulation and rendering quantity; `double` for JSON numbers, long accumulations outside simulation (editor timelines, statistics), and the unit-interval random draws; `sint64` / `uint64` for durations and counters |
| double and char prefixes | a `double` variable takes the prefix `d` (`double dMin`, constant `dPI`) and a `char` the prefix `c` (`cValue`, `acLiteral`); both rows are in `Zenith2/Docs/Contracts.md` → "Naming and layout" (a fixed `char` array takes `ac`) |
| operations | CORE maths uses IEEE operations and CORE's deterministic transcendental functions alone; no `-ffast-math`, no `/fp:fast`, no reassociation, no contraction. `+ − × ÷` and `std::sqrt` are correctly rounded and bit-reproducible on every platform. No CORE code, and under ruling X314-FMA3 no simulation-affecting code and no code that produces a tracked or baked output, calls a C runtime transcendental (`<cmath>` `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `exp`, `exp2`, `log`, `log2`, `log10`, `pow`, `cbrt`), because the x64 Universal CRT selects FMA3 implementations at run time and one build then differs across CPUs; such code calls `Zenith_Sin` … `Zenith_Sqrt`, which are bit-identical on every CPU and platform ("Deterministic transcendental functions"). Every `sin`, `cos`, `tan`, `asin`, `acos`, `atan2` and `pow` in this file's definitions is that function at the operand's precision (Z2-D-019) |
| SIMD | CORE maths types are scalar code; any vectorised simulation kernel computes, per lane, exactly the scalar sequence of operations ([Z2-N-ARCH-883]) |
| denormals | flushed to zero on every simulation thread by the fixed floating-point environment; CORE code never depends on gradual underflow |
| comparisons | floating equality is exact equality; approximate comparison is always explicit through `Zenith_IsNearlyEqual(a, b, fAbsTolerance, fRelTolerance)`; bit equality for determinism checks uses `std::bit_cast<uint32>` |
| degenerate inputs | an operation that is undefined for its input (normalising a zero vector, inverting a singular matrix) has a checked form returning `bool` or a fallback, and an unchecked form whose precondition is `ZENITH_ASSERT` |
| integer overflow | unsigned arithmetic that may overflow uses `Zenith_CheckedAdd`, `Zenith_CheckedMul` and `Zenith_CheckedSub` (below) or is shown not to overflow in a comment; signed overflow never happens (it is undefined behaviour): signed values that may overflow are computed in the checked helpers; narrowing goes through `Zenith_NarrowCast` ([Z2-N-CONTRACT-398]) |
| NaN and infinity | never produced deliberately by simulation code; `IsFinite()` exists on every maths type; parsers reject non-finite input unless their format allows it |

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Maths.h
constexpr float fPI = 3.14159265358979323846f;
constexpr float fTAU = 6.28318530717958647692f;
constexpr float fHALF_PI = 1.57079632679489661923f;
constexpr double dPI = 3.14159265358979323846;
constexpr float fDEGREES_TO_RADIANS = fPI / 180.0f;
constexpr float fRADIANS_TO_DEGREES = 180.0f / fPI;
constexpr float fMATHS_NORMALIZE_EPSILON = 1e-12f;     // squared-length threshold below which a vector is degenerate

/// @contract functions Zenith_Maths
/// Ownership: Pure functions over values.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Angles in radians; other quantities in the caller's units.
/// Errors: Checked helpers return false on overflow and leave the output unspecified; nothing asserts.
/// Version: since Z2 Stage 1. Results are bit-identical per platform and build.
/// @endcontract
template<typename T> constexpr T Zenith_Min(T xA, T xB);
template<typename T> constexpr T Zenith_Max(T xA, T xB);
template<typename T> constexpr T Zenith_Clamp(T xValue, T xLow, T xHigh);   // ZENITH_ASSERT xLow <= xHigh
constexpr float Zenith_Lerp(float fA, float fB, float fT);                  // fA + (fB − fA) × fT
constexpr float Zenith_Saturate(float fValue);
float Zenith_WrapAngle(float fRadians);                                      // into (−π, π]
bool Zenith_IsNearlyEqual(float fA, float fB, float fAbsTolerance, float fRelTolerance);
constexpr bool Zenith_IsPowerOfTwo(uint64 ulValue);
constexpr uint64 Zenith_AlignUp(uint64 ulValue, uint64 ulAlignment);     // ZENITH_ASSERT power of two; caller checks overflow
constexpr bool Zenith_CheckedAdd(uint64 ulA, uint64 ulB, uint64& ulOut); // false on overflow
constexpr bool Zenith_CheckedSub(uint64 ulA, uint64 ulB, uint64& ulOut);
constexpr bool Zenith_CheckedMul(uint64 ulA, uint64 ulB, uint64& ulOut);
constexpr bool Zenith_CheckedAlignUp(uint64 ulValue, uint64 ulAlignment, uint64& ulOut);
```

`Zenith_IsNearlyEqual` returns `|a − b| ≤ max(fAbsTolerance, fRelTolerance × max(|a|, |b|))`, false when either is NaN.

## Deterministic transcendental functions

<!-- elaboration -->
The orchestrator's ruling X314-FMA3 (2026-10-09; cross-doc item X314) is binding here. Simulation-affecting code, and every output that is tracked in source control or baked, never calls a C runtime transcendental function. CORE owns a fixed-algorithm set in binary32 and binary64, and every Windows `main` also switches the CRT's FMA3 dispatch off as defence in depth.

The reason: the x64 Universal CRT picks FMA3 implementations of `exp`, `log`, `pow`, `sin`, `cos`, `tan` and related functions at run time, from the CPU and the process-wide flag `_get_FMA3_enable` reads. So one Windows build returns different bits for `std::sin` on CPUs with and without FMA3, and the per-build guarantee of [Z2-N-ARCH-881] fails across machines. That breaks replays recorded on one machine and checked on another, and tool outputs committed by different team members.

### Transcendental scope and guarantee

- The functions are `Zenith_Sin`, `Zenith_Cos`, `Zenith_SinCos`, `Zenith_Tan`, `Zenith_Asin`, `Zenith_Acos`, `Zenith_Atan`, `Zenith_Atan2`, `Zenith_Exp`, `Zenith_Exp2`, `Zenith_Ln`, `Zenith_Log2`, `Zenith_Log10`, `Zenith_Pow`, `Zenith_Cbrt` and `Zenith_Sqrt`. Each has a `double` (binary64) and a `float` (binary32) overload.
- The set is the union of what the consumers named:
  - ANIM: the screw power, inertialisation, IK, springs and cook resampling (`sin`, `cos`, `atan2`, `acos`, `exp`).
  - PHYS, rule D9 of `Zenith2/Docs/Design/PHYS/Simulation.md` → "Determinism rules": `sin`, `cos`, `tan`, `atan`, `atan2`, `exp`, `log`, `pow`, `cbrt`.
  - WORLD, row WD29: `sin`, `cos` and `atan2` in its generators and tracer.
  - CHAR: the lip-sync estimator's `sin`, `cos`, `exp`, `log` and `log10`, and the ragdoll's `atan2` and `acos`.
  - AI: yaw, sight angles, slope and the logistic utility.
  - TWOD: rotation angles.
  - CORE's own maths types.
  - `Zenith_Exp2`, `Zenith_Log2` and `Zenith_Asin` are the companions those formulas imply.
  - `Zenith_Sqrt` exists so that a single rule can forbid `<cmath>` in simulation code. IEEE 754 already rounds `std::sqrt` correctly on every platform, so `std::sqrt` of a non-negative argument stays allowed.
- Natural log is named `Zenith_Ln`, not `Zenith_Log`, because `Zenith_Log.h` and the `ZENITH_LOG` macros are CORE's logging API.
- Guarantee: for each function and precision, the result bits are a pure function of the argument bits. They are identical:
  - on every CPU that runs one build;
  - whatever the CRT's FMA3 flag is set to;
  - whatever the thread's flush-to-zero and denormals-are-zero state is;
  - on every supported compiler and platform, because every step below is an IEEE 754 binary64 operation with round-to-nearest-even, which MSVC x64, clang x86-64 and clang arm64 implement identically.
- The cross-platform clause is stronger than [Z2-N-ARCH-882] requires. It costs nothing, and it makes the golden vectors of "Golden vectors" one file for every preset.
- The algorithms are fixed: changing a step, a constant or a threshold changes results, which is a replay-format version bump (`Zenith2/Docs/Scripting.md` → "Determinism rules and randomness") and a re-bake.
- Bit identity is the contract. The error bounds of "Error bounds" are documented accuracy, not the oracle.

### Transcendental declarations

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Transcendental.h   (Zenith_Maths.h includes it)
/// @contract functions Zenith_Transcendental
/// Ownership: Pure functions over values; no state, no allocation, no lock, no profiler hook, no log, no system
///   call and no C runtime call (Zenith_ApplyCrtMathMode and Zenith_IsCrtMathModeApplied excepted, below).
/// Thread affinity: ANY_THREAD (callable on the audio device thread and inside profiler hooks).
/// Invalidation: None.
/// Units: Angles in radians; Zenith_Exp, Zenith_Ln and Zenith_Pow use base e, Zenith_Exp2 and Zenith_Log2 base 2,
///   Zenith_Log10 base 10.
/// Errors: None asserts. An argument outside the domain of "Special values" (|x| > 2^20 for Sin, Cos, SinCos and
///   Tan; |x| > 1 for Asin and Acos; x < 0 for Ln, Log2, Log10 and Sqrt; a negative base with a non-integer
///   exponent for Pow) returns the canonical quiet NaN (0x7FF8000000000000 binary64, 0x7FC00000 binary32), as does
///   any NaN argument; Pow's NaN exceptions are its special-value table.
/// Version: since Z2 Stage 2. Results are bit-identical for identical argument bits on every supported CPU,
///   compiler and platform, independent of the FMA3 CRT flag and of FTZ/DAZ ("Transcendental scope and
///   guarantee"); the algorithm is fixed and a change is a replay-format version bump.
/// @endcontract
double Zenith_Sin(double dRadians);
float Zenith_Sin(float fRadians);
double Zenith_Cos(double dRadians);
float Zenith_Cos(float fRadians);
void Zenith_SinCos(double dRadians, double& dSinOut, double& dCosOut);   // bit-identical to Zenith_Sin and Zenith_Cos
void Zenith_SinCos(float fRadians, float& fSinOut, float& fCosOut);
double Zenith_Tan(double dRadians);
float Zenith_Tan(float fRadians);
double Zenith_Asin(double dValue);
float Zenith_Asin(float fValue);
double Zenith_Acos(double dValue);
float Zenith_Acos(float fValue);
double Zenith_Atan(double dValue);
float Zenith_Atan(float fValue);
double Zenith_Atan2(double dY, double dX);               // in [−π, π]
float Zenith_Atan2(float fY, float fX);
double Zenith_Exp(double dValue);
float Zenith_Exp(float fValue);
double Zenith_Exp2(double dValue);
float Zenith_Exp2(float fValue);
double Zenith_Ln(double dValue);
float Zenith_Ln(float fValue);
double Zenith_Log2(double dValue);
float Zenith_Log2(float fValue);
double Zenith_Log10(double dValue);
float Zenith_Log10(float fValue);
double Zenith_Pow(double dBase, double dExponent);
float Zenith_Pow(float fBase, float fExponent);
double Zenith_Cbrt(double dValue);
float Zenith_Cbrt(float fValue);
double Zenith_Sqrt(double dValue);
float Zenith_Sqrt(float fValue);

/// @contract functions Zenith_CrtMathMode
/// Ownership: Process-wide C runtime state; no Zenith state.
/// Thread affinity: MAIN_THREAD, before any other thread exists (the first statement of every main).
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 1. Defence in depth for X314-FMA3: Zenith code never calls a CRT transcendental,
///   but third-party code may.
/// @endcontract
void Zenith_ApplyCrtMathMode();                       // Windows x64: _set_FMA3_enable(0); elsewhere nothing
bool Zenith_IsCrtMathModeApplied();    // Windows x64: _get_FMA3_enable() == 0; elsewhere true
```

An `int` argument is ambiguous between the two overloads, so it fails to compile. A `float` argument selects binary32. Mixed expressions promoted to `double` select binary64, and assigning that result back to `float` is a narrowing the warning set reports.

### Shared evaluation rules

1. **Translation unit.** Every function is defined out of line in `Zenith2/Engine/ZenithCore/Internal/Maths/Zenith_Transcendental.cpp`, which includes no `<cmath>`.
   - The file begins (after `Zenith.h`) with `ZENITH_STRICT_FP_BEGIN` and ends with `ZENITH_STRICT_FP_END`. These two macros are the one compiler-specific spelling, in `Internal/Zenith_CompilerSupport.h` (rule F11):
     - MSVC: `__pragma(float_control(precise, on, push)) __pragma(fp_contract(off))` and `__pragma(float_control(pop))`.
     - clang: `_Pragma("clang fp contract(off)")` and nothing.
   - Contraction is therefore off in this file even if a target's flags change ([Z2-N-ARCH-188] already forbids it everywhere).
   - The file uses no intrinsic, no SIMD type and no run-time CPU dispatch (no `__cpuid`, no function multiversioning).
   - Every step below is one IEEE operation, evaluated exactly in the written order and parenthesisation (`/fp:precise` and the absence of `-ffast-math` forbid reassociation). Horner forms are written out as nested products.
   - The constants are `static constexpr double` values made with `std::bit_cast<double>(<hex>ull)`, in `Internal/Maths/Zenith_TranscendentalTables.h` (the committed coefficient table, "Constants and coefficient tables").
2. **Square root.** `Zenith_HardwareSqrt(double)` in `Internal/Zenith_CompilerSupport.h` is the only square root used: `_mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(d)))` on x86-64 (MSVC and clang) and `vget_lane_f64(vsqrt_f64(vdup_n_f64(d)), 0)` on arm64 (`<arm_neon.h>`), so it is the hardware instruction (`sqrtsd`, `fsqrt`) and never a CRT call. The binary32 `Zenith_Sqrt` is `static_cast<float>` of the binary64 root of the widened argument, which is correctly rounded because binary64 carries more than twice binary32's precision plus two bits. It is called only with a non-negative normal or zero argument, and IEEE 754 §5.4.1 makes the result correctly rounded.
3. **Classification by bits.** `E(v)` is the 11-bit biased exponent field of a binary64 `v` (`(std::bit_cast<uint64>(v) >> 52) & 0x7FF`), and `M(v)` is its 52-bit fraction field.
   - Before anything else, a binary64 argument with `E = 0` and a non-zero fraction (a subnormal) is replaced by the zero of its sign. A binary32 argument with exponent field 0 and a non-zero fraction is replaced the same way, before it is widened.
   - A NaN argument returns the canonical NaN, except where Pow's table says otherwise.
4. **Result flush.**
   - A binary64 result whose magnitude is below 2^−1022 is returned as the zero of its sign. The decision is made by the comparison `|r| < 0x0010000000000000` on a value the steps prove is either normal or of that size class, never by reading a subnormal back.
   - The binary32 overloads evaluate the binary64 function on the widened argument. A binary64 result `d` with `|d| < 2^−126` returns the binary32 zero of its sign, a NaN returns `0x7FC00000`, and anything else returns `static_cast<float>(d)` (round to nearest, so values of binary32 overflow size round to ±∞).
   - Together, rules 3 and 4 make no step read or produce a subnormal on a path whose outcome depends on it, so the FTZ/DAZ state of [Z2-N-CONTRACT-360] (simulation threads) and its absence (tool threads) give the same bits.
5. **Error-free transformations** (no FMA):
   - `Split(a)`: `c = 134217729.0 × a`, `a_hi = c − (c − a)`, `a_lo = a − a_hi`.
   - `TwoProd(a, b) → (p, e)`: `p = a × b`. With `(a_hi, a_lo) = Split(a)` and `(b_hi, b_lo) = Split(b)`, `e = (((a_hi × b_hi − p) + a_hi × b_lo) + a_lo × b_hi) + a_lo × b_lo`.
   - `TwoSum(a, b) → (s, e)`: `s = a + b`, `bb = s − a`, `e = (a − (s − bb)) + (b − bb)`.
   - `FastTwoSum(a, b) → (s, e)`: `s = a + b`, `e = b − (s − a)`; used only where `|a| ≥ |b|` is shown.
6. **Round to integer.** `RoundInt(t) = (t + 0x1.8p52) − 0x1.8p52`, for `|t| < 2^51`, gives the nearest integer with ties to even. An integer `n` computed this way is converted with `static_cast<int32>`.
7. **Powers of two.** `Pow2(k)` for `−1022 ≤ k ≤ 1023` is `std::bit_cast<double>(uint64(k + 1023) << 52)`.
8. **Sign.** `CopySign(m, s)` takes the sign bit of `s` (so −0 is negative).

### Constants and coefficient tables

Each constant is a mathematical value, rounded to the nearest binary64 (ties to even) or truncated as stated. Anyone with a high-precision calculator can re-derive every value from its definition; the hexadecimal encodings below are the normative values, copied into `Zenith_TranscendentalTables.h` in this order:

| Name | Definition | binary64 encoding |
|---|---|---|
| `INV_PIO2` | 2/π | `0x3FE45F306DC9C883` |
| `PIO2_1` | π/2 rounded, low 20 encoding bits cleared (33 significant bits) | `0x3FF921FB54400000` |
| `PIO2_1T` | π/2 − `PIO2_1` | `0x3DD0B4611A626331` |
| `PIO2_2` | `PIO2_1T` with its low 20 encoding bits cleared | `0x3DD0B4611A600000` |
| `PIO2_2T` | π/2 − `PIO2_1` − `PIO2_2` | `0x3BA3198A2E037073` |
| `PIO2_3` | `PIO2_2T` with its low 20 encoding bits cleared | `0x3BA3198A2E000000` |
| `PIO2_3T` | π/2 − `PIO2_1` − `PIO2_2` − `PIO2_3` | `0x397B839A252049C1` |
| `PIO4` | π/4 | `0x3FE921FB54442D18` |
| `PIO2` | π/2 | `0x3FF921FB54442D18` |
| `PI_HI`, `PI_LO` | π; π − `PI_HI` | `0x400921FB54442D18`, `0x3CA1A62633145C07` |
| `PI3O4` | 3π/4 | `0x4002D97C7F3321D2` |
| `S3` … `S17` | (−1)^k / (2k+1)! for 2k+1 = 3, 5, …, 17 | `0xBFC5555555555555`, `0x3F81111111111111`, `0xBF2A01A01A01A01A`, `0x3EC71DE3A556C734`, `0xBE5AE64567F544E4`, `0x3DE6124613A86D09`, `0xBD6AE7F3E733B81F`, `0x3CE952C77030AD4A` |
| `C4` … `C16` | (−1)^k / (2k)! for 2k = 4, 6, …, 16 | `0x3FA5555555555555`, `0xBF56C16C16C16C17`, `0x3EFA01A01A01A01A`, `0xBE927E4FB7789F5C`, `0x3E21EED8EFF8D898`, `0xBDA93974A8C07C9D`, `0x3D2AE7F3E733B81F` |
| `A3` … `A13` | (−1)^k / (2k+1) for 2k+1 = 3, 5, …, 13 | `0xBFD5555555555555`, `0x3FC999999999999A`, `0xBFC2492492492492`, `0x3FBC71C71C71C71C`, `0xBFB745D1745D1746`, `0x3FB3B13B13B13B14` |
| `ATAN_HI[k]`, `ATAN_LO[k]`, k = 0 … 8 | atan(k/8); atan(k/8) − `ATAN_HI[k]` | HI: `0`, `0x3FBFD5BA9AAC2F6E`, `0x3FCF5B75F92C80DD`, `0x3FD6F61941E4DEF1`, `0x3FDDAC670561BB4F`, `0x3FE1E00BABDEFEB4`, `0x3FE4978FA3269EE1`, `0x3FE700A7C5784634`, `0x3FE921FB54442D18`; LO: `0`, `0xBC4CD37686760C17`, `0x3C68AB6E3CF7AFBD`, `0xBC7C63AAE6F6E918`, `0x3C7A2B7F222F65E2`, `0xBC5928DF287A668F`, `0x3C72419A87F2A458`, `0xBC78C34D25AADEF6`, `0x3C81A62633145C07` |
| `ACOT_HI[k]`, `ACOT_LO[k]`, k = 0 … 8 | π/2 − atan(k/8); its remainder | HI: `0x3FF921FB54442D18`, `0x3FF7249FAA996A21`, `0x3FF5368C951E9CFD`, `0x3FF3647503CAF55C`, `0x3FF1B6E192EBBE44`, `0x3FF031F57E54ADBE`, `0x3FEDAC670561BB4F`, `0x3FEB434EE31013FD`, `0x3FE921FB54442D18`; LO: `0x3C91A62633145C07`, `0x3C9A8CC1E7480C68`, `0xBC996F47948A99F1`, `0x3C817E21D9A42C9A`, `0x3C9B1B466A88828E`, `0x3C9338B4259C0270`, `0x3C8A2B7F222F65E2`, `0xBC80520D0701D877`, `0x3C81A62633145C07` |
| `INV_LN2` | 1/ln 2 | `0x3FF71547652B82FE` |
| `LN2_HI`, `LN2_LO` | ln 2 rounded, low 21 encoding bits cleared (32 significant bits); ln 2 − `LN2_HI` | `0x3FE62E42FEE00000`, `0x3DEA39EF35793C76` |
| `LN2_D`, `LN2_DLO` | ln 2; ln 2 − `LN2_D` | `0x3FE62E42FEFA39EF`, `0x3C7ABC9E3B39803F` |
| `INV_LN2_HI`, `INV_LN2_LO` | 1/ln 2; its remainder | `0x3FF71547652B82FE`, `0x3C7777D0FFDA0D24` |
| `INV_LN10_HI`, `INV_LN10_LO` | 1/ln 10; its remainder | `0x3FDBCB7B1526E50E`, `0x3C695355BAAAFAD3` |
| `E2` … `E13` | 1/n! for n = 2 … 13 | `0x3FE0000000000000`, `0x3FC5555555555555`, `0x3FA5555555555555`, `0x3F81111111111111`, `0x3F56C16C16C16C17`, `0x3F2A01A01A01A01A`, `0x3EFA01A01A01A01A`, `0x3EC71DE3A556C734`, `0x3E927E4FB7789F5C`, `0x3E5AE64567F544E4`, `0x3E21EED8EFF8D898`, `0x3DE6124613A86D09` |
| `K1` … `K4` | 2/(2n+1) for n = 1 … 4 | `0x3FE5555555555555`, `0x3FD999999999999A`, `0x3FD2492492492492`, `0x3FCC71C71C71C71C` |
| `LOGC_HI[j]`, `LOGC_LO[j]`, j = 23 … 45 | ln(j/32); its remainder (j = 32 gives 0, 0) | HI: `0xBFD522AE0738A3D8`, `0xBFD269621134DB92`, `0xBFCF991C6CB3B379`, `0xBFCA93ED3C8AD9E3`, `0xBFC5BF406B543DB2`, `0xBFC1178E8227E47C`, `0xBFB9335E5D594989`, `0xBFB08598B59E3A07`, `0xBFA0415D89E74444`, `0`, `0x3F9F829B0E783300`, `0x3FAF0A30C01162A6`, `0x3FB6F0D28AE56B4C`, `0x3FBE27076E2AF2E6`, `0x3FC29552F81FF523`, `0x3FC5FF3070A793D4`, `0x3FC9525A9CF456B4`, `0x3FCC8FF7C79A9A22`, `0x3FCFB9186D5E3E2B`, `0x3FD1675CABABA60E`, `0x3FD2E8E2BAE11D31`, `0x3FD4618BC21C5EC2`, `0x3FD5D1BDBF5809CA`; LO: `0x3C68F7E9B38A6979`, `0xBC7E0EFADD9DB02B`, `0xBC6F665066F980A2`, `0xBC6BCAFA9DE97203`, `0x3C21F5B44C0DF7E7`, `0x3C50E63A5F01C691`, `0x3C5478A85704CCB7`, `0x3C5DD7009902BF32`, `0xBC4C05CF1D753622`, `0`, `0x3C333E3F04F1EF23`, `0x3C485F325C5BBACD`, `0xBC5906D99184B992`, `0xBC361578001E0162`, `0x3C6301771C407DBF`, `0xBC5BC60EFAFC6F6E`, `0x3C6D904C1D4E2E26`, `0xBC64F689F8434012`, `0xBC6CAAAE64F21ACB`, `0x3C2CE63EAB883717`, `0xBC78F4CDB95EBDF9`, `0x3C7F42DECDECCF1D`, `0x3C74236383DC7FE1` |
| `SQRT2` | √2 | `0x3FF6A09E667F3BCD` |
| `INV_SEVEN` | 1/7 | `0x3FC2492492492492` |
| `EXP_OVERFLOW` | the largest x with a finite exp(x) | `0x40862E42FEFA39EF` (709.782712893384) |
| `T27`, `T54`, `T66`, `TRIG_LIMIT` | 2^−27, 2^−54, 2^66, 2^20 | `0x3E40000000000000`, `0x3C90000000000000`, `0x4410000000000000`, `0x4130000000000000` |

The tables use Taylor coefficients (1/n! and 1/(2k+1) rounded) instead of minimax coefficients, so that no fitting program is part of the specification. Their extra terms cost a few multiply-adds per call.

### Trigonometric algorithm steps

**Reduce(x) → (q, hi, lo)**, for a finite x with `|x| ≤ TRIG_LIMIT`: x = n·π/2 + (hi + lo), with q = n mod 4 and `|hi| ≤ π/4 + 2^−40`.

1. If `|x| ≤ PIO4`: `q = 0`, `hi = x`, `lo = 0`.
2. Otherwise:
   1. `t = x × INV_PIO2`; `n = RoundInt(t)`.
   2. `r = x − n × PIO2_1`. Exact: n < 2^21 and `PIO2_1` has 33 bits, and Sterbenz's lemma applies.
   3. `w = n × PIO2_1T`; `y0 = r − w`; `i = E(x) − E(y0)`.
   4. If `i > 16`, take a second round:
      1. `t2 = r`, `w = n × PIO2_2`, `r = t2 − w`, `w = n × PIO2_2T − ((t2 − r) − w)`, `y0 = r − w`, `i = E(x) − E(y0)`.
      2. If now `i > 49`, take a third round: the same five statements with `PIO2_3` and `PIO2_3T`.
   5. `y1 = (r − y0) − w`; `hi = y0`, `lo = y1`, `q = static_cast<int32>(n) & 3`.

This is the Cody–Waite reduction with a two-word remainder and up to three rounds, as in fdlibm 5.3's medium-argument path, written out here from the method; no third-party code is used.

**KernelSin(hi, lo) → (s, s_lo)**:
1. If `|hi| < T27`, return `(hi, lo)`.
2. `z = hi × hi`; `v = z × hi`.
3. `r = S5 + z × (S7 + z × (S9 + z × (S11 + z × (S13 + z × (S15 + z × S17)))))`.
4. Return `FastTwoSum(hi, v × (S3 + z × r) + lo × (1.0 − 0.5 × z))`.

**KernelCos(hi, lo) → (c, c_lo)**:
1. If `|hi| < T27`, return `(1.0, 0)`.
2. `z = hi × hi`; `r = C4 + z × (C6 + z × (C8 + z × (C10 + z × (C12 + z × (C14 + z × C16)))))`.
3. `hz = 0.5 × z`; `w = 1.0 − hz`.
4. Return `FastTwoSum(w, ((1.0 − w) − hz) + (z × (z × r) − hi × lo))`.

**The functions** (after rule 3; a non-finite x, or `|x| > TRIG_LIMIT`, returns the canonical NaN):

- `Zenith_Sin(x)`:
  1. If `|x| < T27`, return x.
  2. `(q, hi, lo) = Reduce(x)`.
  3. `s` is the first word of `KernelCos(hi, lo)` when q is odd, and of `KernelSin(hi, lo)` when q is even.
  4. Return `−s` when `q & 2`, else `s`, with rule 4.
- `Zenith_Cos(x)`:
  1. If `|x| < T27`, return 1.0.
  2. `(q, hi, lo) = Reduce(x)`.
  3. The result is, by q = 0, 1, 2, 3: `KernelCos`, `−KernelSin`, `−KernelCos`, `KernelSin`, each the first word, with rule 4.
- `Zenith_SinCos(x)` performs `Reduce` once and both kernels once, and writes exactly the two values `Zenith_Sin(x)` and `Zenith_Cos(x)` return.
- `Zenith_Tan(x)`:
  1. If `|x| < T27`, return x.
  2. `(q, hi, lo) = Reduce(x)`, `(n_h, n_l) = KernelSin(hi, lo)`, `(d_h, d_l) = KernelCos(hi, lo)`.
  3. When q is odd, replace the pair by `(n_h, n_l, d_h, d_l) = (−d_h, −d_l, n_h, n_l)`.
  4. `t = n_h / d_h`; `(p, pe) = TwoProd(t, d_h)`; `t_l = ((((n_h − p) − pe) + n_l) − t × d_l) / d_h`.
  5. Return `t + t_l`, with rule 4.

### Inverse trigonometric algorithm steps

**AtanCore(s, s_l, bComplement) → (h, l)**, for `0 ≤ s ≤ 1` (s with its low word `s_l`). It returns atan(s) as two words, or π/2 − atan(s) when `bComplement` is set.

1. If `s < T27`:
   - without `bComplement`, return `(s, s_l)`;
   - with it, `(h, l) = TwoSum(ACOT_HI[0], −s)` and return `(h, l + (ACOT_LO[0] − s_l))`.
2. `k = static_cast<int32>(8.0 × s + 0.5)` (0 … 8); `c = k × 0.125`.
3. If `k = 0`: `u = s`, `u_l = s_l`. Otherwise:
   1. `num = s − c`. Exact by Sterbenz.
   2. `(q, qe) = TwoProd(s, c)`; `den = 1.0 + q`; `den_l = ((1.0 − den) + q) + qe`.
   3. `u = num / den`; `(p, pe) = TwoProd(u, den)`; `u_l = ((((num − p) − pe) − u × den_l) + s_l) / den`.
4. `z = u × u`; `poly = u × (z × (A3 + z × (A5 + z × (A7 + z × (A9 + z × (A11 + z × A13))))))`.
5. Without `bComplement`: `(h, l) = TwoSum(ATAN_HI[k], u)`; return `(h, l + (ATAN_LO[k] + (u_l + poly)))`.
6. With `bComplement`: `(h, l) = TwoSum(ACOT_HI[k], −u)`; return `(h, l + (ACOT_LO[k] − (u_l + poly)))`.

**Ratio(n, n_l, d, d_l) → (s, s_l)**, for positive n ≤ d whose quotient is not below 2^−30: `s = n / d`; `(p, pe) = TwoProd(s, d)`; `s_l = ((((n − p) − pe) + n_l) − s × d_l) / d`.

**The functions**:

- `Zenith_Atan(x)`, with `a = |x|`:
  1. If `a < T27`, return x.
  2. If `a > T66`, return `CopySign(PIO2, x)`.
  3. If `a ≤ 1`: `(h, l) = AtanCore(a, 0, false)`.
  4. Otherwise: `s = 1.0 / a`; `(p, pe) = TwoProd(s, a)`; `s_l = ((1.0 − p) − pe) / a`; `(h, l) = AtanCore(s, s_l, true)`.
  5. Return `CopySign(h + l, x)`.
- `Zenith_Atan2(y, x)`:
  1. Return the special values of "Special values" for a zero, infinite or NaN argument. Otherwise let `a_y = |y|` and `a_x = |x|`.
  2. Choose the ratio:
     - `bLower = a_y ≤ a_x`. The numerator N and denominator D are `(a_y, a_x)` when `bLower`, else `(a_x, a_y)`.
     - `d = E(N) − E(D)`.
  3. **Tiny quotient:** when `d < −1022`, or `d = −1022` and `M(N) < M(D)`, the quotient is below 2^−1022 and `(h, l) = (0, 0)`; with `bLower` false it is `(ACOT_HI[0], ACOT_LO[0])` instead.
  4. Else, if `d < −30`: `s = N / D` (a normal quotient) and `(h, l) = AtanCore(s, 0, !bLower)`.
  5. Else, scale both arguments exactly by rewriting exponent fields: `D′` is D with exponent field 1023, and `N′` is N with exponent field `1023 + d`. Then `(s, s_l) = Ratio(N′, 0, D′, 0)` and `(h, l) = AtanCore(s, s_l, !bLower)`.
  6. If x is negative (sign bit set): `(h2, l2) = TwoSum(PI_HI, −h)`; `h = h2`; `l = l2 + (PI_LO − l)`.
  7. Return `CopySign(h + l, y)`, with rule 4.
- `CosOfAsin(a) → (c, c_l)`, for `0 ≤ a ≤ 1`:
  1. `(p, pe) = TwoProd(a, a)`; `(w, e) = TwoSum(1.0, −p)`; `w_l = e − pe`.
  2. If `w = 0`, return `(0, 0)`.
  3. `c = Zenith_HardwareSqrt(w)`; `(q, qe) = TwoProd(c, c)`; `c_l = (((w − q) − qe) + w_l) / (2.0 × c)`.
- `Zenith_Asin(x)`, with `a = |x|`:
  1. If `a > 1`, return the canonical NaN. If `a < T27`, return x.
  2. `(c, c_l) = CosOfAsin(a)`.
  3. Choose:
     - if `c = 0`: `(h, l) = (ACOT_HI[0], ACOT_LO[0])`;
     - else if `a ≤ c`: `(h, l) = AtanCore(Ratio(a, 0, c, c_l), false)`;
     - else: `(h, l) = AtanCore(Ratio(c, c_l, a, 0), true)`.
  4. Return `CopySign(h + l, x)`.
- `Zenith_Acos(x)`, with `a = |x|`:
  1. If `a > 1`, return the canonical NaN.
  2. `(c, c_l) = CosOfAsin(a)`.
  3. Choose:
     - if `c = 0`: `(h, l) = (0, 0)`;
     - else if `c ≤ a`: `(h, l) = AtanCore(Ratio(c, c_l, a, 0), false)`;
     - else: `(h, l) = AtanCore(Ratio(a, 0, c, c_l), true)`.
  4. If x is negative: `(h2, l2) = TwoSum(PI_HI, −h)`; `h = h2`; `l = l2 + (PI_LO − l)`.
  5. Return `h + l`.
- `Zenith_Sqrt(x)`: return the canonical NaN for x < 0 (−0 excepted) and for NaN. Otherwise return ±0 for ±0 (after rule 3), +∞ for +∞, and `Zenith_HardwareSqrt(x)` otherwise.

### Exponential and logarithm algorithm steps

**ExpKernel(r, r_l) → E**, for `|r| ≤ 0.35`:
1. `Q = E2 + r × (E3 + r × (E4 + r × (E5 + r × (E6 + r × (E7 + r × (E8 + r × (E9 + r × (E10 + r × (E11 + r × (E12 + r × E13))))))))))`.
2. `E = 1.0 + (r + (r_l + (r × r) × Q))`.

**ExpScale(E, n)**, for E in [0.70, 1.42]:
- If `n > 1023`, return `(E × 2.0) × Pow2(1023)`; this overflows to +∞ exactly when E ≥ 1.
- If `n < −1022`, or `n = −1022` and `E < 1.0`, return +0, because the result is below 2^−1022.
- Otherwise return `E × Pow2(n)`, which is exact and normal.

**ExpReduced(x, x_l)**, for `|x| ≥ T54` and x in the finite range:
1. `n = RoundInt(x × INV_LN2)`.
2. `r_h = x − n × LN2_HI`. Exact: `LN2_HI` has 32 bits, |n| ≤ 1075, and Sterbenz's lemma applies.
3. `t = n × LN2_LO`; `(a, a_e) = TwoSum(r_h, x_l)`; `(r, e2) = TwoSum(a, −t)`; `r_l = e2 + a_e`.
4. Return `ExpScale(ExpKernel(r, r_l), static_cast<int32>(n))`.

**The exponential functions**:

- `Zenith_Exp(x)`:
  - NaN returns the canonical NaN.
  - Return +∞ when `x > EXP_OVERFLOW` (this includes +∞), and +0 when x < −746 (this includes −∞).
  - Return 1.0 when `|x| < T54`.
  - Otherwise return `ExpReduced(x, 0)`.
- `Zenith_Exp2(x)`:
  1. Return +∞ when x ≥ 1024, +0 when x < −1080, and 1.0 when `|x| < T54`.
  2. `n = RoundInt(x)`; `r = x − n`. Exact.
  3. `(h, h_l) = TwoProd(r, LN2_D)`; `l = h_l + r × LN2_DLO`.
  4. Return `ExpScale(ExpKernel(h, l), static_cast<int32>(n))`.

**LogDD(a) → (H, L)**, for a positive normal finite a: ln a as two words.

1. `e = E(a) − 1023`; `m` is a with exponent field 1023, so m is in [1, 2). If `m ≥ SQRT2`, set `m = 0.5 × m` and `e = e + 1`.
2. `j = static_cast<int32>(m × 32.0 + 0.5)` (23 … 45); `c = j × 0.03125`; `f = m − c`. The subtraction is exact by Sterbenz.
3. `(d, d_l) = TwoSum(m, c)`; `s = f / d`; `(p, pe) = TwoProd(s, d)`; `s_l = (((f − p) − pe) − s × d_l) / d`.
4. `z = s × s`; `T = (s × z) × (K1 + z × (K2 + z × (K3 + z × K4)))`. Since `|s| ≤ 0.0112`, the omitted term is below 2^−68 relative.
5. `m_h = 2.0 × s`; `m_l = 2.0 × s_l + T`.
6. `(A, A_l) = (LOGC_HI[j], LOGC_LO[j])`, which is (0, 0) for j = 32.
7. `(h1, l1) = TwoSum(e × LN2_HI, A)`; `(h2, l2) = TwoSum(h1, m_h)`.
8. `Lsum = l2 + (l1 + (m_l + (A_l + e × LN2_LO)))`.
9. Return `FastTwoSum(h2, Lsum)`.

**The logarithms**:

- `Zenith_Ln(x)`, `Zenith_Log2(x)` and `Zenith_Log10(x)` share the special values: NaN or x < 0 returns the canonical NaN; ±0 (after rule 3) returns −∞; +∞ returns +∞.
- Otherwise:
  - `Zenith_Ln` returns H of `LogDD(x)`.
  - `Zenith_Log2`, with `(H, L) = LogDD(x)` and `(q, qe) = TwoProd(H, INV_LN2_HI)`, returns `q + (qe + (L × INV_LN2_HI + H × INV_LN2_LO))`.
  - `Zenith_Log10` is the same with `INV_LN10_HI` and `INV_LN10_LO`.

### Power and cube root algorithm steps

**Zenith_Pow(x, y)**: the operands go through rule 3 first, then the cases below are taken in order.

1. If `y = ±0`, return 1.0 (NaN x included).
2. If `x = 1.0`, return 1.0 (NaN y included).
3. If x or y is NaN, return the canonical NaN.
4. **Zero base.**
   - `x = ±0` and `y < 0`: −∞ when x is −0 and y is an odd integer, else +∞.
   - `x = ±0` and `y > 0`: x when y is an odd integer, else +0.
5. **Infinite exponent.**
   - `y = ±∞` and `|x| = 1`: 1.0.
   - `y = ±∞` otherwise: +∞ when `|x| < 1` and `y < 0`, or `|x| > 1` and `y > 0`; else +0.
6. **Infinite base.**
   - `x = +∞`: +0 for `y < 0`, +∞ for `y > 0`.
   - `x = −∞` and `y < 0`: −0 for an odd integer y, else +0.
   - `x = −∞` and `y > 0`: −∞ for an odd integer y, else +∞.
7. **Negative base.** `sign = 1.0`. If x is negative:
   - a non-integer y returns the canonical NaN;
   - an odd integer y sets `sign = −1.0`.
   - y is an integer when `|y| ≥ 2^52` or `static_cast<double>(static_cast<sint64>(y)) == y`. It is odd when, in addition, `|y| < 2^53` and `static_cast<sint64>(y) & 1`.
8. **Exact shortcuts.**
   - `y = 1`: return x.
   - `y = 2`: return +0 when `|x| < 2^−511`, else `x × x`.
   - `y = −1`: return `CopySign(0, x)` when `|x| > 2^1022`, else `1.0 / x`.
9. `(H, L) = LogDD(|x|)`; `P = y × H`.
   - If `|P| < T54`, return `sign`.
   - If `P > 709.79`, return `sign × ∞`. If `P < −745.2`, return `sign × 0`.
10. `(p, pe) = TwoProd(y, H)`; `pe = pe + y × L`; `(p_h, p_l) = FastTwoSum(p, pe)`.
11. Return `sign × ExpReduced(p_h, p_l)`.

**Zenith_Cbrt(x)**: NaN returns the canonical NaN; ±0 and ±∞ return themselves (after rule 3). Otherwise, with `a = |x|`:

1. `e = E(a) − 1023`; `q = ⌊e / 3⌋` (rounding toward −∞); `rem = e − 3q` (0, 1 or 2); `m` is a with exponent field `1023 + rem`, so m is in [1, 8).
2. `t = 1.0 + (m − 1.0) × INV_SEVEN`.
3. Do twice: `t3 = t × t × t`, evaluated left to right; `t = t × (t3 + 2.0 × m) / (2.0 × t3 + m)`. These are two Halley steps, and the division is applied after the multiplication.
4. `(a2, a2e) = TwoProd(t, t)`; `(b, be) = TwoProd(a2, t)`; `res = (b − m) + (be + a2e × t)`.
5. `t = t − t × res / (2.0 × (t × t × t) + m)`.
6. Return `CopySign(t × Pow2(q), x)`. The product is exact and normal.

### Special values

The binary32 overloads have the same table at binary32 (rule 4). "NaN" always means the canonical NaN.

| Function | Argument | Result |
|---|---|---|
| Sin, Tan, Atan, Asin, Cbrt | ±0 or a subnormal | ±0 (sign kept) |
| Cos | ±0 | 1 |
| Sin, Cos, SinCos, Tan | ±∞, NaN, `|x| > 2^20` | NaN |
| Atan | ±∞ | ±π/2 (`PIO2`) |
| Atan2 | (±0, +0) or (±0, x > 0) / (±0, −0) or (±0, x < 0) | ±0 / ±π (`PI_HI`) |
| Atan2 | (y > 0, ±0) / (y < 0, ±0) | +π/2 / −π/2 |
| Atan2 | (±y finite, +∞) / (±y finite, −∞) | ±0 / ±π |
| Atan2 | (±∞, x finite) / (±∞, +∞) / (±∞, −∞) | ±π/2 / ±π/4 / ±3π/4 |
| Asin, Acos | `|x| > 1`, NaN | NaN |
| Asin / Acos | ±1 | ±π/2 / (+0 for 1, π for −1) |
| Exp | −∞ / +∞ / below −746 / above `EXP_OVERFLOW` | +0 / +∞ / +0 / +∞ |
| Exp2 | −∞ / +∞ / at least 1024 | +0 / +∞ / +∞ |
| Exp, Exp2 | a result below 2^−1022 | +0 |
| Ln, Log2, Log10 | ±0 or a subnormal / x < 0 / +∞ / 1 | −∞ / NaN / +∞ / +0 |
| Pow | the cases of steps 1 to 8 | as stated there (C99 Annex F.10.4.4, with a canonical NaN) |
| Cbrt | ±∞ | ±∞ |
| Sqrt | −0 / x < 0 / +∞ | −0 / NaN / +∞ |

### Error bounds

Errors are relative to the exact mathematical value, in units in the last place of the result's precision. They hold over the stated domain, for results not flushed by rule 4. The basis of each bound:

- The step analysis above: each two-word path leaves only a final rounding plus a below-2^−56 relative tail.
- A design-time measurement of the binary64 steps exactly as written, against a 1,200-bit reference: at least 30,000 arguments per function, including arguments near multiples of π/2 and tiny ratios. The worst cases measured are in parentheses.

The unit property test of "Maths tests" re-checks the bounds against the host library as an informative cross-check.

| Function | Domain | binary64 bound | binary32 bound |
|---|---|---|---|
| Sin, Cos, SinCos | `|x| ≤ 2^20` | 1 ulp (0.74) | 0.5 + 2^−28 ulp |
| Tan | `|x| ≤ 2^20` | 1.5 ulp (0.89) | 0.5 + 2^−28 ulp |
| Atan, Atan2 | every finite argument | 1 ulp (0.51) | 0.5 + 2^−28 ulp |
| Asin, Acos | [−1, 1] | 1 ulp (0.51, 0.71) | 0.5 + 2^−28 ulp |
| Exp | [−708.39, 709.78] | 1 ulp (0.80) | 0.5 + 2^−28 ulp |
| Exp2 | [−1022, 1024) | 1 ulp (0.84) | 0.5 + 2^−28 ulp |
| Ln, Log2, Log10 | positive normal | 1 ulp (0.50) | 0.5 + 2^−28 ulp |
| Pow | positive normal x, normal results | 1.5 ulp (0.88, at `|y ln x|` up to 709) | 0.5 + 2^−28 ulp |
| Cbrt | every normal x | 1 ulp (0.50) | 0.5 + 2^−28 ulp |
| Sqrt | x ≥ 0 | 0.5 ulp (correctly rounded) | 0.5 ulp (correctly rounded) |

How the binary32 bounds follow: each binary32 overload rounds a binary64 result whose error is at most 1.5 × 2^−29 binary32 ulp, so its error is at most 0.5 + 2^−28 ulp. It is the correctly rounded value except in rare cases adjacent to a halfway point.

The steps above also return these values exactly:

- `Zenith_Sin(PIO2) = 1`;
- `Zenith_Atan(1) = Zenith_Atan2(1, 1) = PIO4`;
- `Zenith_Log2(2^k) = k` for every k in [−1022, 1023];
- `Zenith_Log10(1000) = 3` and `Zenith_Log10(1e−200) = −200`;
- `Zenith_Exp2(k) = 2^k` and `Zenith_Pow(2, k) = 2^k` for integer k in range;
- `Zenith_Pow(10, −2)` is the binary64 nearest 0.01;
- `Zenith_Cbrt(27) = 3` and `Zenith_Cbrt(−8) = −2`.

The golden vectors pin them.

### Golden vectors

These vectors are part of the specification. Each one follows from the steps above by evaluating them by hand in binary64, one IEEE operation at a time. They were produced once by an executable model of these steps (the model is not committed and is not the oracle; the steps are). The worker copies them byte for byte into `Zenith2/Engine/ZenithCore/Tests/Fixtures/Transcendental/Zenith_TranscendentalGolden.inl` as `{ TRANSCENDENTAL_GOLDEN_<FUNCTION>, <argument bits>, <second argument bits or 0>, <result bits> }` rows. The suite runs them on every preset, and that is the cross-build bit-identity check.

binary64 (`<function> <argument bits> [<second argument bits>] -> <result bits>`):

```text
Sin    0x3FE0000000000000                    -> 0x3FDEAEE8744B05F0
Sin    0x3FF0000000000000                    -> 0x3FEAED548F090CEE
Sin    0x4000000000000000                    -> 0x3FED18F6EAD1B446
Sin    0xC008000000000000                    -> 0xBFC210386DB6D55B
Sin    0x4059000000000000                    -> 0xBFE03425B78C4DB8
Sin    0x3FF921FB54442D18                    -> 0x3FF0000000000000
Sin    0x3FF921FB56F35EF4                    -> 0x3FF0000000000000
Sin    0x412FFFFF00000000                    -> 0xBFC4CB305757FA66
Sin    0x3E112E0BE826D695                    -> 0x3E112E0BE826D695
Cos    0x3FE0000000000000                    -> 0x3FEC1528065B7D50
Cos    0x3FF0000000000000                    -> 0x3FE14A280FB5068C
Cos    0x4000000000000000                    -> 0xBFDAA22657537205
Cos    0xC008000000000000                    -> 0xBFEFAE04BE85E5D2
Cos    0x4059000000000000                    -> 0x3FEB981DBF665FDF
Cos    0x3FF921FB54442D18                    -> 0x3C91A62633145C07
Cos    0x3FF921FB56F35EF4                    -> 0xBE45798EDDCB3B3A
Cos    0x412FFFFF00000000                    -> 0x3FEF932EC65B1D71
Tan    0x3FE0000000000000                    -> 0x3FE17B4F5BF3474A
Tan    0x3FF0000000000000                    -> 0x3FF8EB245CBEE3A6
Tan    0x3FF8000000000000                    -> 0x402C33ED50B88777
Tan    0xC000000000000000                    -> 0x40017AF62E0950F8
Tan    0x4059000000000000                    -> 0xBFE2CA74D62B5D38
Tan    0x3FF921FB54442D18                    -> 0x434D02967C31CDB5
Atan   0x3F9EB851EB851EB8                    -> 0x3F9EB5F644234B83
Atan   0x3FD3333333333333                    -> 0x3FD2A73A661EAF06
Atan   0x3FE6666666666666                    -> 0x3FE38B112D7BD4AD
Atan   0x3FF0000000000000                    -> 0x3FE921FB54442D18
Atan   0x4004000000000000                    -> 0x3FF30B6D796A4DA8
Atan   0x40F86A0000000000                    -> 0x3FF921F0D7E968A8
Atan   0xBFECCCCCCCCCCCCD                    -> 0xBFE77338A80603BE
Atan2  0x3FF0000000000000 0x4000000000000000 -> 0x3FDDAC670561BB4F
Atan2  0x4000000000000000 0x3FF0000000000000 -> 0x3FF1B6E192EBBE44
Atan2  0xC008000000000000 0xC010000000000000 -> 0xC003FC176B7A8560
Atan2  0x4008000000000000 0xC010000000000000 -> 0x4003FC176B7A8560
Atan2  0x39B4484BFEEBC2A0 0x3FF0000000000000 -> 0x39B4484BFEEBC2A0
Atan2  0x3FF0000000000000 0x37A16C262777579C -> 0x3FF921FB54442D18
Atan2  0xBFE0000000000000 0x3FE0000000000000 -> 0xBFE921FB54442D18
Asin   0x3FB999999999999A                    -> 0x3FB9A49276037884
Asin   0x3FE0000000000000                    -> 0x3FE0C152382D7366
Asin   0x3FECCCCCCCCCCCCD                    -> 0x3FF1EA93705FA172
Asin   0x3FEFFFFDE7210BE9                    -> 0x3FF91C306B2C13AD
Asin   0xBFD3333333333333                    -> 0xBFD380159E14F6FF
Acos   0x3FB999999999999A                    -> 0x3FF787B22CE3F590
Acos   0x3FE0000000000000                    -> 0x3FF0C152382D7366
Acos   0xBFE0000000000000                    -> 0x4000C152382D7366
Acos   0x3FEFFFFDE7210BE9                    -> 0x3F572BA46065AF16
Acos   0xBFEFFFFDE7210BE9                    -> 0x40091F15DFB82062
Acos   0x3E112E0BE826D695                    -> 0x3FF921FB53FF74E9
Exp    0x3FE0000000000000                    -> 0x3FFA61298E1E069C
Exp    0x3FF0000000000000                    -> 0x4005BF0A8B14576A
Exp    0xBFF0000000000000                    -> 0x3FD78B56362CEF38
Exp    0x4024000000000000                    -> 0x40D5829DCF950560
Exp    0xC024000000000000                    -> 0x3F07CD79B5647C9A
Exp    0x4059000000000000                    -> 0x48F3494A9B171BF5
Exp    0x4085E00000000000                    -> 0x7F0D945DF4F8EC8E
Exp    0xC085E00000000000                    -> 0x00D14F2B0FB9307F
Exp    0x40862E3D70A3D70A                    -> 0x7FEFE9CE5C4C52B4
Exp2   0x3FD3333333333333                    -> 0x3FF3B2C47BFF8329
Exp2   0xBFE6666666666666                    -> 0x3FE3B2C47BFF8329
Exp2   0x4025000000000000                    -> 0x4096A09E667F3BCC
Exp2   0xC08F420000000000                    -> 0x016AE89F995AD3AD
Exp2   0x408FFC0000000000                    -> 0x7FE6A09E667F3BCC
Ln     0x3FE0000000000000                    -> 0xBFE62E42FEFA39EF
Ln     0x4000000000000000                    -> 0x3FE62E42FEFA39EF
Ln     0x4008000000000000                    -> 0x3FF193EA7AAD030B
Ln     0x4024000000000000                    -> 0x40026BB1BBB55516
Ln     0x3FF000001AD7F29B                    -> 0x3E7AD7F2847B6492
Ln     0x3FE8000000000000                    -> 0xBFD269621134DB92
Ln     0x54B249AD2594C37D                    -> 0x406CC845B54B54F2
Ln     0x01A56E1FC2F8F359                    -> 0xC085963447F87FB5
Log2   0x4008000000000000                    -> 0x3FF95C01A39FBD68
Log2   0x4024000000000000                    -> 0x400A934F0979A371
Log2   0x3FD3333333333333                    -> 0xBFFBCA9C6F53897B
Log2   0x7E37E43C8800759C                    -> 0x408F24A09F1A8B89
Log10  0x4000000000000000                    -> 0x3FD34413509F79FF
Log10  0x401C000000000000                    -> 0x3FEB0B0B0B78CC3F
Log10  0x3FE0000000000000                    -> 0xBFD34413509F79FF
Log10  0x16687E92154EF7AC                    -> 0xC069000000000000
Pow    0x4000000000000000 0x3FE0000000000000 -> 0x3FF6A09E667F3BCC
Pow    0x4024000000000000 0xC000000000000000 -> 0x3F847AE147AE147B
Pow    0x3FE0000000000000 0x4003333333333333 -> 0x3FC8406003B2AE5D
Pow    0x3FF0E147AE147AE1 0x4003333333333333 -> 0x3FF231A3A81E1136
Pow    0x401C000000000000 0x3FD5555555555555 -> 0x3FFE9B5DBA58189D
Pow    0xC008000000000000 0x4014000000000000 -> 0xC06E600000000000
Pow    0x3FF00068DB8BAC71 0x40C3880000000000 -> 0x4005BEC34AABBFD3
Pow    0x4004000000000000 0xC072C00000000000 -> 0x27256E1FC2F8F359
Pow    0x3FD3333333333333 0x3FD3333333333333 -> 0x3FE64C8E84C5F4E9
Cbrt   0x4000000000000000                    -> 0x3FF428A2F98D728B
Cbrt   0x4008000000000000                    -> 0x3FF7137449123EF6
Cbrt   0x4024000000000000                    -> 0x40013C484138704F
Cbrt   0xBF50624DD2F1A9FC                    -> 0xBFB999999999999A
Cbrt   0x7E37E43C8800759C                    -> 0x54B249AD2594C37D
Cbrt   0x01A56E1FC2F8F359                    -> 0x2B2BFF2EE48E0530
Sqrt   0x4000000000000000                    -> 0x3FF6A09E667F3BCD
Sqrt   0x4008000000000000                    -> 0x3FFBB67AE8584CAA
Sqrt   0x3FE0000000000000                    -> 0x3FE6A09E667F3BCD
```

binary32:

```text
Sin    0x3F000000            -> 0x3EF57744
Sin    0x40000000            -> 0x3F68C7B7
Sin    0x447A0000            -> 0x3F53AE61
Cos    0x3F000000            -> 0x3F60A940
Cos    0x40000000            -> 0xBED51133
Cos    0x447A0000            -> 0x3F0FF813
Tan    0x3F000000            -> 0x3F0BDA7B
Tan    0x3F99999A            -> 0x40249E23
Atan   0x3E99999A            -> 0x3E9539D4
Atan   0x40400000            -> 0x3F9FE0BB
Atan2  0x3F800000 0xC0000000 -> 0x402B6374
Atan2  0xC0000000 0x3F800000 -> 0xBF8DB70D
Asin   0x3F000000            -> 0x3F060A92
Asin   0xBF666666            -> 0xBF8F549B
Acos   0x3F000000            -> 0x3F860A92
Acos   0xBF666666            -> 0x402C323B
Exp    0x3F800000            -> 0x402DF854
Exp    0xC0A00000            -> 0x3BDCC9FF
Exp    0x42A00000            -> 0x792ABBCE
Exp2   0x3E99999A            -> 0x3F9D9624
Exp2   0xC2F10000            -> 0x033504F3
Ln     0x40400000            -> 0x3F8C9F54
Ln     0x3DCCCCCD            -> 0xC0135D8E
Log2   0x41200000            -> 0x40549A78
Log10  0x40400000            -> 0x3EF4493D
Pow    0x3F000000 0x4019999A -> 0x3E4202FF
Pow    0x40000000 0x3ED55555 -> 0x3FAADC08
Pow    0xC0000000 0x40400000 -> 0xC1000000
Cbrt   0x40A00000            -> 0x3FDAE07E
Cbrt   0xBE4CCCCD            -> 0xBF15B5AF
Sqrt   0x40000000            -> 0x3FB504F3
```

### The CRT maths mode

- `Zenith_ApplyCrtMathMode` is the first statement of every `main` and `SDL_main` of the programme: the Player's entries, every tool, test, benchmark and integration-suite entry. It runs before the root heap is installed and before any thread starts.
  - On Windows x64 its body, in `Internal/Windows/Zenith_CrtMathMode_Windows.cpp`, calls `_set_FMA3_enable(0)`.
  - On Android and Linux, in `Internal/Android/` and `Internal/Linux/`, the body is empty: Bionic and glibc choose no transcendental implementation from a process flag.
- `Zenith_IsCrtMathModeApplied` reads the flag back: `_get_FMA3_enable() == 0` on Windows, true elsewhere.
- The flag is defence in depth only. The guarantee of "Transcendental scope and guarantee" never depends on it, because no Zenith2 function above calls the CRT; it covers third-party code that does, for example a library's own `std::exp`.
- ZenithTestSupport's test main calls it ("Test main steps" of `Zenith2/Docs/Design/CORE/Memory.md`). The Player's entries are MOD's (cross-area row C20). Every other `main` follows the core rule in `Zenith2/Docs/Architecture.md` → "Determinism guarantee and mechanisms" ([Z2-N-ARCH-889]).
- `Internal/Zenith_CrtMathMode.h` also declares `void Zenith_SetCrtFma3EnabledForTest(bool bEnabled)` (Windows `_set_FMA3_enable`, elsewhere nothing) for the independence test alone.

## Units

<!-- elaboration -->
Every quantity in an engine API is in the unit of this table unless its name carries a unit suffix. The contract block field `Units` names the units of every quantity a type exposes (`Zenith2/Docs/Contracts.md` → "Contract blocks per type").

| Quantity | Unit | Type in APIs | Name suffix when another unit is used |
|---|---|---|---|
| length, position | metre | `float` | — (no other length unit exists in engine APIs; texel and pixel counts are integers named `…Pixels`/`…Texels`) |
| mass | kilogram | `float` | — |
| simulated time step | second | `float` (`fDeltaSeconds`) | — |
| durations and timestamps | microsecond | `uint64` | `Us`; nanoseconds `Ns` for profiler timestamps; milliseconds `Ms` only in configuration and budget values |
| angle | radian | `float` | `Degrees` in editor-facing and authoring-document fields only, converted at load |
| velocity, acceleration | m/s, m/s² | `float` / `Zenith_Vector3` | — |
| angular velocity | rad/s | `float` / `Zenith_Vector3` | — |
| force, torque | newton, newton-metre | `float` / `Zenith_Vector3` | — |
| colour | linear Rec.709 RGB, straight (not premultiplied) alpha, in `float` | `Zenith_Colour` | `Srgb` for 8-bit sRGB-encoded values |
| memory | byte | `uint64` | `Bytes`; budget rows use GB = 10^9 and MB = 10^6 (`Zenith2/Docs/Budgets.md` → "Memory budgets"); parser limits use KiB/MiB (2^10, 2^20) as their table states |
| frequency | hertz | `uint32` / `float` | `Hz` |
| percentage | 0 … 100 | `uint32` | `Percent` |
| fraction | 0 … 1 | `float` | — |

Light units (lux, candela, nits), audio levels (decibels) and physical-material quantities are specified by LIGHT, AUDIO and PHYS in their design documents and follow the suffix rule for anything not SI.

## Coordinate systems

<!-- elaboration -->
| Space | Convention |
|---|---|
| world and object space | right-handed; +X right, +Y up, +Z toward the viewer; `Cross(+X, +Y) = +Z` |
| model front | +Z: an asset's front faces +Z, a character's locomotion forward is its local +Z; this is glTF 2.0's convention, so a glTF import needs no axis change (CONTENT converts other sources on import) |
| view forward | −Z: a camera, a spot light and a directional light emit or look along their local −Z; view space is right-handed with the camera looking down −Z and +Y up |
| clip space | produced by the `Zenith_Matrix4` projection builders: +X right and +Y up in [−1, 1], depth in [0, 1]; standard Z maps the near plane to 0 and the far plane to 1; reversed Z maps near to 1 and far to 0 (the depth mode per view is `Zenith2/Docs/Rendering.md` → "Render views") |
| framebuffer and texture coordinates | origin at the top-left, +Y down, in pixels; texture UV (0, 0) is the top-left texel corner; how each RHI maps clip space to it is RHI's (below) |
| 2D | the 2D module's world is the XY plane with +X right, +Y up, metres; its camera maps metres to pixels (TWOD); a 2D transform is a `Zenith_Matrix4` with z = 0 (built with `Zenith_MakeTRS` from a translation `(x, y, 0)`, a rotation about +Z and a scale `(sx, sy, 1)`), and `Zenith_Matrix3` carries rotation, scale and normal transforms alone, never a 2D affine transform |
| transform composition | matrices act on column vectors: `p′ = M · p`; a local-to-world matrix is `M = T · R · S`; world = parent × local |
| Euler angles | `(yaw, pitch, roll)` about (+Y, +X, +Z), composed `R = R_y(yaw) · R_x(pitch) · R_z(roll)`; positive angles rotate counter-clockwise when looking from the positive axis toward the origin (right-hand rule); yaw 0 faces +Z and positive yaw turns +Z toward +X |
| large coordinates | `float` positions keep about 0.25 mm resolution at 2 km from the origin; worlds larger than the W-Outdoor scale use STREAM's large-coordinate handling (inventory item Z2-I-STREAM-05, `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1") |

<!-- assumption: pending RHI -->
The Vulkan backend flips clip-space Y with a negative viewport height (core since Vulkan 1.1), so the +Y-up clip space of the projection builders reaches the top-left framebuffer origin on both RHIs; D3D12 needs no flip.
<!-- /assumption -->

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Vector.h
constexpr Zenith_Vector3 xVECTOR3_ZERO{ 0.0f, 0.0f, 0.0f };
constexpr Zenith_Vector3 xVECTOR3_ONE{ 1.0f, 1.0f, 1.0f };
constexpr Zenith_Vector3 xVECTOR3_RIGHT{ 1.0f, 0.0f, 0.0f };
constexpr Zenith_Vector3 xVECTOR3_UP{ 0.0f, 1.0f, 0.0f };
constexpr Zenith_Vector3 xVECTOR3_MODEL_FRONT{ 0.0f, 0.0f, 1.0f };
constexpr Zenith_Vector3 xVECTOR3_VIEW_FORWARD{ 0.0f, 0.0f, -1.0f };
```

## Vector types

All vector types are aggregates of `float` members with alignment 4, trivially copyable, and match the reflected `vec2`, `vec3` and `vec4` layouts (8, 12, 16 bytes, alignment 4, `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file").

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Vector.h
/// @contract value Zenith_Vector2
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a value type that compound operators
///   (+=, *=) and component stores assign in place, and that containers and components reassign.
/// Invalidation: None.
/// Units: The caller's; positions and lengths are metres by the Units table of Zenith2/Docs/Design/CORE/Maths.md.
/// Errors: Normalize of a vector whose squared length is below fMATHS_NORMALIZE_EPSILON is ZENITH_ASSERT;
///   TryNormalize and NormalizeOr are the checked forms.
/// Version: since Z2 Stage 1. Layout fixed: two floats, x then y; serialized through reflection, never by layout.
/// @endcontract
struct Zenith_Vector2
{
	float m_fX = 0.0f;
	float m_fY = 0.0f;
};

/// @contract value Zenith_Vector3
/// (fields as Zenith_Vector2; three floats x, y, z)
/// @endcontract
struct Zenith_Vector3
{
	float m_fX = 0.0f;
	float m_fY = 0.0f;
	float m_fZ = 0.0f;
};

/// @contract value Zenith_Vector4
/// (fields as Zenith_Vector2; four floats x, y, z, w)
/// @endcontract
struct Zenith_Vector4
{
	float m_fX = 0.0f;
	float m_fY = 0.0f;
	float m_fZ = 0.0f;
	float m_fW = 0.0f;
};
static_assert(sizeof(Zenith_Vector2) == 8 && sizeof(Zenith_Vector3) == 12 && sizeof(Zenith_Vector4) == 16);
static_assert(alignof(Zenith_Vector3) == 4 && alignof(Zenith_Vector4) == 4);
```

In the header each contract block is written out in full (the parenthesised shorthand above abbreviates this document only).

Operations (each a `constexpr` or inline free function or operator, for `V` in `Zenith_Vector2`, `Zenith_Vector3`, `Zenith_Vector4` unless noted):

| Operation | Declaration | Definition |
|---|---|---|
| arithmetic | `V operator+(V, V)`, `operator−(V, V)`, `operator−(V)`, `operator*(V, float)`, `operator*(float, V)`, `operator/(V, float)`, compound forms | component-wise; `/` by a scalar multiplies by its reciprocal only when the reciprocal is exact, otherwise divides each component (so results match scalar code exactly) |
| component-wise | `V Zenith_Multiply(V, V)`, `Zenith_Divide(V, V)`, `Zenith_Min(V, V)`, `Zenith_Max(V, V)`, `Zenith_Abs(V)`, `Zenith_Clamp(V, V, V)`, `Zenith_Lerp(V, V, float)` | per component |
| equality | `bool operator==(V, V)` | exact, per component |
| products | `float Zenith_Dot(V, V)`; `Zenith_Vector3 Zenith_Cross(Zenith_Vector3, Zenith_Vector3)`; `float Zenith_Cross(Zenith_Vector2, Zenith_Vector2)` (the z of the 3D cross) | dot sums in index order: `x·x′ + y·y′ + z·z′` evaluated left to right |
| length | `float Zenith_Length(V)`, `Zenith_LengthSquared(V)`, `Zenith_Distance(V, V)`, `Zenith_DistanceSquared(V, V)` | `std::sqrt` of the dot |
| normalisation | `V Zenith_Normalize(V)` (asserting), `bool Zenith_TryNormalize(V, V& xOut)`, `V Zenith_NormalizeOr(V, V xFallback)` | degenerate below `fMATHS_NORMALIZE_EPSILON` squared length |
| geometry (Vector3) | `Zenith_Reflect(V xIncident, V xUnitNormal)`, `Zenith_Project(V, V xOnto)`, `Zenith_Reject(V, V xFrom)`, `Zenith_AngleBetween(V, V)` (radians, `atan2(|a×b|, a·b)`) | textbook definitions |
| orthonormal basis | `void Zenith_BuildOrthonormalBasis(Zenith_Vector3 xUnitN, Zenith_Vector3& xT, Zenith_Vector3& xB)` | the branchless construction of Duff et al. (2017) |
| finiteness | `bool Zenith_IsFinite(V)` | every component finite |
| near-equality | `bool Zenith_IsNearlyEqual(V, V, float fAbs, float fRel)` | per component |
| conversion | `Zenith_Vector4 Zenith_ToPoint(Zenith_Vector3)` (w = 1), `Zenith_ToDirection` (w = 0), `Zenith_Vector3 Zenith_XYZ(Zenith_Vector4)` | — |

## Matrix types

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Matrix.h
/// @contract value Zenith_Matrix3
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a value type that compound operators
///   (+=, *=) and component stores assign in place, and that containers and components reassign.
/// Invalidation: None.
/// Units: Dimensionless (rotation and scale).
/// Errors: TryInverse returns false for |determinant| below 1e-30; Inverse asserts.
/// Version: since Z2 Stage 1. Layout: three rows of three floats, row-major, acting on column vectors.
/// @endcontract
struct Zenith_Matrix3
{
	Zenith_Vector3 m_axRows[3];
};

/// @contract value Zenith_Matrix4
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a value type that compound operators
///   (+=, *=) and component stores assign in place, and that containers and components reassign.
/// Invalidation: None.
/// Units: Translation column in metres; projection builders take radians and metres.
/// Errors: TryInverse returns false for |determinant| below 1e-30; InverseAffine asserts that the last row is
///   (0, 0, 0, 1) and the upper 3×3 is invertible; projection builders assert near > 0, far > near (finite
///   forms) and aspect > 0.
/// Version: since Z2 Stage 1. Layout: four rows of four floats, row-major, acting on column vectors, so
///   element (r, c) is m_axRows[r] component c and the translation is (m_axRows[0].w, [1].w, [2].w);
///   copying rows 0 to 2 gives the row-major float3x4 of Zenith2/Docs/Rendering.md "GPU scene tables and pools".
/// @endcontract
struct Zenith_Matrix4
{
	Zenith_Vector4 m_axRows[4];
};
static_assert(sizeof(Zenith_Matrix4) == 64 && alignof(Zenith_Matrix4) == 4);
```

Operations on `Zenith_Matrix3` (free functions; rotation, scale and normal transforms):

| Function | Definition |
|---|---|
| `Zenith_Matrix3 Zenith_Matrix3Identity()` | — |
| `Zenith_Matrix3 operator*(const Zenith_Matrix3&, const Zenith_Matrix3&)`, `Zenith_Vector3 operator*(const Zenith_Matrix3&, Zenith_Vector3)` | row-by-column sums in index order |
| `Zenith_Matrix3 Zenith_Transpose(const Zenith_Matrix3&)`, `float Zenith_Determinant(const Zenith_Matrix3&)`, `bool Zenith_TryInverse(const Zenith_Matrix3&, Zenith_Matrix3& xOut)` | adjugate over determinant; false for \|determinant\| below 1e-30 |
| `Zenith_Matrix3 Zenith_Inverse(const Zenith_Matrix3&)` | the same adjugate over determinant; `ZENITH_ASSERT` that \|determinant\| is at least 1e-30 (for matrices known to be invertible, such as a rotation times a non-zero scale) |
| `Zenith_Matrix3 Zenith_MakeScale3(Zenith_Vector3 xScale)` | the diagonal matrix of `xScale` |
| `Zenith_Matrix3 Zenith_ToMatrix3(Zenith_Quaternion)` | the rotation matrix of a unit quaternion |
| `Zenith_Matrix3 Zenith_GetUpper3x3(const Zenith_Matrix4&)` | rows 0 … 2, columns 0 … 2 |
| `bool Zenith_MakeNormalMatrix(const Zenith_Matrix4& xLocalToWorld, Zenith_Matrix3& xOut)` | the inverse-transpose of the upper 3×3; false when it is singular |

Operations on `Zenith_Matrix4` (free functions):

| Function | Definition |
|---|---|
| `Zenith_Matrix4 Zenith_Matrix4Identity()` | — |
| `Zenith_Matrix4 operator*(const Zenith_Matrix4&, const Zenith_Matrix4&)` | `C[r][c] = Σ_k A[r][k] × B[k][c]`, summed k = 0 … 3 in order |
| `Zenith_Vector4 operator*(const Zenith_Matrix4&, Zenith_Vector4)` | `v′[r] = Σ_k M[r][k] × v[k]` |
| `Zenith_Vector3 Zenith_TransformPoint(const Zenith_Matrix4&, Zenith_Vector3)` | w = 1, no divide (affine matrices) |
| `Zenith_Vector3 Zenith_TransformPointProjective(const Zenith_Matrix4&, Zenith_Vector3)` | w = 1, then divides by the resulting w (asserting w ≠ 0) |
| `Zenith_Vector3 Zenith_TransformDirection(const Zenith_Matrix4&, Zenith_Vector3)` | w = 0 |
| `Zenith_Matrix4 Zenith_Transpose(const Zenith_Matrix4&)` | — |
| `float Zenith_Determinant(const Zenith_Matrix4&)` | cofactor expansion with 2×2 sub-determinants |
| `bool Zenith_TryInverse(const Zenith_Matrix4&, Zenith_Matrix4& xOut)` | adjugate over determinant |
| `Zenith_Matrix4 Zenith_InverseAffine(const Zenith_Matrix4&)` | inverse of the 3×3 block, translation `−R⁻¹t` |
| `Zenith_Matrix4 Zenith_MakeTranslation(Zenith_Vector3)`, `Zenith_MakeScale(Zenith_Vector3)`, `Zenith_MakeRotation(Zenith_Quaternion)` | — |
| `Zenith_Matrix4 Zenith_MakeTRS(Zenith_Vector3 xT, Zenith_Quaternion xR, Zenith_Vector3 xS)` | `T · R · S`, built directly (column c of the 3×3 block is `S[c] ×` column c of `R`) |
| `bool Zenith_DecomposeTRS(const Zenith_Matrix4&, Zenith_Vector3& xT, Zenith_Quaternion& xR, Zenith_Vector3& xS)` | column lengths are the scale; a negative determinant negates the x scale; false when a scale is below 1e-12 |
| `Zenith_Matrix4 Zenith_MakeLookAt(Zenith_Vector3 xEye, Zenith_Vector3 xTarget, Zenith_Vector3 xUp)` | view matrix: `f = normalize(target − eye)`, `s = normalize(f × up)`, `u = s × f`; rows `(s, −s·eye)`, `(u, −u·eye)`, `(−f, f·eye)`, `(0, 0, 0, 1)` |
| `Zenith_Matrix4 Zenith_MakePerspectiveStandardZ(float fFovY, float fAspect, float fNear, float fFar)` | `g = 1 / tan(fFovY / 2)`; rows `(g/aspect, 0, 0, 0)`, `(0, g, 0, 0)`, `(0, 0, far/(near − far), near·far/(near − far))`, `(0, 0, −1, 0)` |
| `Zenith_Matrix4 Zenith_MakePerspectiveReversedZ(float fFovY, float fAspect, float fNear, float fFar)` | row 2 is `(0, 0, near/(far − near), near·far/(far − near))` |
| `Zenith_Matrix4 Zenith_MakePerspectiveReversedZInfinite(float fFovY, float fAspect, float fNear)` | row 2 is `(0, 0, 0, near)` |
| `Zenith_Matrix4 Zenith_MakeOrthographic(float fLeft, float fRight, float fBottom, float fTop, float fNear, float fFar)` | standard Z: rows `(2/(r−l), 0, 0, −(r+l)/(r−l))`, `(0, 2/(t−b), 0, −(t+b)/(t−b))`, `(0, 0, −1/(f−n), −n/(f−n))`, `(0, 0, 0, 1)` |
| `void Zenith_StoreRows3x4(const Zenith_Matrix4&, Zenith_Span<float> xOut12)` | rows 0 … 2 in order (the GPU-scene layout) |

<!-- informative -->
Row-major storage with column vectors keeps the translation in the fourth column, so the first three rows are exactly the row-major `float3x4` "world from local" of the GPU scene tables and Slang's row-major parameter layout reads the same bytes (`Zenith2/Docs/Rendering.md` → "Shader system and offline compilation"). The projection builders are CORE maths; which depth mode a view uses is Flux's decision per view.
<!-- /informative -->

## Zenith_Quaternion

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Quaternion.h
/// @contract value Zenith_Quaternion
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a value type that compound operators
///   (+=, *=) and component stores assign in place, and that containers and components reassign.
/// Invalidation: None.
/// Units: Angles in radians.
/// Errors: rotation functions expect a unit quaternion (ZENITH_ASSERT |length − 1| ≤ 1e-3); Normalize of a
///   zero quaternion asserts and NormalizeOrIdentity is the checked form.
/// Version: since Z2 Stage 1. Layout: x, y, z, w floats (w the scalar part), matching the reflected quat
///   (identity [0, 0, 0, 1]). Hamilton convention.
/// @endcontract
struct Zenith_Quaternion
{
	float m_fX = 0.0f;
	float m_fY = 0.0f;
	float m_fZ = 0.0f;
	float m_fW = 1.0f;
};
static_assert(sizeof(Zenith_Quaternion) == 16);
```

| Function | Definition |
|---|---|
| `Zenith_Quaternion operator*(Zenith_Quaternion xA, Zenith_Quaternion xB)` | Hamilton product; `A * B` applies B first, then A (matching matrix composition) |
| `Zenith_Vector3 Zenith_Rotate(Zenith_Quaternion, Zenith_Vector3)` | `v + 2w(q × v) + 2 q × (q × v)` with `q` the vector part |
| `Zenith_Quaternion Zenith_MakeAxisAngle(Zenith_Vector3 xUnitAxis, float fRadians)` | `(axis × sin(θ/2), cos(θ/2))` |
| `Zenith_Quaternion Zenith_MakeEuler(float fYaw, float fPitch, float fRoll)` | `q_y(yaw) * q_x(pitch) * q_z(roll)` (the Euler convention of "Coordinate systems") |
| `void Zenith_ToEuler(Zenith_Quaternion, float& fYaw, float& fPitch, float& fRoll)` | the inverse, with pitch clamped to ±π/2 at the singularity (roll set to 0 there) |
| `Zenith_Quaternion Zenith_Conjugate(Zenith_Quaternion)`, `Zenith_Inverse` (conjugate over squared length) | — |
| `float Zenith_Dot(Zenith_Quaternion, Zenith_Quaternion)` | x, y, z, w order |
| `Zenith_Quaternion Zenith_Normalize(Zenith_Quaternion)`, `Zenith_NormalizeOrIdentity` | — |
| `Zenith_Quaternion Zenith_Nlerp(a, b, t)` | lerp toward `b` or `−b` (the nearer), then normalise |
| `Zenith_Quaternion Zenith_Slerp(a, b, t)` | shortest path; falls back to `Zenith_Nlerp` when the cosine exceeds 0.9995 |
| `Zenith_Quaternion Zenith_MakeFromTo(Zenith_Vector3 xUnitFrom, Zenith_Vector3 xUnitTo)` | shortest arc; antiparallel inputs rotate π about an axis orthogonal to `from` chosen by `Zenith_BuildOrthonormalBasis` |
| `Zenith_Quaternion Zenith_MakeLookRotation(Zenith_Vector3 xFront, Zenith_Vector3 xUp)` | the rotation taking the model front (+Z) to `xFront` with +Y as close to `xUp` as possible; for a camera, `Zenith_MakeLookRotation(−viewDirection, up)` |
| `Zenith_Matrix3 Zenith_ToMatrix3(Zenith_Quaternion)`, `Zenith_Quaternion Zenith_FromMatrix3(const Zenith_Matrix3&)` | Shepperd's method choosing the largest diagonal term |
| `float Zenith_AngleBetween(Zenith_Quaternion, Zenith_Quaternion)` | `2 acos(min(1, |dot|))` |
| `bool Zenith_IsFinite(Zenith_Quaternion)`, `Zenith_IsNearlyEqualRotation(a, b, fRadians)` | the second treats `q` and `−q` as equal |

## Zenith_Colour

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Colour.h
/// @contract value Zenith_Colour
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a value type that compound operators
///   (+=, *=) and component stores assign in place, and that containers and components reassign.
/// Invalidation: None.
/// Units: Linear Rec.709 RGB with straight alpha; 1.0 is the encoding's white, values above 1.0 are HDR.
/// Errors: None; conversions clamp to [0, 1] before encoding to 8 bits.
/// Version: since Z2 Stage 1. Layout: r, g, b, a floats, matching the reflected colour.
/// @endcontract
struct Zenith_Colour
{
	float m_fR = 0.0f;
	float m_fG = 0.0f;
	float m_fB = 0.0f;
	float m_fA = 1.0f;
};

float Zenith_SrgbToLinear(float fEncoded);   // c ≤ 0.04045 ? c / 12.92 : ((c + 0.055) / 1.055)^2.4
float Zenith_LinearToSrgb(float fLinear);    // c ≤ 0.0031308 ? 12.92 c : 1.055 c^(1/2.4) − 0.055
Zenith_Colour Zenith_ColourFromSrgb8(uint8 uR, uint8 uG, uint8 uB, uint8 uA);   // alpha linear
uint32 Zenith_ColourToSrgb8Packed(Zenith_Colour xColour);   // R in bits 0–7, then G, B, A; round half up after clamping
Zenith_Colour Zenith_Premultiply(Zenith_Colour xColour);
```

## Bounds types

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Bounds.h
struct Zenith_AABB { Zenith_Vector3 m_xMin; Zenith_Vector3 m_xMax; };        // empty when any min > max
struct Zenith_Sphere { Zenith_Vector3 m_xCentre; float m_fRadius = 0.0f; };
struct Zenith_Plane { Zenith_Vector3 m_xNormal; float m_fDistance = 0.0f; }; // points p with n·p + d = 0; n unit; positive side n·p + d > 0
struct Zenith_Ray { Zenith_Vector3 m_xOrigin; Zenith_Vector3 m_xDirection; };  // direction unit
struct Zenith_Frustum { Zenith_Plane m_axPlanes[6]; };                        // left, right, bottom, top, near, far; normals point inward
```

Each type carries a `value` contract block (units metres; errors: a non-unit normal or direction is `ZENITH_ASSERT`). Functions:

| Function | Definition |
|---|---|
| `Zenith_AABB Zenith_AABBEmpty()` | min = +∞, max = −∞ per component |
| `Zenith_AABB Zenith_Merge(Zenith_AABB, Zenith_AABB)`, `Zenith_Merge(Zenith_AABB, Zenith_Vector3)` | component min and max |
| `bool Zenith_Contains(Zenith_AABB, Zenith_Vector3)`, `Zenith_Intersects(Zenith_AABB, Zenith_AABB)` | inclusive bounds |
| `Zenith_AABB Zenith_Transform(const Zenith_Matrix4&, Zenith_AABB)` | Arvo's method (per-element min/max of `M[r][c] × min/max`) |
| `bool Zenith_Intersects(Zenith_Sphere, Zenith_Sphere)`, `Zenith_Intersects(Zenith_Sphere, Zenith_AABB)` | squared distances |
| `float Zenith_SignedDistance(Zenith_Plane, Zenith_Vector3)` | `n·p + d` |
| `Zenith_Frustum Zenith_ExtractFrustum(const Zenith_Matrix4& xViewProjection, bool bReversedZ, bool bInfiniteFar)` | Gribb–Hartmann plane extraction for [0, 1] depth, normalised; with `bInfiniteFar` the far plane is written as normal (0, 0, 0) and `m_fDistance = std::numeric_limits<float>::max()`, so every point is on its positive side |
| `bool Zenith_IsOutside(const Zenith_Frustum&, Zenith_AABB)` | conservative: true only when the box is entirely on the negative side of one plane (p-vertex test) |
| `bool Zenith_RayAABB(Zenith_Ray, Zenith_AABB, float& fTNear, float& fTFar)` | slab method with IEEE handling of zero direction components |
| `bool Zenith_RaySphere(Zenith_Ray, Zenith_Sphere, float& fT)`, `Zenith_RayPlane(Zenith_Ray, Zenith_Plane, float& fT)` | nearest non-negative hit |

## Zenith_Hash64

`Zenith_Hash64` is the 64-bit hash of type IDs, graph structure hashes, script random draws, hashed containers and evidence fingerprints (`Zenith2/Docs/Architecture.md` → "Named selection decisions", D-SEL-HASH (open, Stage 1)); its seed is 0 unless a caller states otherwise.

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Hash.h
/// @contract functions Zenith_Hash
/// Ownership: Pure functions; the stream object holds its state inline.
/// Thread affinity: ANY_THREAD (functions); Zenith_Hash64Stream is OWNER_THREAD.
/// Invalidation: None.
/// Units: Byte spans.
/// Errors: None; every input is valid.
/// Version: since Z2 Stage 2. Output is bit-identical on every platform and preset for the pinned library
///   version; a change of algorithm or version changes type IDs and persisted keys and is a ruling with a
///   re-cook and a replay-format version bump (Zenith2/Docs/Scripting.md "Determinism rules and randomness").
/// @endcontract
uint64 Zenith_Hash64(Zenith_ByteSpan xBytes, uint64 ulSeed = 0);
uint64 Zenith_Hash64(Zenith_StringView strText, uint64 ulSeed = 0);
uint64 Zenith_Hash64U64(uint64 ulValue, uint64 ulSeed = 0);     // the 8 little-endian bytes

struct Zenith_Hash128
{
	uint64 m_ulLow = 0;
	uint64 m_ulHigh = 0;
	friend constexpr bool operator==(const Zenith_Hash128&, const Zenith_Hash128&) = default;
	friend constexpr std::strong_ordering operator<=>(const Zenith_Hash128&, const Zenith_Hash128&) = default;
};
Zenith_Hash128 Zenith_Hash128Of(Zenith_ByteSpan xBytes, uint64 ulSeed = 0);

class Zenith_Hash64Stream
{
public:
	explicit Zenith_Hash64Stream(uint64 ulSeed = 0);
	void Update(Zenith_ByteSpan xBytes);
	void UpdateU32(uint32 uValue);          // 4 little-endian bytes
	void UpdateU64(uint64 ulValue);         // 8 little-endian bytes
	void UpdateString(Zenith_StringView strText);   // its uint64 length, then its bytes
	uint64 Finish() const;    // equals Zenith_Hash64 of the concatenated bytes
	void Reset(uint64 ulSeed = 0);
private:
	alignas(64) uint8 m_auState[640];                // the pinned library's streaming state, placed in Internal code
};
```

| D-SEL-HASH path | `Zenith_Hash64` | `Zenith_Hash128Of` | `Zenith_Hash64Stream` |
|---|---|---|---|
| (1) XXH3-64 for in-memory keys, XXH3-128 for persisted keys | `XXH3_64bits_withSeed` | `XXH3_128bits_withSeed` | `XXH3_64bits_reset_withSeed` / `_update` / `_digest` over `XXH3_state_t` |
| (2) XXH64 throughout | `XXH64` | the pair `(XXH64(seed), XXH64(seed ^ 0x9E3779B97F4A7C15))` as low and high words | `XXH64_reset` / `_update` / `_digest` |
| (3) the hand-rolled baseline | exists only inside the D-SEL-HASH harness, never in ZenithCore | — | — |

- Implementation: `Zenith2/Engine/ZenithCore/Internal/Hash/Zenith_Hash.cpp`, including `xxhash.h` from the manifest entry (owner ZenithCore) with `XXH_INLINE_ALL` in that one TU; `static_assert(sizeof(XXH3_state_t) <= 640 && alignof(XXH3_state_t) <= 64)` there, so the public header never names an xxHash type.
- Persisted and cross-platform uses (type IDs, DDC keys, evidence fingerprints) take their bytes in little-endian order from explicit serialisation, never from a struct's memory ([Z2-N-ARCH-884], Z2-D-038).
- Until the ruling lands, `Zenith_Hash.h` exists as the API-only slice (declarations and contract), and no consumer ships a persisted hash.

## Zenith_Sha256

<!-- elaboration -->
`Zenith_Sha256` is SHA-256 exactly as FIPS 180-4 (the Secure Hash Standard: §4.1.2 functions, §4.2.2 constants, §5.1.1 padding, §5.3.3 initial hash value, §6.2 computation) defines it. It lives in ZenithCore so that every library that stores or verifies a digest uses one implementation: ZenithScene derives authoring IDs from it and ZenithDocument records journal digests with it (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Derivation"; `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Journal format"), and ZenithPlatform, at layer 1 and therefore unable to link ZenithDocument, checks each patch archive's digest in the patch mount (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Patch mount verification", [Z2-N-ARCH-955]).

### SHA-256 outside D-SEL-HASH

- `Zenith_Sha256` is a fixed algorithm, not a selection. D-SEL-HASH (open, Stage 1) chooses the algorithm behind `Zenith_Hash64` and never reaches `Zenith_Sha256.h`; no ruling changes SHA-256. Its digests are persisted — derived authoring IDs in authored documents, cooked payloads and saves, archive digests in update manifests — and a per-document migration step cannot re-derive a stored identity, so the algorithm and its byte order are fixed forever.
- `Zenith_Hash64` stays the hash of in-memory keys, type IDs, cache keys and evidence fingerprints. `Zenith_Sha256` is for digests that must agree with tools outside the engine (Python's `hashlib`, OpenSSL, `sha256sum`), which is also how its test vectors are produced.
- SHA-256 is an algorithm, not custom infrastructure: it is no container, allocator, heap wrapper or threading primitive, so it carries no justification record ([Z2-N-CONTRACT-278]). It is written from the standard; no third-party library is linked for it and the manifest gains no entry.

### Zenith_Sha256 declarations

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Sha256.h
#include "Zenith_Span.h"
#include "Zenith_StringView.h"

constexpr uint64 ulZENITH_SHA256_DIGEST_BYTES = 32;
constexpr uint64 ulZENITH_SHA256_HEX_CHARS = 64;
constexpr uint64 ulZENITH_SHA256_MAX_MESSAGE_BYTES = (1ull << 61) - 1;   // FIPS 180-4: a message is shorter than 2^64 bits

/// @contract value Zenith_Sha256Value
/// Ownership: Plain value of 32 bytes; copied freely.
/// Thread affinity: IMMUTABLE. A value is never shared mutably. Members non-const: Finish writes the digest into a
///   default-constructed value, and records that hold one are reassigned.
/// Invalidation: None.
/// Units: The digest bytes in FIPS 180-4 output order: H0 to H7, each word big-endian, so byte 0 is the high
///   byte of H0.
/// Errors: None.
/// Version: since Z2 Stage 2. The layout and byte order are fixed forever, because digests are persisted.
/// @endcontract
struct Zenith_Sha256Value
{
	uint8 m_auBytes[32] = {};
	friend constexpr bool operator==(const Zenith_Sha256Value&, const Zenith_Sha256Value&) = default;
};

/// @contract functions Zenith_Sha256
/// Ownership: Pure functions over caller memory; nothing is allocated or retained.
/// Thread affinity: ANY_THREAD. The functions share no state.
/// Invalidation: None.
/// Units: Bytes. Hexadecimal text is 64 lowercase characters, two per digest byte, in byte order.
/// Errors: A null pointer with a non-zero size is ZENITH_ASSERT; a size above ulZENITH_SHA256_MAX_MESSAGE_BYTES
///   is ZENITH_CHECK; Zenith_ParseSha256Hex returns false, leaving xOut unchanged, for any text that is not
///   exactly 64 characters from 0-9 and a-f.
/// Version: since Z2 Stage 2. SHA-256 per FIPS 180-4, outside D-SEL-HASH; the output never changes.
/// @endcontract
Zenith_Sha256Value Zenith_Sha256Digest(const void* pData, uint64 ulSize);
Zenith_Sha256Value Zenith_Sha256Digest(Zenith_ByteSpan xBytes);          // the pointer form over GetData(), GetCount()
void Zenith_FormatSha256Hex(const Zenith_Sha256Value& xValue, char (&acOut)[64]);      // writes no terminator
bool Zenith_ParseSha256Hex(Zenith_StringView strHex, Zenith_Sha256Value& xOut);

/// @contract type Zenith_Sha256Stream
/// Ownership: Plain object holding its whole state inline (112 bytes); it allocates nothing, and a copy forks the
///   digest in progress.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: A null pointer with a non-zero size is ZENITH_ASSERT; a total above ulZENITH_SHA256_MAX_MESSAGE_BYTES
///   since construction or the last Reset is ZENITH_CHECK.
/// Version: since Z2 Stage 2. Finish equals Zenith_Sha256Digest of the concatenation of every Update since
///   construction or the last Reset, however the bytes were split.
/// @endcontract
class Zenith_Sha256Stream
{
public:
	Zenith_Sha256Stream();                                 // the initial hash value H(0)
	void Update(const void* pData, uint64 ulSize);
	void Update(Zenith_ByteSpan xBytes);
	Zenith_Sha256Value Finish() const;       // pads and digests a copy; the stream can continue
	void Reset();
private:
	uint32 m_auState[8];
	uint64 m_ulTotalBytes;
	uint8 m_auBlock[64];
	uint32 m_uBlockBytes;
};
static_assert(sizeof(Zenith_Sha256Value) == 32 && alignof(Zenith_Sha256Value) == 1);
static_assert(sizeof(Zenith_Sha256Stream) == 112);
```

### SHA-256 algorithm steps

1. The constructor and `Reset` set `m_auState` to H(0) = `6a09e667`, `bb67ae85`, `3c6ef372`, `a54ff53a`, `510e527f`, `9b05688c`, `1f83d9ab`, `5be0cd19` (§5.3.3), and `m_ulTotalBytes` and `m_uBlockBytes` to 0.
2. `Update(pData, ulSize)`: `ZENITH_ASSERT(pData != nullptr || ulSize == 0)`; `ZENITH_CHECK(ulSize <= ulZENITH_SHA256_MAX_MESSAGE_BYTES - m_ulTotalBytes)`; add `ulSize` to `m_ulTotalBytes`; top up `m_auBlock` and compress it when it reaches 64 bytes; compress every further whole 64-byte block directly from the input without copying it; keep the remainder (fewer than 64 bytes) in `m_auBlock`. A zero-length update changes nothing. The span overload calls the pointer form.
3. Compressing one block (§6.2.2): W0 to W15 are the block's bytes read as big-endian 32-bit words by explicit shifts (never a `memcpy` into an integer and never a byte-swap intrinsic, so no compiler test is needed outside `Internal/Zenith_CompilerSupport.h`); W16 to W63 are `σ1(W[t−2]) + W[t−7] + σ0(W[t−15]) + W[t−16]`; the 64 rounds use Ch, Maj, Σ0 and Σ1 of §4.1.2 and the constants K0 to K63 of §4.2.2 from one `static constexpr uint32` table; rotations are `(x >> n) | (x << (32 − n))` on `uint32`; the eight working variables are added back into `m_auState`.
4. `Finish() const` works on a copy of the state: it appends the byte `0x80`, then zero bytes until the block holds 56 bytes (in a second block when 56 or more bytes were pending), then the message length in bits (`m_ulTotalBytes × 8`) as a 64-bit big-endian integer (§5.1.1), compresses the final one or two blocks, and writes H0 to H7 big-endian into `Zenith_Sha256Value::m_auBytes`. The stream itself is unchanged, so more `Update` calls continue the same message.
5. `Zenith_Sha256Digest(pData, ulSize)` constructs a stream on the stack, updates it once and returns `Finish()`; it takes no lock, reads no global state and allocates nothing.
6. `Zenith_FormatSha256Hex` writes two lowercase hexadecimal characters per byte, byte 0 first. `Zenith_ParseSha256Hex` accepts exactly `ulZENITH_SHA256_HEX_CHARS` characters from `0-9` and `a-f` (uppercase is rejected, as persistent-ID text rejects it) and writes `xOut` only when the whole text is valid.

Complexity: O(n) in the message length, 64 rounds per 64-byte block. The implementation is `Zenith2/Engine/ZenithCore/Internal/Hash/Zenith_Sha256.cpp`, beside `Zenith_Hash.cpp` and independent of it: it includes no xxHash header and calls no other ZenithCore hash.

## Zenith_RandomSource

<!-- elaboration -->
A counter-based stream: every draw is `Zenith_Hash64` of a 24-byte little-endian block `{ seed, stream, counter }`, so there is no generator state other than the counter, parallel streams never share state, and restoring a stream is restoring one counter. `ForbiddenCheck` rule F14 routes all runtime randomness here (`Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates").

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Random.h
enum Zenith_RandomError : uint8
{
	RANDOM_ERROR_ENTROPY_UNAVAILABLE = 1,
};

/// @contract type Zenith_RandomSource
/// Ownership: Plain value of three uint64 (seed, stream, counter); copyable, which forks an identical stream.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Unit-interval results in [0, 1); angles radians.
/// Errors: CreateFromEntropy returns RANDOM_ERROR_ENTROPY_UNAVAILABLE when the OS generator fails; Range with
///   b < a, or a span above 2^32, is ZENITH_ASSERT.
/// Version: since Z2 Stage 2. The draw layout and every derivation below are fixed; changing any of them
///   changes every seeded sequence and is a replay-format version bump.
/// @endcontract
class Zenith_RandomSource
{
public:
	constexpr Zenith_RandomSource() = default;    // seed 0, stream 0: the value a failed CreateFromEntropy result holds
	constexpr Zenith_RandomSource(uint64 ulSeed, uint64 ulStream);
	static Zenith_Result<Zenith_RandomSource, Zenith_RandomError> CreateFromEntropy();
	uint64 NextU64();
	uint32 NextU32();
	double NextUnitDouble();
	float NextUnitFloat();
	sint64 Range(sint64 iLow, sint64 iHigh);      // inclusive
	double RangeDouble(double dLow, double dHigh);    // [low, high)
	bool Chance(double dProbability);
	Zenith_Vector3 UnitVector();
	template<typename T> void Shuffle(Zenith_Span<T> xItems);
	uint64 GetCounter() const;
	void SetCounter(uint64 ulCounter);
	uint64 GetSeed() const;
	uint64 GetStream() const;
private:
	uint64 m_ulSeed = 0;
	uint64 m_ulStream = 0;
	uint64 m_ulCounter = 0;
};

// The derivations, shared with Zenith2/Docs/Scripting.md "Determinism rules and randomness" (its table "Random API"):
constexpr uint32 Zenith_RandomToU32(uint64 ulDraw) { return static_cast<uint32>(ulDraw >> 32); }
constexpr double Zenith_RandomToUnitDouble(uint64 ulDraw);   // (d >> 11) × 2^−53
constexpr float Zenith_RandomToUnitFloat(uint64 ulDraw);     // (d >> 40) × 2^−24
template<typename TNextDraw> sint64 Zenith_RandomRange(TNextDraw&& xNextDraw, sint64 iLow, sint64 iHigh);
template<typename TNextDraw> Zenith_Vector3 Zenith_RandomUnitVector(TNextDraw&& xNextDraw);
template<typename T, typename TNextDraw> void Zenith_RandomShuffle(Zenith_Span<T> xItems, TNextDraw&& xNextDraw);
```

| Member | Derivation (draw `d` = `NextU64()`) |
|---|---|
| `NextU64` | `d = Zenith_Hash64(LE(seed) ‖ LE(stream) ‖ LE(counter))`, then `counter += 1` |
| `NextU32` | `d >> 32` |
| `NextUnitDouble` | `(d >> 11) × 2^−53` |
| `NextUnitFloat` | `(d >> 40) × 2^−24` |
| `Range(a, b)` | `s = b − a + 1` (as `uint64`; `s = 2^32` gives `a + (d >> 32)`); otherwise Lemire: `x = d >> 32`, `m = x × s` (64-bit), `l = m mod 2^32`; if `l < s`, `t = (2^32 − s) mod s`, and while `l < t` take the next draw and recompute; result `a + (m >> 32)` |
| `RangeDouble(a, b)` | `a + NextUnitDouble() × (b − a)` |
| `Chance(p)` | `NextUnitDouble() < p` |
| `UnitVector` | `u = NextUnitDouble()`, `v = NextUnitDouble()`, `z = 1 − 2u`, `φ = 2πv`, `r = sqrt(max(0, 1 − z²))`, `(sin φ, cos φ)` from `Zenith_SinCos(double)`, result `(r cos φ, r sin φ, z)` as floats (each product in `double`, narrowed once) |
| `Shuffle` | Fisher–Yates from the last index down to 1, swapping index `i` with `Range(0, i)` |

- `CreateFromEntropy` reads 16 bytes from the OS (`BCryptGenRandom` with `BCRYPT_USE_SYSTEM_PREFERRED_RNG` in `Internal/Windows/`; `getrandom(…, 0)` retried on `EINTR` in `Internal/Android/` and `Internal/Linux/`) as the seed and stream. It is for identities and non-simulation variety (persistent IDs, session tokens of development tools); simulation code never calls it ([Z2-N-ARCH-885]).

## Zenith_PersistentID functions

`Zenith_PersistentID` is declared in `Zenith2/Engine/ZenithCore/Zenith_PersistentID.h` exactly as `Zenith2/Docs/Contracts.md` → "Handle kinds" states. The functions:

```cpp
enum Zenith_PersistentIDError : uint8
{
	PERSISTENT_ID_ERROR_BAD_LENGTH = 1,       // not 36 bytes
	PERSISTENT_ID_ERROR_BAD_SEPARATOR,        // a '-' missing at offset 8, 13, 18 or 23
	PERSISTENT_ID_ERROR_BAD_DIGIT,            // not 0–9 or a–f (uppercase is rejected: the form is canonical)
};

Zenith_PersistentID Zenith_GeneratePersistentID(Zenith_RandomSource& xRandom);
Zenith_Result<Zenith_PersistentID, Zenith_PersistentIDError> Zenith_ParsePersistentID(Zenith_StringView strText);
void Zenith_FormatPersistentID(const Zenith_PersistentID& xID, Zenith_Span<char> xOut37);   // 36 characters and a NUL
void Zenith_WritePersistentIDBinary(const Zenith_PersistentID& xID, Zenith_Span<uint8> xOut16);
Zenith_PersistentID Zenith_ReadPersistentIDBinary(Zenith_Span<const uint8> xIn16);
```

- Generation: `d0 = xRandom.NextU64()`, `d1 = xRandom.NextU64()`; `m_ulHigh = (d0 & ~0xF000) | 0x4000` (version 4 in the thirteenth hexadecimal digit); `m_ulLow = (d1 & 0x3FFFFFFFFFFFFFFF) | 0x8000000000000000` (variant `10`); the result is never all-zero.
- Text: the 16 hexadecimal digits of `m_ulHigh` then those of `m_ulLow`, lowercase, with `-` inserted before digit 9, 13, 17 and 21 (`xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`); parsing accepts exactly that form and returns the null ID for the all-zero text.
- Binary: `m_ulHigh` then `m_ulLow`, each little-endian ([Z2-N-CONTRACT-067]).

## Maths tests

| Test ID | File | Asserts |
|---|---|---|
| `Test_Vector_Layout_MatchesSchema` | `Zenith_Vector_Tests.cpp` | sizes 8, 12, 16 and alignment 4; member offsets 0, 4, 8, 12 |
| `Test_Vector_Cross_RightHanded` | `Zenith_Vector_Tests.cpp` | `Cross(X, Y) = Z`, `Cross(Y, Z) = X`, `Cross(Z, X) = Y` exactly |
| `Test_Vector_TryNormalize_Degenerate` | `Zenith_Vector_Tests.cpp` | the zero vector and a 1e-7 vector return false; `NormalizeOr` returns the fallback |
| `Test_Vector_Normalize_Zero_Asserts` | `Zenith_Vector_DefectTests.cpp` | `z2-death-assert` |
| `Test_Vector_Operations_MatchReference` | `Zenith_Vector_Tests.cpp` | every operation of the table against a double-precision reference within 2 ulp over 100,000 generated inputs |
| `Test_Matrix_TRS_ComposeDecompose` | `Zenith_Matrix_Tests.cpp` | `DecomposeTRS(MakeTRS(t, r, s))` recovers t, r (as a rotation) and s within 1e-5 for 10,000 generated inputs including one negative scale |
| `Test_Matrix_Inverse_TimesSelfIsIdentity` | `Zenith_Matrix_Tests.cpp` | within 1e-4 for generated well-conditioned matrices; `TryInverse` false for a rank-3 matrix |
| `Test_Matrix3_InverseAndScale` | `Zenith_Matrix_Tests.cpp` | `Zenith_MakeScale3((2, 4, 8)) * v` scales each component; `Zenith_Inverse(Zenith_MakeScale3((2, 4, 8)))` equals `Zenith_MakeScale3((0.5, 0.25, 0.125))` exactly; for generated rotation-times-scale matrices `Zenith_Inverse(M) * M` is the identity within 1e-4 and equals `Zenith_TryInverse`'s output bit for bit |
| `Test_Matrix3_InverseSingular_Asserts` | `Zenith_Matrix_DefectTests.cpp` | `z2-death-assert`: `Zenith_Inverse` of a rank-2 matrix |
| `Test_Matrix_Perspective_DepthMapping` | `Zenith_Matrix_Tests.cpp` | the near and far view points map to depth 0 and 1 (standard), 1 and 0 (reversed), and far → 0 as distance → ∞ (reversed infinite) |
| `Test_Matrix_LookAt_LooksDownNegativeZ` | `Zenith_Matrix_Tests.cpp` | the target maps to view-space (0, 0, −distance); up maps to +Y |
| `Test_Matrix_Rows3x4_MatchGpuSceneLayout` | `Zenith_Matrix_Tests.cpp` | `Zenith_StoreRows3x4` equals rows 0 … 2 and the translation sits at indices 3, 7, 11 |
| `Test_Quaternion_EulerConvention` | `Zenith_Quaternion_Tests.cpp` | yaw +π/2 rotates +Z to +X; pitch +π/2 rotates +Y to +Z; roll +π/2 rotates +X to +Y |
| `Test_Quaternion_ProductOrder_MatchesMatrix` | `Zenith_Quaternion_Tests.cpp` | `ToMatrix3(a * b) = ToMatrix3(a) · ToMatrix3(b)` within 1e-5 |
| `Test_Quaternion_Slerp_ShortestPathAndEndpoints` | `Zenith_Quaternion_Tests.cpp` | t = 0 and 1 give the inputs; `Slerp(q, −q, t)` stays at q |
| `Test_Quaternion_FromTo_Antiparallel` | `Zenith_Quaternion_Tests.cpp` | rotating `from` gives `to` for antiparallel pairs along each axis |
| `Test_Quaternion_LookRotation_ModelFront` | `Zenith_Quaternion_Tests.cpp` | the result rotates +Z onto the requested front |
| `Test_Colour_SrgbRoundTrip_AllBytes` | `Zenith_Colour_Tests.cpp` | every 8-bit value round-trips through linear and back |
| `Test_Colour_Srgb8Packed_Layout` | `Zenith_Colour_Tests.cpp` | `Zenith_ColourToSrgb8Packed({1, 0, 0, 0.5})` is `0x800000FF` (R in bits 0–7, A = round(0.5 × 255) = 128 in bits 24–31); channels above 1 and below 0 clamp |
| `Test_Colour_Premultiply_ScalesRgbOnly` | `Zenith_Colour_Tests.cpp` | `Zenith_Premultiply({0.5, 1, 0.25, 0.5})` is `{0.25, 0.5, 0.125, 0.5}` |
| `Test_Matrix3_FromQuaternion_Orthonormal` | `Zenith_Matrix_Tests.cpp` | `Zenith_ToMatrix3` of 10,000 generated unit quaternions is orthonormal with determinant +1 within 1e-5, and `Zenith_FromMatrix3` recovers the rotation |
| `Test_Matrix3_NormalMatrix_NonUniformScale` | `Zenith_Matrix_Tests.cpp` | for a TRS with scale (2, 1, 0.5), the normal matrix keeps transformed normals perpendicular to transformed tangents; a zero scale returns false |
| `Test_Bounds_FrustumCulling_Conservative` | `Zenith_Bounds_Tests.cpp` | no box intersecting the frustum is reported outside, for 100,000 generated boxes and each depth mode |
| `Test_Bounds_RayAABB_ZeroDirectionComponents` | `Zenith_Bounds_Tests.cpp` | axis-parallel rays hit and miss correctly |
| `Test_Maths_CheckedArithmetic_Overflow` | `Zenith_Maths_Tests.cpp` | each checked helper at its overflow boundary |
| `Test_Hash64_KnownVectors_MatchReference` | `Zenith_Hash_Tests.cpp` | `Tests/Fixtures/Hash/vectors.json` (generated by the pinned upstream `xxhsum` in the D-SEL-HASH ruling ticket: lengths 0 … 256 and 4096, seeds 0 and 0x9E3779B97F4A7C15) match on every preset |
| `Test_Hash64_StreamEqualsOneShot` | `Zenith_Hash_Tests.cpp` | random splits of 10,000 inputs give the one-shot value |
| `Test_Hash64_CrossPlatform_Identical` | `Zenith_Hash_Tests.cpp` | the vector file is the same on Windows and Android (run in both presets against one fixture) |
| `Test_Sha256_NistVectors` | `Zenith_Sha256_Tests.cpp` | the FIPS 180-4 examples, one-shot and streamed: the empty message → `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`; `abc` → `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`; the 448-bit `abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq` → `248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1`; 1,000,000 × `a` → `cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0` (one-shot over one buffer, and streamed as 1,000 updates of 1,000 bytes) |
| `Test_Sha256_PaddingBoundaries` | `Zenith_Sha256_Tests.cpp` | the pattern byte `(i × 31 + 7) mod 256` at the lengths where padding changes shape, against digests computed outside the engine with OpenSSL (Node.js `crypto.createHash("sha256")`): 55 → `8aa994584139d128848eeebc4e815639ba5ab6e6e39574195a63ac4f14f7c43b`, 56 → `ad574708f75c044c9b85de64cb568ee7711ff4f36448c6242f053ba8f6cc2b63`, 63 → `280ed3e8ff1df845b2e7dfe6ac6cee817bef20e783cc65abc41b818b4d2fe076`, 64 → `c6ab9724ade5b6a7a1edfffb12f3aa9181351355af8fd08c919952ad211339dd`, 65 → `788367c73c7ddf4c53f65e68cc0d943e6227ab55b0e78ba63ace822b1c6301c0`, 119 → `3d610547d68216dedf7435a4fb6260353911f6b3fd3f18805ddb8be285d726fe`, 120 → `1f80156a804cb7862ad113e8200e9d74499723e7c7854d5f48776d3148e09656` |
| `Test_Sha256_StreamEqualsOneShot` | `Zenith_Sha256_Tests.cpp` | property test (`Zenith_PropertyGen`, 16 seeds × 256 inputs of 0 to 1,100 bytes, each split at generated points including zero-length updates): `Finish` equals `Zenith_Sha256Digest` of the whole input; `Finish` twice returns one value and later updates continue the message; a copied stream forks; `Reset` then the same input reproduces the digest; both span overloads equal their pointer forms |
| `Test_Sha256_HexRoundTrip` | `Zenith_Sha256_Tests.cpp` | the `abc` digest formats to its 64-character lowercase text and parses back to an equal value; 63 and 65 characters, one uppercase `A` and one `g` are rejected and leave the output unchanged |
| `Test_Sha256_NullWithSize_Asserts` | `Zenith_Sha256_DefectTests.cpp` | `z2-death-assert`: `Zenith_Sha256Digest(nullptr, 1)` |
| `Test_RandomSource_SequenceGolden` | `Zenith_Random_Tests.cpp` | the first 64 draws of seed 1, stream 2 match `Tests/Fixtures/Random/golden.json` (generated with the ruled hash) |
| `Test_RandomSource_CounterRestore_Replays` | `Zenith_Random_Tests.cpp` | `SetCounter(k)` reproduces the draws from k |
| `Test_RandomSource_Range_Unbiased` | `Zenith_Random_Tests.cpp` | a chi-square test over 10^6 draws of `Range(0, 6)` at p > 0.001, and exact bounds for `Range(a, a)` and the 2^32 span |
| `Test_RandomSource_DerivationsMatchScriptTable` | `Zenith_Random_Tests.cpp` | the free derivations and the members agree for the same draws |
| `Test_RandomSource_Entropy_Distinct` | `Zenith_Random_Tests.cpp` | two `CreateFromEntropy` sources differ |
| `Test_PersistentID_Generate_VersionAndVariant` | `Zenith_PersistentID_Tests.cpp` | 10,000 IDs: version nibble 4, variant `10`, never null, all distinct |
| `Test_PersistentID_TextRoundTrip` | `Zenith_PersistentID_Tests.cpp` | format then parse is the identity; the high word comes first |
| `Test_PersistentID_Parse_Rejects` | `Zenith_PersistentID_Tests.cpp` | 35 and 37 bytes, a missing dash, uppercase, `g` |
| `Test_PersistentID_BinaryLittleEndian` | `Zenith_PersistentID_Tests.cpp` | byte 0 is the low byte of the high word |
| `Test_PersistentID_Fuzz` (`Zenith_Fuzz_PersistentID`) | `Tests/Fuzz/PersistentID.cpp` | libFuzzer over `Zenith_ParsePersistentID`: accepted inputs re-format to themselves |
| `Test_Transcendental_ApiSurface_Overloads` | `Zenith_Transcendental_ApiSurface_Tests.cpp` | both overloads of every function exist and return their argument's precision (`static_assert` on `decltype`); `Zenith_Maths.h` alone makes them visible |
| `Test_Transcendental_GoldenVectors_Trigonometric` | `Zenith_Transcendental_Tests.cpp` | every Sin, Cos, Tan, Atan, Atan2, Asin, Acos and Sqrt row of `Tests/Fixtures/Transcendental/Zenith_TranscendentalGolden.inl` (binary64 and binary32) returns its result bits exactly, on every preset |
| `Test_Transcendental_GoldenVectors_ExponentialLogarithm` | `Zenith_Transcendental_Tests.cpp` | every Exp, Exp2, Ln, Log2, Log10, Pow and Cbrt row of the same file returns its result bits exactly, on every preset |
| `Test_Transcendental_SpecialValues_Trigonometric` | `Zenith_Transcendental_Tests.cpp` | the Sin … Acos and Sqrt rows of "Special values" at both precisions, NaN results compared as the canonical bit patterns; `|x| = 2^20` reduces and `2^20 + 1` returns NaN |
| `Test_Transcendental_SpecialValues_ExponentialLogarithm` | `Zenith_Transcendental_Tests.cpp` | the Exp … Cbrt rows of "Special values", every Pow case of steps 1 to 8, and the exact values of "Error bounds" (Log2 of 2^k for k = −1022 … 1023, Exp2(k) and Pow(2, k) for integer k in [−1022, 1023]) |
| `Test_Transcendental_SinCosMatchesSinAndCos` | `Zenith_Transcendental_Tests.cpp` | for 100,000 generated arguments in [−2^20, 2^20] at both precisions, `Zenith_SinCos` writes exactly `Zenith_Sin` and `Zenith_Cos` |
| `Test_Transcendental_SymmetryBitExact` | `Zenith_Transcendental_Tests.cpp` | bit-exact odd and even symmetry over generated arguments: Sin, Tan, Atan, Asin and Cbrt of −x are the negations; Cos(−x) = Cos(x); Atan2(−y, x) = −Atan2(y, x) |
| `Test_Transcendental_SubnormalArgumentsReadAsZero` | `Zenith_Transcendental_Tests.cpp` | each function of a subnormal argument (smallest, largest, negative, both precisions) returns its value at the signed zero |
| `Test_Transcendental_SubnormalResultsFlushToZero` | `Zenith_Transcendental_Tests.cpp` | `Zenith_Exp(−708.5)`, `Zenith_Exp2(−1022.5)`, `Zenith_Pow(2, −1023)`, `Zenith_Atan2(1e−300, 1e300)` and the binary32 `Zenith_Exp(−88.0f)` are signed zeros, while `Zenith_Exp2(−1022)` is exactly 2^−1022 |
| `Test_Transcendental_WithinBoundOfHostLibrary_Trigonometric` | `Zenith_Transcendental_PropertyTests.cpp` | informative only, never the oracle: over 16 seeds × 4,096 generated arguments per function in its domain, Sin … Acos stay within their bound of "Error bounds" plus 1 ulp of the host `<cmath>` result in `double` (the test's own CRT call is allowed in test code), and binary32 within 1 ulp; prints the worst case with its seed |
| `Test_Transcendental_WithinBoundOfHostLibrary_ExponentialLogarithm` | `Zenith_Transcendental_PropertyTests.cpp` | the same for Exp … Cbrt, Pow over `x` in [1e−5, 1e5] and `y` in [−60, 60] |
| `ZenithCore.TranscendentalNoCrtCall` (CTest, `cmake -P`) | `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` | fails when `Internal/Maths/Zenith_Transcendental.cpp` contains `<cmath>`, `<math.h>` or a `std::` or bare call of a CRT transcendental name, or lacks `ZENITH_STRICT_FP_BEGIN` |
| `Test_CoreMaths_Fma3DisabledAtStartup` | `Zenith_CrtMathMode_Tests.cpp` | in a test executable, `Zenith_IsCrtMathModeApplied()` is true: on Windows the test main called `Zenith_ApplyCrtMathMode` before doctest ran |
| `Test_Transcendental_IndependentOfCrtFma3Mode` | `Zenith_CrtMathMode_Tests.cpp` | with `Zenith_SetCrtFma3EnabledForTest(true)`, then `(false)`, every golden row returns the same bits; the flag is restored to the startup state |
