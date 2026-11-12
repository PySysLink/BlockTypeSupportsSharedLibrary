#ifndef SRC_SIMULATION_BLOCK_SHARED_LIBRARY
#define SRC_SIMULATION_BLOCK_SHARED_LIBRARY

#include <dlfcn.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <PySysLinkBase/ISimulationBlock.h>
#include <PySysLinkBase/PortsAndSignalValues/InputPort.h>
#include <PySysLinkBase/PortsAndSignalValues/OutputPort.h>
#include <PySysLinkBase/PortsAndSignalValues/PortTypeMetadata.h>
#include <PySysLinkBase/PortsAndSignalValues/SignalValue.h>

#include "SharedLibraryBlockApi.h"

namespace BlockTypeSupports::SharedLibrarySupport
{
    class SimulationBlockSharedLibrary : public PySysLinkBase::ISimulationBlock
    {
        using InputReader = void (*)(
            const std::shared_ptr<PySysLinkBase::InputPort>&,
            PySysLinkBase::FullySupportedSignalValue&);
        using OutputWriter = void (*)(
            const std::shared_ptr<PySysLinkBase::OutputPort>&,
            const PySysLinkBase::FullySupportedSignalValue&);

        std::vector<std::shared_ptr<PySysLinkBase::InputPort>> inputPorts;
        std::vector<std::shared_ptr<PySysLinkBase::OutputPort>> outputPorts;
        std::vector<InputReader> inputReaders;
        std::vector<OutputWriter> outputWriters;
        std::vector<PySysLinkBase::FullySupportedSignalValue> inputValues;
        std::vector<PySysLinkBase::FullySupportedSignalValue> outputValues;
        std::shared_ptr<PySysLinkBase::SampleTime> sampleTime;
        void* libraryHandle = nullptr;
        SharedLibraryComputeFunction computeFunction = nullptr;

        static PySysLinkBase::FullySupportedSignalValue CreateValue(
            const PySysLinkBase::PortTypeMetadata& type)
        {
            if (type.category != PySysLinkBase::PortCategory::FullySupportedSignalValue ||
                !type.signalValueType)
            {
                throw std::invalid_argument(
                    "SharedLibrary only supports fully supported signal-value ports");
            }

            return PySysLinkBase::DispatchType(*type.signalValueType, []<typename T>()
            {
                return PySysLinkBase::FullySupportedSignalValue{T{}};
            });
        }

        static InputReader CreateInputReader(const PySysLinkBase::PortTypeMetadata& type)
        {
            CreateValue(type);
            return PySysLinkBase::DispatchType(*type.signalValueType, []<typename T>()
            {
                return +[](
                    const std::shared_ptr<PySysLinkBase::InputPort>& port,
                    PySysLinkBase::FullySupportedSignalValue& value)
                {
                    auto signalValue = port->GetValue()->TryCastToTyped<T>();
                    value = signalValue->GetPayload();
                };
            });
        }

        static OutputWriter CreateOutputWriter(const PySysLinkBase::PortTypeMetadata& type)
        {
            CreateValue(type);
            return PySysLinkBase::DispatchType(*type.signalValueType, []<typename T>()
            {
                return +[](
                    const std::shared_ptr<PySysLinkBase::OutputPort>& port,
                    const PySysLinkBase::FullySupportedSignalValue& value)
                {
                    port->SetValue(
                        std::make_shared<PySysLinkBase::SignalValue<T>>(std::get<T>(value)));
                };
            });
        }

