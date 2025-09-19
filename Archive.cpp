#include <iostream>
#include <filesystem>
#include "Archive.hpp"
#include <cerrno>
#include <cstring>
#include "zlib.h"
#include "xxhash.h"
#include "FastLZ/fastlz.h"
#include <cassert>

// size comes from the zlib example
#define CHUNK 16384 

bool Archive::create(const std::string& outPath, const std::string& inPath)
{
    std::string decompressedFileName = outPath + ".tmp";
    std::filesystem::path inputPath(inPath);
    if (!std::filesystem::exists(inputPath))
    {
        std::cerr << "Error: Input path does not exist: " << inPath << std::endl;
        return false;
    }

    std::ofstream archiveFile(decompressedFileName, std::ios::binary);
    if (!archiveFile.is_open())
    {
        std::cerr << "Error: Could not open archive file for writing: " << outPath << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        return false;
    }

    for ( const auto& entry : std::filesystem::recursive_directory_iterator(inputPath) )
    {
        if(entry.is_directory())
        {
            std::cout << "Directory: " << entry.path() << std::endl;
            addDirectory( archiveFile, entry.path().string(), inPath );
        }
        else if(entry.is_regular_file()) 
        {
            std::cout << "File: " << entry.path() << std::endl;
            addFile( archiveFile, entry.path().string(), inPath );
        }
    }

    archiveFile.close();

    if ( !compress( decompressedFileName, outPath ) )
    {
        std::cerr << "Error: Could not compress archive file: " << decompressedFileName << std::endl;
        return false;
    }

    return true;
}

bool Archive::extract(const std::string& archivePath, const std::string& outPath)
{
    std::string decompressedFileName = archivePath + ".tmp";
    if ( !decompress( archivePath, decompressedFileName ) )
    {
        std::cerr << "Error: Could not decompress archive file: " << archivePath << std::endl;
        return false;
    }
    std::ifstream archiveFile(decompressedFileName, std::ios::binary);
    if (!archiveFile.is_open())
    {
        std::cerr << "Error: Could not open archive file for reading: " << archivePath << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        return false;
    }

    std::filesystem::create_directories( outPath );

    std::map<uint32_t, File> files;

    TlvEntry entry;
    while (archiveFile.read(reinterpret_cast<char*>(&entry), sizeof(entry)))
    {
        std::cout << "Tag: " << to_str(entry.tag) << ", Length: " << std::dec << entry.length << std::endl;
        if ( entry.tag == DIRECTORY_TAG )
        {
            std::string dirName = outPath + "/" + readDirectory( archiveFile, entry.length );
            std::filesystem::create_directories( dirName );
            std::cout << "Directory: " << dirName << std::endl;
            continue;
        }
        else if( entry.tag == FILE_TAG )
        {
            auto file = readFile( archiveFile, entry.length );
            std::cout << "File: " << file.name << std::endl;
            files[file.offset] = file;
            continue;
        }
        archiveFile.seekg(entry.length, std::ios::cur); // Skip the data
    }

    extractFiles( files, outPath, archiveFile );

    archiveFile.close();

    return true; 
}

bool Archive::list(const std::string& archivePath)
{
    std::string decompressedFileName = archivePath + ".tmp";
    if ( !decompress( archivePath, decompressedFileName ) )
    {
        std::cerr << "Error: Could not decompress archive file: " << archivePath << std::endl;
        return false;
    }
    std::ifstream archiveFile(decompressedFileName, std::ios::binary);
    if (!archiveFile.is_open())
    {
        std::cerr << "Error: Could not open archive file for reading: " << archivePath << std::endl;
        std::cerr << "Error: " << strerror(errno) << std::endl;
        return false;
    }

    TlvEntry entry;
    while (archiveFile.read(reinterpret_cast<char*>(&entry), sizeof(entry)))
    {
        std::cout << "Tag: " << to_str(entry.tag) << ", Length: " << std::dec << entry.length << std::endl;
        if( entry.tag == DIRECTORY_TAG )
        {
            std::string dirName = readDirectory( archiveFile, entry.length );
            std::cout << "Directory: " << dirName << std::endl;
            continue;
        }
        else if( entry.tag == FILE_TAG )
        {
            auto file = readFile( archiveFile, entry.length );
            std::cout << "File: " << file.name << std::endl;
            continue;
        }
        archiveFile.seekg(entry.length, std::ios::cur); // Skip the data
    }

    archiveFile.close();

    return true; 
}

