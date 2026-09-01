#pragma once

#include <string>

namespace IronGang
{
    // plan_36 IG-36-005: write @p text to @p path so that a crash, a full disk, or a failed
    // rename can never leave a half-written file where a complete one belongs.
    //
    // The new content goes to "<path>.tmp" first and is renamed into place only once it is
    // complete -- a rename within one directory is atomic. When @p keepBackup is true the previous
    // file is rotated to "<path>.bak" first, so there is always one generation to fall back on.
    //
    // Extracted from SaveGame, which had the only copy: settings need the same guarantee, and the
    // second caller is where a pattern either becomes shared or becomes two subtly different
    // implementations.
    [[nodiscard]] bool WriteTextFileAtomically(const std::string& path,
                                               const std::string& text,
                                               bool keepBackup,
                                               std::string& errorMessage);

    // "<path>.bak" and "<path>.tmp" -- the rolling backup and the write-in-progress file.
    [[nodiscard]] std::string BackupFilePath(const std::string& path);
    [[nodiscard]] std::string TemporaryFilePath(const std::string& path);
}
