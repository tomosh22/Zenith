// Standalone constinit validation for Zenith_Engine design.
// Compile: cl /std:c++20 /c constinit_prototype.cpp
// Expected: zero errors, zero warnings.

#include <type_traits>

// Forward-declared subsystem types (mimics Zenith_Engine.h's intent
// to NOT include subsystem headers).
class Zenith_TaskSystem;
class Zenith_Profiling;
class Zenith_Multithreading;
class Zenith_AssetRegistry;
class Zenith_Physics;
class Zenith_SceneManager;
class Zenith_Input;
class Zenith_Editor;
class Flux_Renderer;
struct FrameContext;

class Zenith_Engine
{
public:
    Zenith_Engine() = default;   // trivial; implicitly constexpr in C++20
    ~Zenith_Engine() = default;  // trivial

    void Initialise();
    void Shutdown();

    // Accessor bodies live in Zenith_Engine.cpp (where the full
    // subsystem headers are visible). Here just the signatures.
    Zenith_TaskSystem&    Tasks();
    Zenith_Profiling&     Profiling();
    Zenith_Multithreading& Threading();
    Zenith_AssetRegistry& Assets();
    Zenith_Physics&       Physics();
    Zenith_SceneManager&  Scenes();
    Zenith_Input&         Input();
#ifdef ZENITH_TOOLS
    Zenith_Editor&        Editor();
#endif
    Flux_Renderer&        Renderer();
    FrameContext&         Frame();

private:
    // All members initialised to nullptr at declaration. nullptr is a
    // constant expression of std::nullptr_t convertible to any
    // pointer-to-incomplete-type. The default ctor is implicitly
    // constexpr because every member has a constant default-initializer.
    Zenith_TaskSystem*    m_pxTaskSystem    = nullptr;
    Zenith_Profiling*     m_pxProfiling     = nullptr;
    Zenith_Multithreading* m_pxThreading    = nullptr;
    Zenith_AssetRegistry* m_pxAssets        = nullptr;
    Zenith_Physics*       m_pxPhysics       = nullptr;
    Zenith_SceneManager*  m_pxScenes        = nullptr;
    Zenith_Input*         m_pxInput         = nullptr;
#ifdef ZENITH_TOOLS
    Zenith_Editor*        m_pxEditor        = nullptr;
#endif
    Flux_Renderer*        m_pxRenderer      = nullptr;
    FrameContext*         m_pxFrame         = nullptr;
};

// The point of the prototype: does this compile?
constinit Zenith_Engine g_xEngine;

// Compile-time guards.
//
// NOT useful here: std::is_trivially_default_constructible_v.
// In-class member initialisers (= nullptr) make the default ctor
// non-trivial in the type-traits sense even though every initialiser
// is a constant expression. constinit doesn't require triviality —
// it requires constant initialisation. The constinit declaration
// above is itself the compile-time guarantee.
//
// Useful: trivial destructor. We must NOT run a static destructor at
// process exit (those can fire in undefined order vs. other globals,
// reintroducing the static-destruction-order fiasco we're avoiding).
// Shutdown() is the explicit teardown path.
static_assert(std::is_trivially_destructible_v<Zenith_Engine>,
              "Zenith_Engine must be trivially destructible so process "
              "shutdown doesn't run static destructors in undefined order. "
              "Subsystem cleanup belongs in Zenith_Engine::Shutdown().");