void Archive::addFile( std::ofstream& archiveFile, const std::string& path, const std::string& basePath )
{
    std::ifstream inputFile(path, std::ios::binary);
    if (!inputFile.is_open())
    {
        std::cerr << "Error: Could not open input file: " << path << std::endl;
        return;
    }

    auto fingerprint = findDuplicate( path );
    if ( fingerprint )
    {
        addDuplicateFile( archiveFile, path, *fingerprint, basePath );
    }
    else
    {
        std::vector<TlvEntry> children;
        TlvEntry nameEntry;
        TlvEntry dataEntry;

        uint32_t fileOffset = static_cast<uint32_t>( archiveFile.tellp() );

        std::string relativePath = toRelativePath( basePath, path );

        nameEntry.tag = NAME_TAG;
        nameEntry.length = static_cast<Tag_t>(relativePath.size());

        uint32_t fileSize = static_cast<uint32_t>(std::filesystem::file_size( path ));
        dataEntry.tag = DATA_TAG;
        dataEntry.length = fileSize;

        children.push_back( nameEntry );
        children.push_back( dataEntry );

        TlvEntry fileEntry;
        fileEntry.tag = FILE_TAG;
        fileEntry.length = getTlvSize( children );

        archiveFile.write(reinterpret_cast<const char*>(&fileEntry), sizeof(fileEntry));
        archiveFile.write(reinterpret_cast<const char*>(&nameEntry), sizeof(nameEntry));
        archiveFile.write(relativePath.c_str(), relativePath.size());
        archiveFile.write(reinterpret_cast<const char*>(&dataEntry), sizeof(dataEntry));

        uint32_t dataOffset = static_cast<uint32_t>( archiveFile.tellp() ); // Pozycja danych

        char buffer[4096];
        while( inputFile.read(buffer, sizeof(buffer)) || inputFile.gcount() > 0 )
        {
            archiveFile.write(buffer, inputFile.gcount());
        }

        Fingerprint fingerprint = calculateFingerprint( path );
        File file;
        file.name = path;
        file.size = dataEntry.length;
        file.offset = fileOffset;
        file.dataOffset = dataOffset;
        file.dataOffsetRef = 0;
        m_files[fingerprint] = file;
    }
    
    inputFile.close();
}

void Archive::addDuplicateFile( std::ofstream& archiveFile, const std::string& path, const Fingerprint& fingerprint, const std::string& basePath )
{
    auto fileEntry = m_files[fingerprint];

    std::vector<TlvEntry> children;
    TlvEntry nameEntry;
    TlvEntry dataRefEntry;

    std::string relativePath = toRelativePath( basePath, path );

    nameEntry.tag = NAME_TAG;
    nameEntry.length = static_cast<Tag_t>(relativePath.size());

    dataRefEntry.tag = DATA_REF_TAG;
    dataRefEntry.length = sizeof(fileEntry.offset);

    children.push_back( nameEntry );
    children.push_back( dataRefEntry );

    TlvEntry entry;
    entry.tag = FILE_TAG;
    entry.length = getTlvSize( children );
    archiveFile.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
    
    archiveFile.write(reinterpret_cast<const char*>(&nameEntry), sizeof(nameEntry));
    archiveFile.write(relativePath.c_str(), relativePath.size());
    archiveFile.write(reinterpret_cast<const char*>(&dataRefEntry), sizeof(dataRefEntry));
    archiveFile.write(reinterpret_cast<const char*>(&fileEntry.offset), sizeof(fileEntry.offset));

    std::cout << "Duplicate file detected: " << path << " (refers to offset " << fileEntry.offset << ")" << std::endl;
}

void Archive::addDirectory( std::ofstream& archiveFile, const std::string& path, const std::string& basePath )
{
    std::string relativePath = toRelativePath( basePath, path );

    TlvEntry entry;
    entry.tag = DIRECTORY_TAG;
    entry.length = static_cast<Tag_t>(relativePath.size());
    archiveFile.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
    archiveFile.write(relativePath.c_str(), relativePath.size());
}

