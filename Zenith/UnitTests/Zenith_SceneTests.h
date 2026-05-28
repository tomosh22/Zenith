#pragma once

// Zenith_SceneTests was the holder class for the ~13k-line async-era scene
// regression suite. The suite was deleted as part of the Phase B "delete async
// machinery" refactor; the class shell is kept empty so the include site in
// Zenith_SceneSystem.Tests.inl resolves without churn.
class Zenith_SceneTests
{
};
