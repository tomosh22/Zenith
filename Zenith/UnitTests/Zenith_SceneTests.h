#pragma once

// Zenith_SceneTests was the holder class for the large async-era scene
// regression suite, deleted with the async machinery. The empty shell remains
// only because Zenith_UnitTests.h still includes this header and
// Zenith_SceneData.h still names it in a `friend` declaration. There are no
// scene unit tests today — scene load/unload is exercised by the
// DevilsPlayground automated tests. Safe to delete once those two references go.
class Zenith_SceneTests
{
};