Fingerprint Archive::calculateFingerprint( const std::string& path )
{
    XXH64_state_t* state = XXH64_createState();
    XXH64_reset(state, 0);

    std::ifstream f(path, std::ios::binary);
    std::vector<char> buf(64*1024);
    while (f) {
        f.read(buf.data(), buf.size());
        std::streamsize r = f.gcount();
        if (r > 0) XXH64_update(state, buf.data(), (size_t)r);
    }
    uint64_t digest = XXH64_digest(state);
    XXH64_freeState(state);

    std::ostringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << digest;
    return ss.str(); 
}

bool Archive::compareFiles( const std::string& path1, const std::string& path2 )
{
    std::ifstream file1(path1, std::ios::binary);
    std::ifstream file2(path2, std::ios::binary);
    if (!file1.is_open() || !file2.is_open())
    {
        std::cerr << "Error: Could not open files for comparison: " << path1 << ", " << path2 << std::endl;
        return false;
    }
    char buffer1[4096];
    char buffer2[4096];
    do
    {
        file1.read(buffer1, sizeof(buffer1));
        file2.read(buffer2, sizeof(buffer2));
        std::streamsize bytesRead1 = file1.gcount();
        std::streamsize bytesRead2 = file2.gcount();
        if (bytesRead1 != bytesRead2 || std::memcmp(buffer1, buffer2, bytesRead1) != 0)
        {
            file1.close();
            file2.close();
        }
    } while (file1 && file2);
    file1.close();
    file2.close();
    if (file1.eof() && file2.eof())
    {
        return true;
    }
    return false;
}

std::optional<Fingerprint> Archive::findDuplicate( const std::string& path )
{
    auto fingerprint = calculateFingerprint( path );
    if ( m_files.find( fingerprint ) != m_files.end() && compareFiles( path, m_files[fingerprint].name ) )
    {
        return fingerprint;
    }

    return std::nullopt;
}

uint32_t Archive::getTlvSize( const std::vector<TlvEntry>& entries )
{
    uint32_t totalSize = 0;
    for ( const auto& entry : entries )
    {
        totalSize += sizeof(entry) + entry.length;
    }
    return totalSize;
}

std::string Archive::readDirectory( std::ifstream& archiveFile, uint32_t length )
{
    char* buffer = new char[length];
    archiveFile.read(buffer, length);
    std::string dirName(buffer, length);
    delete[] buffer;
    return dirName;
}

Archive::File Archive::readFile( std::ifstream& archiveFile, uint32_t length )
{
    File file;
    uint32_t bytesRead = 0;
    TlvEntry entry;

    file.offset = static_cast<uint32_t>(archiveFile.tellg()) - static_cast<uint32_t>(sizeof(TlvEntry));
    
    while (bytesRead < length && archiveFile.read(reinterpret_cast<char*>(&entry), sizeof(entry)))
    {
        bytesRead += sizeof(entry);
        std::cout << "  Tag: " << to_str(entry.tag) << ", Length: " << std::dec << entry.length << std::endl;
        
        if( entry.tag == NAME_TAG )
        {
            char* buffer = new char[entry.length];
            archiveFile.read(buffer, entry.length);
            file.name = std::string(buffer, entry.length);
            delete[] buffer;
            std::cout << "      fname: " << file.name << std::endl;
        }
        else if( entry.tag == DATA_TAG )
        {
            file.dataOffset = static_cast<uint32_t>(archiveFile.tellg());
            std::cout << "      data size: " << entry.length << std::endl;
            file.size = entry.length;
            archiveFile.seekg(entry.length, std::ios::cur); // Skip the data
        }
        else if( entry.tag == DATA_REF_TAG )
        {
            uint32_t fileOffset;
            archiveFile.read(reinterpret_cast<char*>(&fileOffset), sizeof(fileOffset));
            file.dataOffsetRef = fileOffset;
            std::cout << "File Reference Offset: " << fileOffset << std::endl;
        }
        
        bytesRead += entry.length;
    }

    return file;
}

