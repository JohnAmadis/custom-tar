#ifndef ARCHIVE_HPP
#define ARCHIVE_HPP

#include <map>
#include <string>
#include <fstream>
#include <optional>
#include <vector>

#include "IArchive.hpp"
#include "compiler.h"

using Tag_t = uint32_t;

constexpr Tag_t operator"" _tag( const char* str, size_t len )
{
    // Ensure we have exactly 4 characters for the tag
    if (len != 4) {
        // This will cause a compile-time error if length is not 4
        throw "Tag must be exactly 4 characters";
    }
    
    return (static_cast<uint32_t>(str[0]) << 24) |
           (static_cast<uint32_t>(str[1]) << 16) |
           (static_cast<uint32_t>(str[2]) << 8)  |
           (static_cast<uint32_t>(str[3]));
}

static inline std::string to_str( Tag_t tag )
{
    char chars[5] = { 
        static_cast<char>((tag >> 24) & 0xFF),
        static_cast<char>((tag >> 16) & 0xFF),
        static_cast<char>((tag >> 8) & 0xFF),
        static_cast<char>(tag & 0xFF),
        '\0'
    };
    return std::string(chars);
}

class Archive : public IArchive
{
public:
    /**
     * @copydoc IArchive::create
     */
    bool create(const std::string& archivePath, const std::string& inPath) override;

    /**
     * @copydoc IArchive::extract
     */
    bool extract(const std::string& archivePath, const std::string& outPath) override;

    /**
     * @copydoc IArchive::list
     */
    bool list(const std::string& archivePath) override;

private:
    static constexpr const Tag_t FILE_TAG      = "FILE"_tag;
    static constexpr const Tag_t DIRECTORY_TAG = "DIR_"_tag; 
    static constexpr const Tag_t NAME_TAG      = "NAME"_tag;
    static constexpr const Tag_t SIZE_TAG      = "SIZE"_tag;
    static constexpr const Tag_t DATA_TAG      = "DATA"_tag;
    static constexpr const Tag_t DATA_REF_TAG  = "DATR"_tag;

    struct TlvEntry
    {
        Tag_t    length;
        uint32_t tag;
    } PACKED;

    struct File
    {
        std::string name;
        uint32_t size;
        uint32_t offset;
    };

    std::map<uint64_t, File> m_files;

    void addFile( std::ofstream& archiveFile, const std::string& path );
    void addDuplicateFile( std::ofstream& archiveFile, const std::string& path, uint64_t fingerprint );
    void addDirectory( std::ofstream& archiveFile, const std::string& path );

    uint64_t calculateFingerprint( const std::string& path );
    bool compareFiles( const std::string& path1, const std::string& path2 );
    std::optional<uint64_t> findDuplicate( const std::string& path );

    static uint32_t getTlvSize( const std::vector<TlvEntry>& entries );
};

#endif // ARCHIVE_HPP