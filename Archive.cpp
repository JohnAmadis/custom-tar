#include <iostream>
#include <filesystem>
#include "Archive.hpp"
#include <cerrno>
#include <cstring>
#include "xxhash.h"

bool Archive::create(const std::string& outPath, const std::string& inPath)
{
    std::filesystem::path inputPath(inPath);
    if (!std::filesystem::exists(inputPath))
    {
        std::cerr << "Error: Input path does not exist: " << inPath << std::endl;
        return false;
    }

    std::ofstream archiveFile(outPath, std::ios::binary);
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

    return true; // Return true if successful, false otherwise
}

bool Archive::extract(const std::string& archivePath, const std::string& outPath)
{
    std::ifstream archiveFile(archivePath, std::ios::binary);
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

    return true; // Return true if successful, false otherwise
}

bool Archive::list(const std::string& archivePath)
{
    std::ifstream archiveFile(archivePath, std::ios::binary);
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

    return true; // Return true if successful, false otherwise
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
            uint32_t dataOffset;
            archiveFile.read(reinterpret_cast<char*>(&dataOffset), sizeof(dataOffset));
            file.dataOffsetRef = dataOffset;
            std::cout << "Data Reference Offset: " << dataOffset << std::endl;
        }
        
        bytesRead += entry.length;
    }

    return file;
}

bool Archive::extractFiles( const std::map<uint32_t, File>& files, const std::string& outPath, std::ifstream& archiveFile )
{
    archiveFile.clear(); // Clear any EOF flags
    for ( const auto& [offset, cfile] : files )
    {
        auto file = cfile; // Make a copy to modify if needed
        std::string fullPath = outPath + "/" + file.name;
        std::filesystem::create_directories( std::filesystem::path(fullPath).parent_path() );

        if ( file.dataOffsetRef != 0 )
        {
            // This is a duplicate file, find the original data
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

        // This is an original file, read its data
        char buffer[4096];
        archiveFile.seekg(file.dataOffset, std::ios::beg);
        uint32_t remaining = file.size;
        while (remaining > 0)
        {
            uint32_t bytesToRead = std::min(static_cast<uint32_t>(sizeof(buffer)), remaining);
            archiveFile.read(buffer, bytesToRead);
            if(archiveFile.gcount() == 0)
            {
                break; // EOF or error
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