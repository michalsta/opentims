#include "sqlite_helper.h"

#if !defined(OPENTIMS_BUILDING_R) && !defined(OPENTIMS_LINK_SQLITE_STATICALLY)
std::optional<LoadedLibraryHandle> ot_sqlite::sqlite_so_handle;
#endif
