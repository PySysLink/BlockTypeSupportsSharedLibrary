#ifndef SRC_BLOCK_TYPE_SUPPORTS_SHARED_LIBRARY_SUPPORT_BLOCK_FACTORY_SHARED_LIBRARY
#define SRC_BLOCK_TYPE_SUPPORTS_SHARED_LIBRARY_SUPPORT_BLOCK_FACTORY_SHARED_LIBRARY

#include <PySysLinkBase/IBlockFactory.h>
#include <PySysLinkBase/IBlockEventsHandler.h>
#include "SimulationBlockSharedLibrary.h"
#include <stdexcept>
#include <algorithm> 

namespace BlockTypeSupports::SharedLibrarySupport
{
    class BlockFactorySharedLibrary : public PySysLinkBase::IBlockFactory
    {               
        public:
            BlockFactorySharedLibrary(std::map<std::string, PySysLinkBase::ConfigurationValue> pluginConfiguration);
            std::shared_ptr<PySysLinkBase::ISimulationBlock> CreateBlock(std::map<std::string, PySysLinkBase::ConfigurationValue> blockConfiguration, std::shared_ptr<PySysLinkBase::IBlockEventsHandler> blockEventsHandler);
    };
} // namespace BlockTypeSupports::SharedLibrarySupport


#endif /* SRC_BLOCK_TYPE_SUPPORTS_SHARED_LIBRARY_SUPPORT_BLOCK_FACTORY_SHARED_LIBRARY */
