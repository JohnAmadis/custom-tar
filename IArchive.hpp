#ifndef IARCHIVE_HPP
#define IARCHIVE_HPP

#include <string>

class IArchive
{
public:
    virtual ~IArchive() = default;

    /**
     * @brief Creates an archive from the specified input path.
     * 
     * @param archivePath The path where the archive will be created.
     * @param inPath The input path (file or directory) to be archived.
     * @return true if the archive was created successfully, false otherwise.
     */
    virtual bool create(const std::string& archivePath, const std::string& inPath) = 0;

    /**
     * @brief Extracts the contents of the specified archive to the output path.
     * 
     * @param archivePath The path of the archive to be extracted.
     * @param outPath The output path where the contents will be extracted.
     * @return true if the extraction was successful, false otherwise.
     */
    virtual bool extract(const std::string& archivePath, const std::string& outPath) = 0;

    /**
     * @brief Lists the contents of the specified archive.
     * 
     * @param archivePath The path of the archive to be listed.
     * @return true if the listing was successful, false otherwise.
     */
    virtual bool list(const std::string& archivePath) = 0;
};

#endif // IARCHIVE_HPP