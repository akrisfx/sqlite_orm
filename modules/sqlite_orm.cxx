// In a module-file, the optional `module;` must appear first; see [cpp.pre].
module;

#define _BUILD_SQLITE_ORM_MODULE
#define _IMPORT_STD_MODULE

#ifdef _IMPORT_STD_MODULE
// Including assert.h when using `import std;` doesn't work with msvc (as it leads to multiple defined symbols) (see sqlite_orm.ixx), hence we include it here
#include <assert.h>  //  assert macro
#endif

export module sqlite_orm;

#ifdef _IMPORT_STD_MODULE
import std.compat;
#endif

#include <sqlite_orm/sqlite_orm.h>