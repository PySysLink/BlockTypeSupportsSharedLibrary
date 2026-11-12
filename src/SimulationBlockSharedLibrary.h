#ifndef SRC_SIMULATION_BLOCK_SHARED_LIBRARY
#define SRC_SIMULATION_BLOCK_SHARED_LIBRARY

#include <string>
#include <vector>
#include <memory>
#include <PySysLinkBase/PortsAndSignalValues/InputPort.h>
#include <PySysLinkBase/PortsAndSignalValues/OutputPort.h>
#include <PySysLinkBase/PortsAndSignalValues/SignalValue.h>
#include <PySysLinkBase/SampleTime.h>
#include <PySysLinkBase/ISimulationBlock.h>
#include <PySysLinkBase/IBlockEventsHandler.h>
#include <spdlog/spdlog.h>
#include <PySysLinkBase/PortsAndSignalValues/PortTypeMetadata.h>

#include "LoggerInstance.h"


namespace BlockTypeSupports::SharedLibrarySupport
{
    class SimulationBlockSharedLibrary : public PySysLinkBase::ISimulationBlock {
        protected:
            std::vector<std::shared_ptr<PySysLinkBase::InputPort>> inputPorts;
            std::vector<std::shared_ptr<PySysLinkBase::OutputPort>> outputPorts;

            std::shared_ptr<PySysLinkBase::SampleTime> sampleTime;

        public:
            SimulationBlockSharedLibrary(std::map<std::string, PySysLinkBase::ConfigurationValue> blockConfiguration, 
                                std::shared_ptr<PySysLinkBase::IBlockEventsHandler> blockEventsHandler,
                                std::vector<PySysLinkBase::PortTypeMetadata> inputTypes, std::vector<PySysLinkBase::PortTypeMetadata> outputTypes, std::vector<bool> inputsHasDirectFeedthrough) 
                                : ISimulationBlock(blockConfiguration, blockEventsHandler) 
            {
                LoggerInstance::GetLogger()->debug("Creating basic simulation block cpp...");
                LoggerInstance::GetLogger()->debug("Input ports: {}, Output ports: {}", inputPortNumber, outputPortNumber);

                int inputPortNumber = inputTypes.size();
                int outputPortNumber = outputTypes.size();

           
                for (int i = 0; i < inputPortNumber; i++)
                {
                    auto inputPort = PySysLinkBase::CreateInputPortOfType(inputTypes[i], inputsHasDirectFeedthrough[i]);
                    this->inputPorts.push_back(inputPort);
                }
                for (int i = 0; i < outputPortNumber; i++)
                {
                    auto outputPort = PySysLinkBase::CreateOutputPortOfType(outputTypes[i]);
                    this->outputPorts.push_back(outputPort);
                }

                LoggerInstance::GetLogger()->debug("Ports configured...");               
                LoggerInstance::GetLogger()->debug("Basic simulation block cpp created");
            }

            const virtual std::shared_ptr<PySysLinkBase::SampleTime> GetSampleTime() const { return this->sampleTime; }
            virtual void SetSampleTime(std::shared_ptr<PySysLinkBase::SampleTime> sampleTime) { this->sampleTime = sampleTime; }


            std::vector<std::shared_ptr<PySysLinkBase::InputPort>> GetInputPorts() const
            {
                return this->inputPorts;
            }
                    
            const std::vector<std::shared_ptr<PySysLinkBase::OutputPort>> GetOutputPorts() const
            {
                return this->outputPorts;
            }

            const std::vector<std::shared_ptr<PySysLinkBase::OutputPort>> _ComputeOutputsOfBlock(const std::shared_ptr<PySysLinkBase::SampleTime> sampleTime, double currentTime, bool isMinorStep=false)
            {
                std::vector<I> inputValues = this->GetInputValues();

                std::vector<O> outputValues = this->ComputeOutputsOfSharedLibraryBlock(inputValues, sampleTime, currentTime, isMinorStep);
                this->SetOutputValues(outputValues);
                
                return this->GetOutputPorts();
            }

            virtual std::vector<O> ComputeOutputsOfSharedLibraryBlock(const std::vector<I> inputs, const std::shared_ptr<PySysLinkBase::SampleTime> sampleTime, double currentTime, bool isMinorStep=false) = 0;


            virtual bool _TryUpdateConfigurationValue(std::string keyName, PySysLinkBase::ConfigurationValue value) {return false;}

            virtual const std::vector<std::pair<double, double>> GetEvents(const std::vector<I> inputs, const std::shared_ptr<PySysLinkBase::SampleTime> sampleTime, double eventTime, std::vector<double> eventTimeStates, bool includeKnownEvents=false) const
            {
                return {};
            }

            virtual const std::vector<double> GetKnownEvents(const std::shared_ptr<PySysLinkBase::SampleTime> resolvedSampleTime, double simulationStartTime, double simulationEndTime) const
            {
                return {};
            }
    };
}

#endif /* SRC_SIMULATION_BLOCK_SHARED_LIBRARY */
