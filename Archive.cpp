#include <iostream>
#include <filesystem>
#include "Archive.hpp"

bool Archive::create(const std::string& outPath, const std::string& inPath)
{
    std::filesystem::path inputPath(inPath);
    if (!std::filesystem::exists(inputPath))
    {
        std::cerr << "Error: Input path does not exist: " << inPath << std::endl;
        return false;
    }

    for ( const auto& entry : std::filesystem::recursive_directory_iterator(inputPath) )
    {
        
    }

    return true; // Return true if successful, false otherwise
}

bool Archive::extract(const std::string& archivePath, const std::string& outPath)
{
    // Implementation of archive extraction goes here

    return true; // Return true if successful, false otherwise
}

bool Archive::list(const std::string& archivePath)
{
    // Implementation of listing archive contents goes here

    return true; // Return true if successful, false otherwise
}