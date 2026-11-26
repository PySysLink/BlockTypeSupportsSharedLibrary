import sys
import tempfile
import unittest
from pathlib import Path

from pysyslink_toolkit.HighLevelBlock import HighLevelBlock
from pysyslink_toolkit.PortType import FullySupportedSignalValueType, PortType

sys.path.insert(
    0,
    str(
        Path(__file__).resolve().parents[1]
        / "toolkit_shared_library_plugin"
    ),
)
from shared_library_plugin import SharedLibraryPlugin


class SharedLibraryPluginTests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.root = Path(self.temporary_directory.name)
        self.system_path = self.root / "system.pslk"
        self.system_path.touch()

    def _block(self, block_type, properties):
        return HighLevelBlock(
            id=f"test-{block_type}",
            label=f"test {block_type}",
            input_ports=1,
            input_port_types=[
                PortType(signal_value_type=FullySupportedSignalValueType.Int)
            ],
            output_ports=1,
            output_port_types=[
                PortType(signal_value_type=FullySupportedSignalValueType.Double)
            ],
            block_library="shared_library_plugin",
            block_type=block_type,
            properties={
                name: {"type": "string", "value": value}
                for name, value in properties.items()
            }
            | {"SampleTime": {"type": "double", "value": 0.25}},
        )

    def test_embedded_code_builds_and_lowers_to_shared_library_block(self):
        code = """
std::tuple<double> computeOutputs(
    std::shared_ptr<PySysLinkBase::SampleTime>,
    double currentTime,
    bool,
    int input)
{
    return {currentTime + input};
}
"""
        block = self._block(
            "CppBlockEmbedded",
            {"Code": code, "ComputeFunctionName": "computeBlock"},
        )

        result = SharedLibraryPlugin(None)._compile_block(
            block, str(self.system_path)
        )

        self._assert_lowered_block(result, "computeBlock")

    def test_cmake_project_builds_and_lowers_to_shared_library_block(self):
        project = self.root / "source"
        project.mkdir()
        (project / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.16)\n"
            "project(TestUserBlock LANGUAGES CXX)\n"
            "find_package(PySysLinkBase REQUIRED)\n"
            "find_package(BlockTypeSupportsSharedLibrarySupport REQUIRED)\n"
            "add_library(pysyslink_block SHARED "
            "${CMAKE_SOURCE_DIR}/block.cpp)\n"
            "target_include_directories(pysyslink_block PRIVATE "
            "${BLOCK_TYPE_SUPPORTS_SHARED_LIBRARY_SUPPORT_INCLUDE_DIRS})\n"
            "target_link_libraries(pysyslink_block PRIVATE "
            "PySysLinkBase::PySysLinkBase)\n",
            encoding="utf-8",
        )
        (project / "block.cpp").write_text(
            """
#include <BlockTypeSupports/SharedLibrarySupport/SharedLibraryBlockApi.h>

extern "C" void computeBlock(
    const std::shared_ptr<PySysLinkBase::SampleTime>&,
    double,
    bool,
    const std::vector<BlockTypeSupports::SharedLibrarySupport::SignalValue>& inputs,
    std::vector<BlockTypeSupports::SharedLibrarySupport::SignalValue>& outputs)
{
    outputs[0] = static_cast<double>(std::get<int>(inputs[0]));
}
""",
            encoding="utf-8",
        )
        block = self._block(
            "CMakeBlock",
            {
                "ProjectDirectory": "source",
                "TargetName": "pysyslink_block",
                "ComputeFunctionName": "computeBlock",
            },
        )

        result = SharedLibraryPlugin(None)._compile_block(
            block, str(self.system_path)
        )

        self._assert_lowered_block(result, "computeBlock")

    def _assert_lowered_block(self, result, function_name):
        self.assertEqual(len(result.blocks), 1)
        low_level_block = result.blocks[0]
        self.assertEqual(low_level_block.block_class, "SharedLibraryBlocks/SharedLibrary")
        self.assertEqual(low_level_block.extra["ComputeFunctionName[string]"], function_name)
        self.assertEqual(low_level_block.extra["SampleTime[double]"], 0.25)
        self.assertTrue(
            Path(low_level_block.extra["SharedLibraryPath[string]"]).is_file()
        )
        self.assertEqual(result.port_map[("input", 0)], (low_level_block.id, 0))
        self.assertEqual(result.port_map[("output", 0)], (low_level_block.id, 0))


if __name__ == "__main__":
    unittest.main()
