#define PANDAJ2534DLL_API extern "C" __declspec(dllexport)

// A quick way to avoid the name mangling that __stdcall liked to do
#define EXPORT comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
