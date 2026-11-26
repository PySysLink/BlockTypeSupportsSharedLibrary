import hashlib
import json
import re
import subprocess
from pathlib import Path

import pysyslink_toolkit
from pysyslink_toolkit.HighLevelBlock import HighLevelBlock
from pysyslink_toolkit.LowLevelBlockStructure import LowLevelBlock, LowLevelBlockStructure
from pysyslink_toolkit.PortType import PortCategory


class SharedLibraryPlugin(pysyslink_toolkit.BlockLibraryPlugin):
    _EMBEDDED_FUNCTION_NAME = "computeOutputs"
    _DEFAULT_PROJECT_TARGET = "pysyslink_block"

    def _compile_block(
        self, high_level_block: HighLevelBlock, pslk_path: str
    ) -> LowLevelBlockStructure:
        self._validate_ports(high_level_block)
        build_root = self._get_build_root(high_level_block, pslk_path)
        build_directory = build_root / "build"
        build_root.mkdir(parents=True, exist_ok=True)
        function_name = self._get_property_value(
            high_level_block, "ComputeFunctionName", "computeBlock"
        )
        self._validate_identifier(function_name, "ComputeFunctionName")

        if high_level_block.block_type == "CppBlockEmbedded":
            library_path = self._build_embedded_block(
                high_level_block, build_root, build_directory, function_name
            )
        elif high_level_block.block_type == "CMakeBlock":
            library_path = self._build_cmake_block(
                high_level_block, pslk_path, build_root, build_directory
            )
        else:
            raise ValueError(
                f"Unsupported shared-library high-level block type: "
                f"{high_level_block.block_type}"
            )

        block = LowLevelBlock(
            id=high_level_block.id,
            name=f"{high_level_block.label} Low Level",
            block_type="SharedLibrary",
            block_class="SharedLibraryBlocks/SharedLibrary",
            input_port_number=high_level_block.input_ports,
            input_port_types=[
                port_type.to_string()
                for port_type in high_level_block.input_port_types
            ],
            output_port_number=high_level_block.output_ports,
            output_port_types=[
                port_type.to_string()
                for port_type in high_level_block.output_port_types
            ],
            **{
                "SharedLibraryPath[string]": str(library_path),
                "ComputeFunctionName[string]": function_name,
                "SampleTime[double]": float(
                    self._get_property_value(high_level_block, "SampleTime", -1)
                ),
            },
        )

        port_map = {}
        for port_index in range(high_level_block.input_ports):
            port_map[("input", port_index)] = (block.id, port_index)
        for port_index in range(high_level_block.output_ports):
            port_map[("output", port_index)] = (block.id, port_index)

        return LowLevelBlockStructure([block], [], port_map)

    def _build_embedded_block(
        self, block, build_root, build_directory, function_name
    ):
        code = self._get_property_value(block, "Code", "")
        if not isinstance(code, str) or not code.strip():
            raise ValueError("CppBlockEmbedded requires non-empty Code")

        self._write_embedded_project(build_root, code, function_name)
        self._run_cmake_configure(build_root, build_directory)
        self._run_cmake_build(build_directory, "pysyslink_embedded_block")

        library_path = build_directory / "artifact" / "libpysyslink_embedded_block.so"
        if not library_path.is_file():
            raise FileNotFoundError(
                f"Embedded block build did not produce the expected shared "
                f"library: {library_path}"
            )
        return library_path.resolve()

    def _write_embedded_project(self, build_root, code, function_name):
        source = (
            "#include <memory>\n"
            "#include <tuple>\n"
            "#include <vector>\n"
            "#include <BlockTypeSupports/SharedLibrarySupport/SharedLibraryBlockApi.h>\n\n"
            f"{code}\n\n"
            "extern \"C\" void "
            f"{function_name}(\n"
            "    const std::shared_ptr<PySysLinkBase::SampleTime>& sampleTime,\n"
            "    double currentTime,\n"
            "    bool isMinorStep,\n"
            "    const std::vector<BlockTypeSupports::SharedLibrarySupport::SignalValue>& inputs,\n"
            "    std::vector<BlockTypeSupports::SharedLibrarySupport::SignalValue>& outputs)\n"
            "{\n"
            "    BlockTypeSupports::SharedLibrarySupport::InvokeTypedFunction<"
            f"&{self._EMBEDDED_FUNCTION_NAME}>(\n"
            "        sampleTime, currentTime, isMinorStep, inputs, outputs);\n"
            "}\n"
        )
        (build_root / "embedded_block.cpp").write_text(source, encoding="utf-8")
        (build_root / "CMakeLists.txt").write_text(
            self._embedded_cmake_project(), encoding="utf-8"
        )

    @staticmethod
    def _embedded_cmake_project():
        return """cmake_minimum_required(VERSION 3.16)
project(PySysLinkEmbeddedBlock LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PySysLinkBase REQUIRED)
find_package(BlockTypeSupportsSharedLibrarySupport REQUIRED)

add_library(pysyslink_embedded_block SHARED embedded_block.cpp)
target_include_directories(pysyslink_embedded_block PRIVATE
    ${PySysLinkBase_INCLUDE_DIRS}
    ${BLOCK_TYPE_SUPPORTS_SHARED_LIBRARY_SUPPORT_INCLUDE_DIRS})
target_link_libraries(pysyslink_embedded_block PRIVATE PySysLinkBase::PySysLinkBase)

set(_artifact_directory "${CMAKE_BINARY_DIR}/artifact")
set_target_properties(pysyslink_embedded_block PROPERTIES
    OUTPUT_NAME pysyslink_embedded_block
    LIBRARY_OUTPUT_DIRECTORY "${_artifact_directory}")
foreach(_configuration DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(pysyslink_embedded_block PROPERTIES
        "LIBRARY_OUTPUT_DIRECTORY_${_configuration}" "${_artifact_directory}")
endforeach()
"""

    def _build_cmake_block(
        self, block, pslk_path, build_root, build_directory
    ):
        project_directory = self._get_project_directory(block, pslk_path)
        target_name = self._get_property_value(
            block, "TargetName", self._DEFAULT_PROJECT_TARGET
        )
        self._validate_identifier(target_name, "TargetName")
        configure_arguments = self._get_property_value(
            block, "CMakeConfigureArguments", []
        )
        if not isinstance(configure_arguments, (list, tuple)) or not all(
            isinstance(argument, str) for argument in configure_arguments
        ):
            raise ValueError("CMakeConfigureArguments must be a list of strings")
        if any(not argument.startswith("-D") for argument in configure_arguments):
            raise ValueError(
                "CMakeConfigureArguments only accepts CMake -D options"
            )

        query_directory = build_directory / ".cmake" / "api" / "v1" / "query"
        query_directory.mkdir(parents=True, exist_ok=True)
        (query_directory / "codemodel-v2").touch()
        self._run_cmake_configure(
            project_directory,
            build_directory,
            list(configure_arguments),
        )
        self._run_cmake_build(build_directory, target_name)
        return SharedLibraryPlugin._get_cmake_target_library(
            build_directory, target_name
        )

    @staticmethod
    def _get_cmake_target_library(build_directory, target_name):
        reply_directory = build_directory / ".cmake" / "api" / "v1" / "reply"
        index_files = list(reply_directory.glob("index-*.json"))
        if not index_files:
            raise FileNotFoundError("CMake did not return File API codemodel data")

        index_file = max(index_files, key=lambda path: path.stat().st_mtime_ns)
        index = json.loads(index_file.read_text(encoding="utf-8"))
        codemodel_reference = index.get("reply", {}).get("codemodel-v2")
        if codemodel_reference is None:
            raise RuntimeError("CMake File API response has no codemodel-v2 entry")

        codemodel_path = reply_directory / codemodel_reference["jsonFile"]
        codemodel = json.loads(codemodel_path.read_text(encoding="utf-8"))
        configurations = codemodel.get("configurations", [])
        configuration = next(
            (item for item in configurations if item.get("name") == "Release"),
            configurations[0] if configurations else None,
        )
        if configuration is None:
            raise RuntimeError("CMake File API response has no build configuration")

        target_reference = next(
            (
                target
                for target in configuration.get("targets", [])
                if target.get("name") == target_name
            ),
            None,
        )
        if target_reference is None:
            raise ValueError(
                f"CMake project has no target named '{target_name}'"
            )

        target_path = reply_directory / target_reference["jsonFile"]
        target = json.loads(target_path.read_text(encoding="utf-8"))
        if target.get("type") != "SHARED_LIBRARY":
            raise ValueError(
                f"CMake target '{target_name}' must be a SHARED_LIBRARY"
            )

        artifacts = target.get("artifacts", [])
        if len(artifacts) != 1:
            raise RuntimeError(
                f"CMake target '{target_name}' must produce exactly one "
                "shared-library artifact"
            )
        library_path = Path(artifacts[0]["path"])
        if not library_path.is_absolute():
            library_path = build_directory / library_path
        if not library_path.is_file():
            raise FileNotFoundError(
                f"CMake target did not produce a shared library: {library_path}"
            )
        return library_path.resolve()

    @staticmethod
    def _run_cmake_configure(source_directory, build_directory, extra_args=None):
        command = [
            "cmake",
            "-S",
            str(source_directory),
            "-B",
            str(build_directory),
            "-DCMAKE_BUILD_TYPE=Release",
        ]
        command.extend(extra_args or [])
        SharedLibraryPlugin._run_command(command, "configure the C++ block")

    @staticmethod
    def _run_cmake_build(build_directory, target_name):
        SharedLibraryPlugin._run_command(
            [
                "cmake",
                "--build",
                str(build_directory),
                "--config",
                "Release",
                "--target",
                target_name,
            ],
            "build the C++ block",
        )

    @staticmethod
    def _run_command(command, description):
        try:
            result = subprocess.run(
                command,
                check=True,
                capture_output=True,
                text=True,
            )
        except FileNotFoundError as error:
            raise RuntimeError(
                "CMake is required to build shared-library blocks"
            ) from error
        except subprocess.CalledProcessError as error:
            output = "\n".join(
                part for part in (error.stdout, error.stderr) if part
            )
            raise RuntimeError(
                f"Failed to {description} (exit code {error.returncode}):\n"
                f"{output}"
            ) from error

        if result.stdout:
            print(result.stdout, end="")
        if result.stderr:
            print(result.stderr, end="")

    @staticmethod
    def _get_project_directory(block, pslk_path):
        configured_path = SharedLibraryPlugin._get_property_value(
            block, "ProjectDirectory", ""
        )
        if not configured_path:
            raise ValueError("CMakeBlock requires ProjectDirectory")
        project_directory = Path(configured_path)
        if not project_directory.is_absolute():
            project_directory = Path(pslk_path).resolve().parent / project_directory
        project_directory = project_directory.resolve()
        if not (project_directory / "CMakeLists.txt").is_file():
            raise FileNotFoundError(
                f"CMakeBlock project must contain CMakeLists.txt: "
                f"{project_directory}"
            )
        return project_directory

    @staticmethod
    def _get_build_root(block, pslk_path):
        system_path = Path(pslk_path).resolve()
        system_hash = hashlib.sha256(
            str(system_path).encode("utf-8")
        ).hexdigest()[:12]
        block_hash = hashlib.sha256(block.id.encode("utf-8")).hexdigest()[:12]
        return (
            system_path.parent
            / "build"
            / "shared_library"
            / f"{system_path.stem}-{system_hash}"
            / block_hash
        )

    @staticmethod
    def _validate_ports(block):
        if len(block.input_port_types) != block.input_ports:
            raise ValueError("Input port count does not match InputPortTypes")
        if len(block.output_port_types) != block.output_ports:
            raise ValueError("Output port count does not match OutputPortTypes")

        for direction, port_types in (
            ("input", block.input_port_types),
            ("output", block.output_port_types),
        ):
            for index, port_type in enumerate(port_types):
                if (
                    port_type.port_category
                    != PortCategory.fully_supported_signal_value
                    or port_type.signal_value_type is None
                ):
                    raise ValueError(
                        f"Port {direction} {index} must use a fully supported "
                        "signal value type"
                    )

    @staticmethod
    def _validate_identifier(value, property_name):
        if not isinstance(value, str) or not re.fullmatch(
            r"[A-Za-z_][A-Za-z0-9_]*", value
        ):
            raise ValueError(
                f"{property_name} must be a valid C++ identifier"
            )

    @staticmethod
    def _get_property_value(block, property_name, default=None):
        property_value = block.properties.get(property_name)
        if property_value is None:
            return default
        if isinstance(property_value, dict):
            return property_value.get("value", default)
        return property_value
