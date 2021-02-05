// Copyright (c) 2021 Nicholas Corgan
// SPDX-License-Identifier: MIT

#pragma once

#include <Poco/SharedLibrary.h>

#include <memory>
#include <string>

class ScopedDynLib
{
public:
    using SPtr = std::shared_ptr<ScopedDynLib>;

    static inline SPtr load(const std::string& library)
    {
        return SPtr(new ScopedDynLib(library));
    }

    inline ScopedDynLib(const std::string& library): _sharedLibrary(library)
    {}

    virtual ~ScopedDynLib()
    {
        _sharedLibrary.unload();
    }

private:
    Poco::SharedLibrary _sharedLibrary;
};
