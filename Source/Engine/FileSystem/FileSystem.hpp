#pragma once

#include "Engine/FileSystem/FilePath.hpp"

namespace FileSystem
{
    struct FileFilter
    {
        std::string_view description;
        std::string_view pattern;
    };

    struct DialogDescription
    {
        std::string_view title;
        FilePath folder;
        std::vector<FileFilter> filters;
    };

    std::vector<char> ReadFile(const FilePath& path);
    void CreateDirectories(const FilePath& path);

    FilePath ShowOpenFileDialog(const DialogDescription& description);
}

namespace FileFilters
{
    constexpr FileSystem::FileFilter gltf = { "GLTF scenes (*.gltf, *.glb)", "*.gltf *.glb" };
}
