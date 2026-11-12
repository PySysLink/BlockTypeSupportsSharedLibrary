#include <PySysLinkBase/IBlockFactory.h>
#include "BlockFactorySharedLibrary.h"
#include "spdlog/spdlog.h"
#include <iostream>
#include "LoggerInstance.h"

extern "C" void RegisterBlockFactories(std::map<std::string, std::shared_ptr<PySysLinkBase::IBlockFactory>>& registry, std::map<std::string, PySysLinkBase::ConfigurationValue> pluginConfiguration) {
    std::cout << "Call to RegisterBlockFactories" << std::endl;
    registry["SharedLibrary"] = std::make_shared<BlockTypeSupports::SharedLibrarySupport::BlockFactorySharedLibrary>(pluginConfiguration);
    std::cout << "End of function" << std::endl;
}

extern "C" void RegisterSpdlogLogger(std::shared_ptr<spdlog::logger> logger) {
    BlockTypeSupports::SharedLibrarySupport::LoggerInstance::SetLogger(logger);
    BlockTypeSupports::SharedLibrarySupport::LoggerInstance::GetLogger()->debug("Logger from plugin BlockTypeSupportsSharedLibrary!");
}
