// Stand-ins for the handful of symbols the harnessed LeviLamina sources reference but whose
// only implementations are Windows-only (*_win.cpp) or live in the closed BDS binary.
// None of them is on a parsing path: they format/print diagnostics or query the host locale.
#include "ll/api/io/Logger.h"
#include "ll/api/utils/ErrorUtils.h"
#include "ll/api/utils/SystemUtils.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandOutputParameter.h"

#include <exception>
#include <stdexcept>
#include <string>

namespace ll::inline utils::sys_utils {
std::string getSystemLocaleCode() { return "en_US"; }
} // namespace ll::inline utils::sys_utils

namespace ll::io {
void Logger::printStr(LogLevel, std::string&&) const noexcept {}
bool Logger::shouldLog(LogLevel) const noexcept { return false; }
} // namespace ll::io

namespace ll::inline utils::error_utils {
// impl holds a copy of the exception_ptr, like ErrorUtils_win.cpp.
AnyExceptionRef::AnyExceptionRef(std::exception_ptr const& ptr) : impl(new std::exception_ptr(ptr)) {}
AnyExceptionRef::~AnyExceptionRef() { delete static_cast<std::exception_ptr*>(impl); }
void* AnyExceptionRef::typeCast(std::type_info const&) const { return nullptr; }
std::string makeExceptionString(std::exception_ptr ePtr) noexcept {
    try {
        if (ePtr) std::rethrow_exception(ePtr);
    } catch (std::exception const& e) {
        return std::string("std::exception: ") + e.what();
    } catch (...) {
        return "unknown exception";
    }
    return "no exception";
}
} // namespace ll::inline utils::error_utils

// BDS command-output types (Error::log(CommandOutput&) in Expected.cpp; unreachable here).
void CommandOutput::addMessage(
    std::string_view, std::vector<::CommandOutputParameter> const&, ::CommandOutputMessageType
) {}
CommandOutputParameter::~CommandOutputParameter() {}
