# BlockTypeSupportsSharedLibrary

The configured `InputPortTypes` and `OutputPortTypes` determine the number and
types of ports. The shared-library entry point has one common C++ ABI signature
for every block:

```cpp
#include <BlockTypeSupports/SharedLibrarySupport/SharedLibraryBlockApi.h>

extern "C" void computeOutputs(
    const std::shared_ptr<PySysLinkBase::SampleTime>& sampleTime,
    double currentTime,
    bool isMinorStep,
    const std::vector<BlockTypeSupports::SharedLibrarySupport::SignalValue>& inputs,
    std::vector<BlockTypeSupports::SharedLibrarySupport::SignalValue>& outputs)
{
    outputs[0] = std::get<int>(inputs[0]) + 1;
}
```

`inputs` and `outputs` use `PySysLinkBase::FullySupportedSignalValue`, so the
same entry point handles any supported port count and type combination. The
output vector is initialized to the configured output count and types before
the call; assign each output using the configured type and index.

For a typed user function, the API header provides `InvokeTypedFunction`.
Compile a thin exported adapter that binds the user's function at build time:

```cpp
#include <memory>
#include <string>
#include <tuple>

#include <BlockTypeSupports/SharedLibrarySupport/SharedLibraryBlockApi.h>

std::tuple<int, double, std::string> userCompute(
    std::shared_ptr<PySysLinkBase::SampleTime> sampleTime,
    double currentTime,
    bool isMinorStep,
    int inputA,
    double inputB)
{
    return {inputA, inputB, std::to_string(currentTime)};
}

extern "C" void computeOutputs(
    const std::shared_ptr<PySysLinkBase::SampleTime>& sampleTime,
    double currentTime,
    bool isMinorStep,
    const std::vector<BlockTypeSupports::SharedLibrarySupport::SignalValue>& inputs,
    std::vector<BlockTypeSupports::SharedLibrarySupport::SignalValue>& outputs)
{
    BlockTypeSupports::SharedLibrarySupport::InvokeTypedFunction<&userCompute>(
        sampleTime, currentTime, isMinorStep, inputs, outputs);
}
```

The typed function may have any number of supported input arguments and tuple
outputs. Their compile-time types and order must match the configured port
types and order. The adapter uses `std::get<T>` rather than type-name strings;
an incorrect type configuration raises `std::bad_variant_access`.

Configure the block with `SharedLibraryPath` (the `.so` path) and
`ComputeFunctionName` (`computeOutputs` in the example). The library and
function are resolved once when the block is constructed. The compute path
does not inspect port type-name strings.

The adapter and block library use C++ types and therefore require a compatible
C++ ABI (compiler, standard library, and build settings) between the two
libraries. The existing port API returns cloned signal values, so input
retrieval still incurs copies.

The installed block-library manifest defines a `SharedLibrary` block with
these configuration values:

| Parameter | Type | Meaning |
|---|---|---|
| `SharedLibraryPath` | `string` | Path to the user-provided `.so` |
| `ComputeFunctionName` | `string` | Exported entry-point name |
| `InputPortTypes` | `string[]` | One port type per input, e.g. `FullySupportedSignalValue.int` |
| `OutputPortTypes` | `string[]` | One port type per output, in the same format |
| `SampleTime` | `double` | Discrete sample time; `-1` requests inherited discrete time |

Port counts come from the lengths of the type arrays. A positive `SampleTime`
creates a discrete sample time; the default `-1` allows the simulation to
resolve an inherited discrete sample time.