#ifndef ARCHIVE_HPP
#define ARCHIVE_HPP

#include <map>

#include "IArchive.hpp"
#include "compiler.h"
#include "Tlv.hpp"

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
    static constexpr 
};

#endif // ARCHIVE_HPP