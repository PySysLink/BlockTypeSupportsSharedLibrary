#include <tuple>

#include "SharedLibraryBlockApi.h"

namespace
{
    std::tuple<double, int> Compute(
        std::shared_ptr<PySysLinkBase::SampleTime>,
        double currentTime,
        bool isMinorStep,
        int inputA,
        bool enabled,
        double inputB)
    {
        return {
            enabled ? inputA * inputB + currentTime : 0.0,
            isMinorStep ? inputA + 1 : inputA};
    }
}

extern "C" void computeTypedTestBlock(
    const std::shared_ptr<PySysLinkBase::SampleTime>& sampleTime,
    double currentTime,
    bool isMinorStep,
    const std::vector<BlockTypeSupports::SharedLibrarySupport::SignalValue>& inputs,
    std::vector<BlockTypeSupports::SharedLibrarySupport::SignalValue>& outputs)
{
    BlockTypeSupports::SharedLibrarySupport::InvokeTypedFunction<&Compute>(
        sampleTime, currentTime, isMinorStep, inputs, outputs);
}
