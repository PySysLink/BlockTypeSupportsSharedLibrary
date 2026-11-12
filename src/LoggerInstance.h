#ifndef SRC_BLOCK_TYPE_SUPPORTS_SHARED_LIBRARY_SUPPORT_LOGGER_INSTANCE
#define SRC_BLOCK_TYPE_SUPPORTS_SHARED_LIBRARY_SUPPORT_LOGGER_INSTANCE

#include <spdlog/spdlog.h>

namespace BlockTypeSupports::SharedLibrarySupport
{
    class LoggerInstance
    {
        private:
        static std::shared_ptr<spdlog::logger> s_logger;

        public:
        static std::shared_ptr<spdlog::logger> GetLogger()
        {
            return s_logger;
        }

        static void SetLogger(std::shared_ptr<spdlog::logger> logger)
        {
            s_logger = logger;
        }

    };
}

#endif /* SRC_BLOCK_TYPE_SUPPORTS_SHARED_LIBRARY_SUPPORT_LOGGER_INSTANCE */
