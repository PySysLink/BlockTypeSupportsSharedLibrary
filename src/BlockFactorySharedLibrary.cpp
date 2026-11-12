#include "BlockFactorySharedLibrary.h"
#include <stdexcept>
#include "SimulationBlockSharedLibrary.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include "LoggerInstance.h"
#include <PySysLinkBase/PortsAndSignalValues/PortTypeMetadata.h>
#include <iostream>
#include "SimulationBlockSharedLibraryWithContinuousStates.h"
#include "SimulationBlockSharedLibrary.h"

namespace BlockTypeSupports::SharedLibrarySupport
{
    BlockFactorySharedLibrary::BlockFactorySharedLibrary(std::map<std::string, PySysLinkBase::ConfigurationValue> pluginConfiguration)
    {

    }


    std::string JoinPortTypes(const std::vector<PySysLinkBase::PortTypeMetadata>& types)
    {
        if (types.empty())
            return "[]";

        std::ostringstream oss;
        oss << "[";

        for (size_t i = 0; i < types.size(); ++i)
        {
            oss << PySysLinkBase::PortCategoryToString(types[i].category);
            oss << "(";
            if (types[i].signalValueType.has_value())
            {
                    oss << types[i].signalValueType.value();
            }
            else
            {
                    oss << "no signal value type";
            }
            oss << ")";
            if (i + 1 < types.size())
                    oss << ", ";
        }

        oss << "]";
        return oss.str();
    }


    std::shared_ptr<PySysLinkBase::ISimulationBlock> BlockFactorySharedLibrary::CreateBlock(std::map<std::string, PySysLinkBase::ConfigurationValue> blockConfiguration, std::shared_ptr<PySysLinkBase::IBlockEventsHandler> blockEventsHandler)
    {
        std::string blockClass = PySysLinkBase::ConfigurationValueManager::TryGetConfigurationValue<std::string>("BlockClass", blockConfiguration);
        
        int inputPortNumber = PySysLinkBase::ConfigurationValueManager::TryGetConfigurationValue<int>("InputPortNumber", blockConfiguration);
        int outputPortNumber = PySysLinkBase::ConfigurationValueManager::TryGetConfigurationValue<int>("OutputPortNumber", blockConfiguration);

        auto inputTypes = PySysLinkBase::ParsePortTypeMetadatas(
        PySysLinkBase::ConfigurationValueManager::TryGetConfigurationValue<std::vector<std::string>>(
            "InputPortTypes", blockConfiguration));

        auto outputTypes = PySysLinkBase::ParsePortTypeMetadatas(
        PySysLinkBase::ConfigurationValueManager::TryGetConfigurationValue<std::vector<std::string>>(
            "OutputPortTypes", blockConfiguration));
        

        LoggerInstance::GetLogger()->debug("{} class block to create with input types {}, output types {}", blockClass, JoinPortTypes(inputTypes), JoinPortTypes(outputTypes));

        if (blockClass == "SharedLibrary")
        {
            return std::make_shared<SimulationBlockSharedLibrary>(blockConfiguration, blockEventsHandler, inputTypes, outputTypes, true);
        }
        else if (blockClass == "SharedLibraryWithContinuousStates")
        {
            return std::make_shared<SimulationBlockSharedLibraryWithContinuousStates>(blockConfiguration, blockEventsHandler, inputTypes, outputTypes, true);
        }
        else
        {
            throw std::runtime_error("Block class not supported by BlockFactorySharedLibrary: " + blockClass);
        }
        
        return this->CreateBlockFromRegistry(this->factoryRegistry, blockClass, blockConfiguration, blockEventsHandler, JoinPortTypes(inputTypes), JoinPortTypes(outputTypes), inputPortNumber, outputPortNumber);
    }  

} // namespace BlockTypeSupports::SharedLibrarySupport