bool Archive::extractFiles( const std::map<uint32_t, File>& files, const std::string& outPath, std::ifstream& archiveFile )
{
    archiveFile.clear(); 
    for ( const auto& [offset, cfile] : files )
    {
        auto file = cfile; 
        std::string fullPath = outPath + "/" + file.name;
        std::filesystem::create_directories( std::filesystem::path(fullPath).parent_path() );

        if ( file.dataOffset == 0 )
        {
            auto it = files.find( file.dataOffsetRef );
            if ( it != files.end() )
            {
                std::cout << "Extracting duplicate file: " << fullPath << " from a file: " << it->second.name << " referencing offset: " << file.dataOffsetRef << std::endl;
                file.dataOffset = it->second.dataOffset;
                file.size = it->second.size;
            }
            else
            {
                std::cerr << "Error: Could not find original data for duplicate file: " << fullPath << std::endl;
                continue;
            }
        }
        std::ofstream outputFile(fullPath, std::ios::binary);
        if (!outputFile.is_open())
        {
            std::cerr << "Error: Could not open output file for writing: " << fullPath << std::endl;
            continue;
        }

        char buffer[4096];
        archiveFile.seekg(file.dataOffset, std::ios::beg);
        uint32_t remaining = file.size;
        while (remaining > 0)
        {
            uint32_t bytesToRead = std::min(static_cast<uint32_t>(sizeof(buffer)), remaining);
            archiveFile.read(buffer, bytesToRead);
            if(archiveFile.gcount() == 0)
            {
                break; 
            }

            std::streamsize bytesRead = archiveFile.gcount();
            outputFile.write(buffer, bytesRead);
            remaining -= static_cast<uint32_t>(bytesRead);
        }
        if (remaining != 0)
        {
            std::cerr << "Error: Unexpected end of file while extracting: " << fullPath << " remaining bytes: " << remaining << std::endl;
        }
        else 
        {
            std::cout << "Extracted file: " << fullPath << " with size: " << file.size << std::endl;
        }

        outputFile.close();
    }

    return true;
}

std::string Archive::toRelativePath( const std::string& basePath, const std::string& fullPath )
{
    std::filesystem::path base(basePath);
    std::filesystem::path full(fullPath);
    std::filesystem::path relative = std::filesystem::relative(full, base);
    return relative.string();
}

int def(FILE *source, FILE *dest, int level)
{
    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, level);
    if (ret != Z_OK)
        return ret;

    /* compress until end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)deflateEnd(&strm);
            return Z_ERRNO;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)deflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);     /* all input will be used */

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    (void)deflateEnd(&strm);
    return Z_OK;
}

/* Decompress from file source to file dest until stream ends or EOF.
    inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
    allocated for processing, Z_DATA_ERROR if the deflate data is
    invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
    the version of the library linked do not match, or Z_ERRNO if there
    is an error reading or writing the files. */
int inf(FILE *source, FILE *dest)
{
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)inflateEnd(&strm);
            return Z_ERRNO;
        }
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                return ret;
            }
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)inflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

bool Archive::compress( const std::string& inPath, const std::string& outPath )
{
    FILE *source = fopen(inPath.c_str(), "rb");
    if (source == nullptr)
    {
        std::cerr << "Error: Could not open input file for compression: " << inPath << std::endl;
        return false;
    }

    FILE *dest = fopen(outPath.c_str(), "wb");
    if (dest == nullptr)
    {
        std::cerr << "Error: Could not open output file for compression: " << outPath << std::endl;
        fclose(source);
        return false;
    }

    int ret = def(source, dest, Z_BEST_COMPRESSION);
    if (ret != Z_OK)
    {
        std::cerr << "Error: Compression failed with code: " << ret << std::endl;
    }

    fclose(source);
    fclose(dest);

    return ret == Z_OK;
}

bool Archive::decompress( const std::string& inPath, const std::string& outPath )
{
    FILE *source = fopen(inPath.c_str(), "rb");
    if (source == nullptr)
    {
        std::cerr << "Error: Could not open input file for decompression: " << inPath << std::endl;
        return false;
    }

    FILE *dest = fopen(outPath.c_str(), "wb");
    if (dest == nullptr)
    {
        std::cerr << "Error: Could not open output file for decompression: " << outPath << std::endl;
        fclose(source);
        return false;
    }

    int ret = inf(source, dest);
    if (ret != Z_OK)
    {
        std::cerr << "Error: Decompression failed with code: " << ret << std::endl;
    }

    fclose(source);
    fclose(dest);

    return ret == Z_OK;
}