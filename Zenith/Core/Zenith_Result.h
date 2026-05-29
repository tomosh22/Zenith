#pragma once

#include "Core/Zenith_ErrorCode.h"
#include <type_traits>

// Zenith_Result<T> - a status code paired with a plain value payload.
//
// This is the generic form of the codebase's existing "struct holding a status
// enum + payload, returned by value" pattern (cf. Zenith_PathResult). It is a
// trivial value type by design: no heap, no user-declared destructor, no owning
// members. When T is trivially-destructible, so is Zenith_Result<T> — which is
// why it may only ever live as a local / return value and NEVER as a member of
// a constinit global (e.g. g_xEngine). The static_asserts below pin that
// guarantee for the instantiations the engine relies on.
//
// Header-only with light deps (ErrorCode + <type_traits>) so it can be included
// widely (e.g. from Zenith_Asset.h) without dragging in Zenith.h, Zenith_Vector,
// or <string>, and without forming an include cycle.
template<typename T>
class Zenith_Result
{
public:
	// Value ctor: implicit success. Lets a loader `return pxAsset;` directly.
	Zenith_Result(const T& xValue)
		: m_eError(Zenith_ErrorCode::SUCCESS)
		, m_xValue(xValue)
	{
	}

	// Error ctor: implicit from an error code. Lets a function `return Error();`
	// or `return Zenith_ErrorCode::FILE_NOT_FOUND;` build a failed Result.
	// (No assert that eError != SUCCESS here, intentionally — pulling in
	// Zenith_Assert would defeat the keep-deps-light goal of this header.)
	Zenith_Result(Zenith_ErrorCode eError)
		: m_eError(eError)
		, m_xValue{}
	{
	}

	bool IsOk() const { return m_eError == Zenith_ErrorCode::SUCCESS; }

	const T& Value() const { return m_xValue; }
	T& Value() { return m_xValue; }

	Zenith_ErrorCode Error() const { return m_eError; }

private:
	Zenith_ErrorCode m_eError;
	T m_xValue;
};

// Status-only return (no value payload). The bool carries the legacy
// true/false success signal; the error code carries the specific reason.
using Zenith_Status = Zenith_Result<bool>;

// ZENITH_TRY - early-return helper for the error boundary. Evaluates expr (a
// Zenith_Result/Zenith_Status) once; on failure, returns its error code. Relies
// on the implicit Zenith_Result(Zenith_ErrorCode) ctor, so it only compiles
// inside a function whose return type is a Zenith_Result/Zenith_Status.
// BOUNDARY-ONLY: do not paste into a function returning T* or bool.
#define ZENITH_TRY(expr) \
	do { \
		auto _xZenithTryRes = (expr); \
		if (!_xZenithTryRes.IsOk()) \
		{ \
			return _xZenithTryRes.Error(); \
		} \
	} while (0)

// Constinit discipline: these fail the build if Zenith_Result ever grows a
// non-trivial member, which would make it illegal to live on a constinit global.
static_assert(std::is_trivially_destructible_v<Zenith_Result<void*>>);
static_assert(std::is_trivially_destructible_v<Zenith_Status>);
