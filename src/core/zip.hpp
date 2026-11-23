// 极简 ZIP（仅 store 存储方式），用于 .mjnote 打包
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "util.hpp"

namespace moji {

Bytes zip_write(const std::vector<std::pair<std::string, Bytes>>& entries);
std::map<std::string, Bytes> zip_read(const Bytes& data);

}  // namespace moji
