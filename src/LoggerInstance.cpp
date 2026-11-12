#include "LoggerInstance.h"

namespace BlockTypeSupports::SharedLibrarySupport
{
    std::shared_ptr<spdlog::logger> LoggerInstance::s_logger = nullptr;
} // namespace BlockTypeSupports::SharedLibrarySupport
