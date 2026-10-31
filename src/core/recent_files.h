#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace cc::core {

/// Most-recently-used list behind "Últimos ficheros" on the start page. Paths use '/' separators and are
/// compared case-insensitively (Windows file systems). The start page shows six entries; how many the original
/// keeps behind its "Más..." button is not documented, so kCapacity is a provisional value (ticket TK-018).
class RecentFiles {
public:
    static constexpr std::size_t kCapacity = 20;
    static constexpr std::size_t kStartPageSlots = 6;

    RecentFiles() = default;
    explicit RecentFiles(std::vector<std::string> paths);

    /// Moves `path` to the front (adding it if new); the oldest entry is dropped beyond kCapacity.
    void add(const std::string& path);
    void remove(const std::string& path);
    /// Drops every entry for which `exists` is false; returns how many were removed.
    std::size_t pruneMissing(const std::function<bool(const std::string&)>& exists);

    const std::vector<std::string>& paths() const { return m_paths; }
    /// The first `count` entries (the start page uses kStartPageSlots).
    std::vector<std::string> top(std::size_t count = kStartPageSlots) const;

private:
    static std::string normalise(std::string path);
    std::vector<std::string> m_paths;
};

}  // namespace cc::core
