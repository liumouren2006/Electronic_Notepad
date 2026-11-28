// 导入导出：json / mjnote(zip) / todo.txt / markdown
#pragma once

#include <filesystem>
#include <string>

#include "repository.hpp"

namespace moji {

void export_json(Repository& repo, const fs::path& path);
ImportReport import_json(Repository& repo, const fs::path& path, bool merge = true);

void export_todotxt(Repository& repo, const fs::path& path);
ImportReport import_todotxt(Repository& repo, const fs::path& path, bool merge = true);

void export_markdown(Repository& repo, const fs::path& path);

fs::path export_bundle(Repository& repo, const fs::path& path);
ImportReport import_bundle(Repository& repo, const fs::path& path, bool merge = true);
SyncReport sync_merge_bundle(Repository& repo, const fs::path& path);

fs::path export_auto(Repository& repo, const fs::path& path);
ImportReport import_auto(Repository& repo, const fs::path& path, bool merge = true);

}  // namespace moji
