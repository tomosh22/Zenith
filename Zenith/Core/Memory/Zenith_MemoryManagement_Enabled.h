#define new new(__LINE__, __FILE__)
#define malloc(x) static_assert(false, "malloc forbidden when memory profiled");
#define realloc(x, y) static_assert(false, "realloc forbidden when memory profiled");
#define free(x) static_assert(false, "free forbidden when memory profiled"); 