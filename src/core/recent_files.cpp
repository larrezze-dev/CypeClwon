#include "core/recent_files.h"

#include <algorithm>
#include <cctype>

namespace cc::core {

std::string RecentFiles::normalise(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

namespace {
bool sameFile(const std::string& a, const std::string& b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
}
}  // namespace

RecentFiles::RecentFiles(std::vector<std::string> paths) {
    // Oldest first, so that add() leaves the original order and drops duplicates.
    for (auto it = paths.rbegin(); it != paths.rend(); ++it) add(*it);
}

void RecentFiles::add(const std::string& path) {
    const std::string clean = normalise(path);
    if (clean.empty()) return;
    remove(clean);
    m_paths.insert(m_paths.begin(), clean);
    if (m_paths.size() > kCapacity) m_paths.resize(kCapacity);
}

void RecentFiles::remove(const std::string& path) {
    const std::string clean = normalise(path);
    m_paths.erase(std::remove_if(m_paths.begin(), m_paths.end(), [&](const std::string& p) { return sameFile(p, clean); }),
                  m_paths.end());
}

std::size_t RecentFiles::pruneMissing(const std::function<bool(const std::string&)>& exists) {
    const std::size_t before = m_paths.size();
    m_paths.erase(std::remove_if(m_paths.begin(), m_paths.end(), [&](const std::string& p) { return !exists(p); }),
                  m_paths.end());
    return before - m_paths.size();
}

std::vector<std::string> RecentFiles::top(std::size_t count) const {
    return {m_paths.begin(), m_paths.begin() + static_cast<std::ptrdiff_t>(std::min(count, m_paths.size()))};
}

}  // namespace cc::core
