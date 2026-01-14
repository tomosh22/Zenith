// Only re-enable macros if we're not inside a placement-new safe zone
// Headers that use placement new should define ZENITH_PLACEMENT_NEW_ZONE before includes
// and undef it at the end of the header
#if defined(ZENITH_MEMORY_MANAGEMENT_ENABLED) && !defined(ZENITH_PLACEMENT_NEW_ZONE)
#define new new(__LINE__, __FILE__)
#define malloc(x) static_assert(false, "malloc forbidden when memory profiled");
#define realloc(x, y) static_assert(false, "realloc forbidden when memory profiled");
#define free(x) static_assert(false, "free forbidden when memory profiled");
#endif
