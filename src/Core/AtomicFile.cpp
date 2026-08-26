#include "IronGang/Core/AtomicFile.hpp"

#include "System/IO/Directory.hpp"
#include "System/IO/File.hpp"

#include <filesystem>

namespace IronGang
{
    std::string BackupFilePath(const std::string& path)
    {
        return path + ".bak";
    }

    std::string TemporaryFilePath(const std::string& path)
    {
        return path + ".tmp";
    }

    bool WriteTextFileAtomically(const std::string& path,
                                 const std::string& text,
                                 bool keepBackup,
                                 std::string& errorMessage)
    {
        try
        {
            const std::filesystem::path filesystemPath(path);
            const std::filesystem::path parent = filesystemPath.parent_path();
            if (!parent.empty() && !System::IO::Directory::Exists(parent.string()))
            {
                System::IO::Directory::CreateDirectory(parent.string());
            }

            const std::filesystem::path temporaryPath(TemporaryFilePath(path));
            System::IO::File::WriteAllText(temporaryPath.string(), text);

            std::error_code renameError;
            if (keepBackup && std::filesystem::exists(filesystemPath))
            {
                std::filesystem::rename(filesystemPath, std::filesystem::path(BackupFilePath(path)),
                                        renameError);
                if (renameError)
                {
                    std::filesystem::remove(temporaryPath, renameError);
                    errorMessage = "Could not rotate the previous file to a backup: " + renameError.message();
                    return false;
                }
            }
            std::filesystem::rename(temporaryPath, filesystemPath, renameError);
            if (renameError)
            {
                errorMessage = "Could not move the new file into place: " + renameError.message();
                return false;
            }
            return true;
        }
        catch (const std::exception& exception)
        {
            std::error_code ignored;
            std::filesystem::remove(std::filesystem::path(TemporaryFilePath(path)), ignored);
            errorMessage = exception.what();
            return false;
        }
    }
}
