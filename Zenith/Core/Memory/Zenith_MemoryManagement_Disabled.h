#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
#undef new
#undef malloc(x)
#undef realloc(x, y)
#undef free(x)
#endif