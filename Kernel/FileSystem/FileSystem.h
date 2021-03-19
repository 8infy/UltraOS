#pragma once

#include "Drivers/Storage.h"
#include "File.h"
#include "Core/ErrorCode.h"

namespace kernel {

class FileSystem {
    MAKE_NONCOPYABLE(FileSystem);

public:
    FileSystem(StorageDevice& associated_device, LBARange lba_range)
        : m_associated_device(associated_device)
        , m_lba_range(lba_range)
    {
    }

    virtual Pair<ErrorCode, File*> open(StringView path) = 0;
    virtual ErrorCode close(File&) = 0;
    virtual ErrorCode remove(StringView path) = 0;
    virtual ErrorCode create(StringView file_path, File::Attributes) = 0;

    virtual ErrorCode move(StringView path, StringView new_path) = 0;
    virtual ErrorCode copy(StringView path, StringView new_path) = 0;

    StorageDevice& associated_device() { return m_associated_device; }
    LBARange lba_range() const { return m_lba_range; }

    virtual ~FileSystem() = default;

private:
    StorageDevice& m_associated_device;
    LBARange m_lba_range;
};

}