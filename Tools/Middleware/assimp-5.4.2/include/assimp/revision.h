#ifndef ASSIMP_REVISION_H_INC
#define ASSIMP_REVISION_H_INC

//#TO got rid of all these, cmake is the spawn of satan
// 
//#define GitVersion 0x@GIT_COMMIT_HASH@
#define GitVersion 0x0

//#define GitBranch "@GIT_BRANCH@"
#define GitBranch "invalid"

//#define VER_MAJOR @ASSIMP_VERSION_MAJOR@
#define VER_MAJOR 0

//#define VER_MINOR @ASSIMP_VERSION_MINOR@
#define VER_MINOR 0

//#define VER_PATCH @ASSIMP_VERSION_PATCH@
#define VER_PATCH 0

//#define VER_BUILD @ASSIMP_PACKAGE_VERSION@
#define VER_BUILD 0


#define STR_HELP(x) #x
#define STR(x) STR_HELP(x)

#define VER_FILEVERSION             VER_MAJOR,VER_MINOR,VER_PATCH,VER_BUILD
#if (GitVersion == 0)
#define VER_FILEVERSION_STR         STR(VER_MAJOR) "." STR(VER_MINOR) "." STR(VER_PATCH) "." STR(VER_BUILD)
#else
#define VER_FILEVERSION_STR         STR(VER_MAJOR) "." STR(VER_MINOR) "." STR(VER_PATCH) "." STR(VER_BUILD) " (Commit @GIT_COMMIT_HASH@)"
#endif
#define VER_COPYRIGHT_STR           "\xA9 2006-2023"

#ifdef  NDEBUG
#define VER_ORIGINAL_FILENAME_STR   "@CMAKE_SHARED_LIBRARY_PREFIX@assimp@LIBRARY_SUFFIX@.dll"
#else
#define VER_ORIGINAL_FILENAME_STR   "@CMAKE_SHARED_LIBRARY_PREFIX@assimp@LIBRARY_SUFFIX@@CMAKE_DEBUG_POSTFIX@.dll"
#endif //  NDEBUG

#endif // ASSIMP_REVISION_H_INC