    public:
        SimulationBlockSharedLibrary(
            std::map<std::string, PySysLinkBase::ConfigurationValue> blockConfiguration,
            std::shared_ptr<PySysLinkBase::IBlockEventsHandler> blockEventsHandler,
            const std::vector<PySysLinkBase::PortTypeMetadata>& inputTypes,
            const std::vector<PySysLinkBase::PortTypeMetadata>& outputTypes,
            const std::vector<bool>& inputsHasDirectFeedthrough)
            : ISimulationBlock(blockConfiguration, blockEventsHandler)
        {
            if (inputsHasDirectFeedthrough.size() != inputTypes.size())
            {
                throw std::invalid_argument(
                    "SharedLibrary port metadata and has direct feedthrough information must match");
            }

            inputPorts.reserve(inputTypes.size());
            inputReaders.reserve(inputTypes.size());
            inputValues.reserve(inputTypes.size());
            for (std::size_t i = 0; i < inputTypes.size(); ++i)
            {
                inputPorts.push_back(
                    PySysLinkBase::CreateInputPortOfType(inputTypes[i], inputsHasDirectFeedthrough[i]));
                inputReaders.push_back(CreateInputReader(inputTypes[i]));
                inputValues.push_back(CreateValue(inputTypes[i]));
            }

            outputPorts.reserve(outputTypes.size());
            outputWriters.reserve(outputTypes.size());
            outputValues.reserve(outputTypes.size());
            for (const auto& outputType : outputTypes)
            {
                outputPorts.push_back(PySysLinkBase::CreateOutputPortOfType(outputType));
                outputWriters.push_back(CreateOutputWriter(outputType));
                outputValues.push_back(CreateValue(outputType));
            }

            const auto libraryPath =
                PySysLinkBase::ConfigurationValueManager::TryGetConfigurationValue<std::string>(
                    "SharedLibraryPath", blockConfiguration);
            const auto functionName =
                PySysLinkBase::ConfigurationValueManager::TryGetConfigurationValue<std::string>(
                    "ComputeFunctionName", blockConfiguration);

            libraryHandle = dlopen(libraryPath.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!libraryHandle)
            {
                const char* error = dlerror();
                throw std::runtime_error(
                    "Failed to load shared block library '" + libraryPath + "': " +
                    (error ? error : "unknown dynamic loader error"));
            }

            dlerror();
            computeFunction = reinterpret_cast<SharedLibraryComputeFunction>(
                dlsym(libraryHandle, functionName.c_str()));
            const char* symbolError = dlerror();
            if (symbolError || !computeFunction)
            {
                const std::string error = symbolError ? symbolError : "symbol resolved to null";
                dlclose(libraryHandle);
                libraryHandle = nullptr;
                throw std::runtime_error(
                    "Failed to find compute function '" + functionName + "' in '" +
                    libraryPath + "': " + error);
            }

            const double _sampleTime =
            PySysLinkBase::ConfigurationValueManager::TryGetConfigurationValue<
                double>("SampleTime", blockConfiguration);
            if (!std::isfinite(_sampleTime) ||
                (_sampleTime <= 0.0 && _sampleTime != -1.0)) {
                throw std::invalid_argument(
                    "SampleTime must be greater than 0 or equal to -1 for inherited "
                    "sample time");
            }
            if (_sampleTime == -1.0) {
                sampleTime = std::make_shared<PySysLinkBase::SampleTime>(
                    PySysLinkBase::SampleTimeType::inherited,
                    std::vector<PySysLinkBase::SampleTimeType>{
                        PySysLinkBase::SampleTimeType::discrete});
            } else {
                sampleTime = std::make_shared<PySysLinkBase::SampleTime>(
                    PySysLinkBase::SampleTimeType::discrete, _sampleTime);
            }
        }

        ~SimulationBlockSharedLibrary() override
        {
            if (libraryHandle)
            {
                dlclose(libraryHandle);
            }
        }

        SimulationBlockSharedLibrary(const SimulationBlockSharedLibrary&) = delete;
        SimulationBlockSharedLibrary& operator=(const SimulationBlockSharedLibrary&) = delete;

        const std::shared_ptr<PySysLinkBase::SampleTime> GetSampleTime() const override
        {
            return sampleTime;
        }

        void SetSampleTime(std::shared_ptr<PySysLinkBase::SampleTime> value) override
        {
            sampleTime = std::move(value);
        }

        std::vector<std::shared_ptr<PySysLinkBase::InputPort>> GetInputPorts() const
        {
            return inputPorts;
        }

        const std::vector<std::shared_ptr<PySysLinkBase::OutputPort>> GetOutputPorts() const
        {
            return outputPorts;
        }

        const std::vector<std::shared_ptr<PySysLinkBase::OutputPort>> _ComputeOutputsOfBlock(
            const std::shared_ptr<PySysLinkBase::SampleTime> currentSampleTime,
            double currentTime,
            bool isMinorStep = false) override
        {
            for (std::size_t i = 0; i < inputPorts.size(); ++i)
            {
                inputReaders[i](inputPorts[i], inputValues[i]);
            }

            computeFunction(
                currentSampleTime,
                currentTime,
                isMinorStep,
                inputValues,
                outputValues);

            if (outputValues.size() != outputPorts.size())
            {
                throw std::runtime_error(
                    "SharedLibrary compute function changed the output vector size");
            }

            for (std::size_t i = 0; i < outputPorts.size(); ++i)
            {
                outputWriters[i](outputPorts[i], outputValues[i]);
            }

            return GetOutputPorts();
        }

        bool _TryUpdateConfigurationValue(std::string, PySysLinkBase::ConfigurationValue) override
        {
            return false;
        }
    };
}

#endif /* SRC_SIMULATION_BLOCK_SHARED_LIBRARY */
