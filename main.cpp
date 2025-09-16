#include <iostream>
#include "Archive.hpp"

void PrintHelp()
{
    std::cout << "Usage: custom-tar <command> [options]\n";
    std::cout << "Commands:\n";
    std::cout << "  create <archive_path> <input_path>   Create an archive from the input path\n";
    std::cout << "  extract <archive_path> <output_path> Extract an archive to the output path\n";
    std::cout << "  list <archive_path>                  List contents of the archive\n";
    std::cout << "  help                                 Show this help message\n";
}

int main( int argc, char* argv[] ) 
{
    if (argc < 2) 
    {
        PrintHelp();
        return 1;
    }    
    else if (std::string(argv[1]) == "help") 
    {
        PrintHelp();
        return 0;
    } 
    else if (std::string(argv[1]) == "create") 
    {
        if (argc != 4) 
        {
            std::cerr << "Error: Invalid number of arguments for create command.\n";
            PrintHelp();
            return 1;
        }
        std::string archivePath = argv[2];
        std::string inputPath = argv[3];

        Archive archive;
        if (!archive.create(archivePath, inputPath)) 
        {
            std::cerr << "Error: Failed to create archive.\n";
            return 1;
        }
        std::cout << "Archive created successfully: " << archivePath << "\n";
    } 
    else if (std::string(argv[1]) == "extract") 
    {
        if (argc != 4) 
        {
            std::cerr << "Error: Invalid number of arguments for extract command.\n";
            PrintHelp();
            return 1;
        }
        std::string archivePath = argv[2];
        std::string outputPath = argv[3];

        Archive archive;
        if (!archive.extract(archivePath, outputPath)) 
        {
            std::cerr << "Error: Failed to extract archive.\n";
            return 1;
        }
        std::cout << "Archive extracted successfully to: " << outputPath << "\n";
    } 
    else if (std::string(argv[1]) == "list") 
    {
        if (argc != 3) 
        {
            std::cerr << "Error: Invalid number of arguments for list command.\n";
            PrintHelp();
            return 1;
        }
        std::string archivePath = argv[2];

        Archive archive;
        if (!archive.list(archivePath)) 
        {
            std::cerr << "Error: Failed to list archive contents.\n";
            return 1;
        }
    } 
    else 
    {
        std::cerr << "Error: Unknown command '" << argv[1] << "'.\n";
        PrintHelp();
        return 1;
    }
    return 0;
}