#include <string>
#include "uFilterPicker/version.hpp"

using namespace UFilterPicker;

int Version::getMajor() noexcept
{
    return uFilterPicker_MAJOR;
}

int Version::getMinor() noexcept
{
    return uFilterPicker_MINOR;
}

int Version::getPatch() noexcept
{
    return uFilterPicker_PATCH;
}

//NOLINTBEGIN(bugprone-easily-swappable-parameters)
bool Version::isAtLeast(const int major, const int minor,
                        const int patch) noexcept
//NOLINTEND(bugprone-easily-swappable-parameters)
{
    if (uFilterPicker_MAJOR < major){return false;}
    if (uFilterPicker_MAJOR > major){return true;}
    if (uFilterPicker_MINOR < minor){return false;}
    if (uFilterPicker_MINOR > minor){return true;}
    if (uFilterPicker_PATCH < patch){return false;}
    return true;
}

std::string Version::getVersion() noexcept
{
    std::string version{uFilterPicker_VERSION};
    return version;
}

std::string Version::getTag() noexcept
{
    const std::string tag{uFilterPicker_GITTAG};
    return tag;
}
