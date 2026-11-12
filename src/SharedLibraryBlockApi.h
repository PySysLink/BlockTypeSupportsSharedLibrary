#ifndef SRC_SHARED_LIBRARY_BLOCK_API
#define SRC_SHARED_LIBRARY_BLOCK_API

#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <PySysLinkBase/FullySupportedSignalValue.h>
#include <PySysLinkBase/SampleTime.h>

namespace BlockTypeSupports::SharedLibrarySupport
{
    using SignalValue = PySysLinkBase::FullySupportedSignalValue;

    using SharedLibraryComputeFunction = void (*)(
        const std::shared_ptr<PySysLinkBase::SampleTime>& sampleTime,
        double currentTime,
        bool isMinorStep,
        const std::vector<SignalValue>& inputs,
        std::vector<SignalValue>& outputs);

    template <typename Function>
    struct TypedFunctionTraits;

    template <typename Return, typename... Args>
    struct TypedFunctionTraits<Return (*)(Args...)>
    {
        using ReturnType = Return;
        using ArgsTuple = std::tuple<Args...>;
        static constexpr std::size_t ArgumentCount = sizeof...(Args);
    };

    template <typename T>
    struct IsStdTuple : std::false_type
    {
    };

    template <typename... Types>
    struct IsStdTuple<std::tuple<Types...>> : std::true_type
    {
    };

    template <
        auto Function,
        std::size_t... InputIndices,
        std::size_t... OutputIndices>
    void InvokeTypedFunction(
        const std::shared_ptr<PySysLinkBase::SampleTime>& sampleTime,
        double currentTime,
        bool isMinorStep,
        const std::vector<SignalValue>& inputs,
        std::vector<SignalValue>& outputs,
        std::index_sequence<InputIndices...>,
        std::index_sequence<OutputIndices...>)
    {
        using Traits = TypedFunctionTraits<decltype(Function)>;
        using Args = typename Traits::ArgsTuple;
        using Return = typename Traits::ReturnType;

        static_assert(Traits::ArgumentCount >= 3, "Typed block functions require sample time, current time, and minor-step arguments");
        static_assert(IsStdTuple<Return>::value, "Typed block functions must return a std::tuple");

        if (inputs.size() != sizeof...(InputIndices))
        {
            throw std::invalid_argument("Typed block function input count does not match configured input ports");
        }

        auto result = Function(
            sampleTime,
            currentTime,
            isMinorStep,
            std::get<
                std::remove_cv_t<std::remove_reference_t<
                    std::tuple_element_t<InputIndices + 3, Args>>>>(inputs[InputIndices])...);

        outputs.resize(sizeof...(OutputIndices));
        ((outputs[OutputIndices] = std::get<OutputIndices>(std::move(result))), ...);
    }

    template <auto Function>
    void InvokeTypedFunction(
        const std::shared_ptr<PySysLinkBase::SampleTime>& sampleTime,
        double currentTime,
        bool isMinorStep,
        const std::vector<SignalValue>& inputs,
        std::vector<SignalValue>& outputs)
    {
        using Traits = TypedFunctionTraits<decltype(Function)>;
        using Return = typename Traits::ReturnType;

        static_assert(Traits::ArgumentCount >= 3, "Typed block functions require sample time, current time, and minor-step arguments");
        static_assert(IsStdTuple<Return>::value, "Typed block functions must return a std::tuple");
        using InputIndices = std::make_index_sequence<Traits::ArgumentCount - 3>;
        using OutputIndices = std::make_index_sequence<std::tuple_size_v<Return>>;
        InvokeTypedFunction<Function>(
            sampleTime,
            currentTime,
            isMinorStep,
            inputs,
            outputs,
            InputIndices{},
            OutputIndices{});
    }
}

#endif /* SRC_SHARED_LIBRARY_BLOCK_API */
