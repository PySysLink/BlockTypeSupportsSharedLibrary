#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <PySysLinkBase/PortsAndSignalValues/SignalValue.h>
#include <PySysLinkBase/SampleTime.h>

#include "SimulationBlockSharedLibrary.h"

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Expected path to typed test plugin\n";
        return 2;
    }

    std::map<std::string, PySysLinkBase::ConfigurationValue> configuration{
        {"Name", std::string("typed shared-library test")},
        {"Id", std::string("typed-shared-library-test")},
        {"InputPortNumber", 3},
        {"OutputPortNumber", 2},
        {"InputPortTypes", std::vector<std::string>{
            "FullySupportedSignalValue.int",
            "FullySupportedSignalValue.bool",
            "FullySupportedSignalValue.double"}},
        {"OutputPortTypes", std::vector<std::string>{
            "FullySupportedSignalValue.double",
            "FullySupportedSignalValue.int"}},
        {"SampleTime", 0.1},
        {"SharedLibraryPath", std::string(argv[1])},
        {"ComputeFunctionName", std::string("computeTypedTestBlock")}};

    auto block = std::make_shared<BlockTypeSupports::SharedLibrarySupport::SimulationBlockSharedLibrary>(
        configuration,
        nullptr,
        PySysLinkBase::ParsePortTypeMetadatas(
            std::get<std::vector<std::string>>(configuration.at("InputPortTypes"))),
        PySysLinkBase::ParsePortTypeMetadatas(
            std::get<std::vector<std::string>>(configuration.at("OutputPortTypes"))),
        std::vector<bool>{true, true, true});

    const auto inputs = block->GetInputPorts();
    inputs[0]->SetValue(std::make_shared<PySysLinkBase::SignalValue<int>>(4));
    inputs[1]->SetValue(std::make_shared<PySysLinkBase::SignalValue<bool>>(true));
    inputs[2]->SetValue(std::make_shared<PySysLinkBase::SignalValue<double>>(2.5));

    const auto outputs = block->ComputeOutputsOfBlock(
        std::make_shared<PySysLinkBase::SampleTime>(PySysLinkBase::discrete, 0.1),
        3.0);

    const auto outputValue = outputs[0]->GetValue()->TryCastToTyped<double>();
    const auto outputNumber = outputs[1]->GetValue()->TryCastToTyped<int>();
    if (outputValue->GetPayload() != 13.0 || outputNumber->GetPayload() != 4)
    {
        std::cerr << "Typed shared-library block returned unexpected outputs\n";
        return 1;
    }

    return 0;
}
