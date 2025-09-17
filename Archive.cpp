#include <iostream>
#include <filesystem>
#include "Archive.hpp"
#include <cerrno>
#include <cstring>

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
            addDirectory( archiveFile, entry.path().string() );
        }
        else if(entry.is_regular_file()) 
        {
            std::cout << "File: " << entry.path() << std::endl;
            addFile( archiveFile, entry.path().string() );
        }
    }

    archiveFile.close();

    return true; // Return true if successful, false otherwise
}

bool Archive::extract(const std::string& archivePath, const std::string& outPath)
{
    // Implementation of archive extraction goes here

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

void Archive::addFile( std::ofstream& archiveFile, const std::string& path )
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
        addDuplicateFile( archiveFile, path, *fingerprint );
    }
    else
    {
        std::vector<TlvEntry> children;
        TlvEntry nameEntry;
        TlvEntry dataEntry;

        uint32_t fileOffset = static_cast<uint32_t>( archiveFile.tellp() );

        nameEntry.tag = NAME_TAG;
        nameEntry.length = static_cast<Tag_t>(path.size());

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
        archiveFile.write(path.c_str(), path.size());
        archiveFile.write(reinterpret_cast<const char*>(&dataEntry), sizeof(dataEntry));

        char buffer[4096];
        while( inputFile.read(buffer, sizeof(buffer)) || inputFile.gcount() > 0 )
        {
            archiveFile.write(buffer, inputFile.gcount());
        }

        uint64_t fingerprint = calculateFingerprint( path );
        File file;
        file.name = path;
        file.size = dataEntry.length;
        file.offset = fileOffset;
        m_files[fingerprint] = file;
    }
    
    inputFile.close();
}

void Archive::addDuplicateFile( std::ofstream& archiveFile, const std::string& path, uint64_t fingerprint )
{
    auto fileEntry = m_files[fingerprint];

    std::vector<TlvEntry> children;
    TlvEntry nameEntry;
    TlvEntry dataRefEntry;

    nameEntry.tag = NAME_TAG;
    nameEntry.length = static_cast<Tag_t>(path.size());

    dataRefEntry.tag = DATA_REF_TAG;
    dataRefEntry.length = sizeof(fileEntry.offset);

    children.push_back( nameEntry );
    children.push_back( dataRefEntry );

    TlvEntry entry;
    entry.tag = FILE_TAG;
    entry.length = getTlvSize( children );
    archiveFile.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
    
    archiveFile.write(reinterpret_cast<const char*>(&nameEntry), sizeof(nameEntry));
    archiveFile.write(path.c_str(), path.size());
    archiveFile.write(reinterpret_cast<const char*>(&dataRefEntry), sizeof(dataRefEntry));
    archiveFile.write(reinterpret_cast<const char*>(&fileEntry.offset), sizeof(fileEntry.offset));
}

void Archive::addDirectory( std::ofstream& archiveFile, const std::string& path )
{
    TlvEntry entry;
    entry.tag = DIRECTORY_TAG;
    entry.length = static_cast<Tag_t>(path.size());
    archiveFile.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
    archiveFile.write(path.c_str(), path.size());
}

uint64_t Archive::calculateFingerprint( const std::string& path )
{
    // Implementation of fingerprint calculation goes here
    return 0;
}

bool Archive::compareFiles( const std::string& path1, const std::string& path2 )
{
    // Implementation of file comparison goes here
    return false;
}

std::optional<uint64_t> Archive::findDuplicate( const std::string& path )
{
    uint64_t fingerprint = calculateFingerprint( path );
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
            archiveFile.seekg(entry.length, std::ios::cur); // Skip the data
            std::cout << "      data size: " << entry.length << std::endl;
        }
        else if( entry.tag == DATA_REF_TAG )
        {
            uint32_t dataOffset;
            archiveFile.read(reinterpret_cast<char*>(&dataOffset), sizeof(dataOffset));
            file.offset = dataOffset;
            std::cout << "Data Reference Offset: " << file.offset << std::endl;
        }
        
        bytesRead += entry.length;
    }

    return file;
}