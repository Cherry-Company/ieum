# SPDX-FileCopyrightText: (C) 2026 Ieum contributors
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

from __future__ import annotations

import importlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parents[1]
GENERATOR = TOOLS_DIR / "generate-codegraph.py"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))


class CodeGraphCoreTests(unittest.TestCase):
    def load_module(self):
        module_path = TOOLS_DIR / "codegraph.py"
        if not module_path.is_file():
            self.fail("tools/codegraph.py must provide the code graph API")
        return importlib.import_module("codegraph")

    def require_api(self, module, *names: str) -> None:
        missing = [name for name in names if not hasattr(module, name)]
        if missing:
            self.fail(f"codegraph API is missing: {', '.join(missing)}")

    def make_repository(self, files: dict[str, str]) -> Path:
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        for relative_path, content in files.items():
            destination = root / relative_path
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(content, encoding="utf-8", newline="\n")
        subprocess.run(["git", "init", "-q"], cwd=root, check=True, timeout=10)
        subprocess.run(["git", "add", "."], cwd=root, check=True, timeout=10)
        return root

    def test_classification_keeps_runtime_components_separate(self):
        """Catches collapsing applications and libraries into one root node."""
        codegraph = self.load_module()

        net = codegraph.classify_component("src/lib/net/SecureSocket.cpp")
        core = codegraph.classify_component("src/apps/deskflow-core/deskflow-core.cpp")

        self.assertEqual(
            (net.id, net.kind, net.path),
            ("lib:net", "library", "src/lib/net"),
        )
        self.assertEqual(
            (core.id, core.kind, core.path),
            ("app:deskflow-core", "application", "src/apps/deskflow-core"),
        )

    def test_local_include_parser_excludes_system_headers_and_deduplicates(self):
        """Catches treating Qt/system headers as repository file edges."""
        codegraph = self.load_module()
        source = """
            #include <QCoreApplication>
            # include "net/SecureSocket.h"
            #include "base/Log.h"
            #include "net/SecureSocket.h"
        """

        self.assertEqual(
            codegraph.parse_local_includes(source),
            ["base/Log.h", "net/SecureSocket.h"],
        )

    def test_classification_covers_tests_tools_deployment_and_support(self):
        """Catches dropping non-runtime code from the component inventory."""
        codegraph = self.load_module()
        cases = {
            "src/unittests/net/SecureUtilsTests.cpp": (
                "test:net",
                "test",
                "src/unittests/net",
            ),
            "tools/cgtap-logger/main.swift": (
                "tool:cgtap-logger",
                "tool",
                "tools/cgtap-logger",
            ),
            "tools/merge-input-timeline.py": (
                "tool:merge-input-timeline",
                "tool",
                "tools/merge-input-timeline.py",
            ),
            "deploy/windows/deploy.cmake": (
                "deploy:windows",
                "deployment",
                "deploy/windows",
            ),
            ".github/workflows/continuous-integration.yml": (
                "support:github",
                "automation",
                ".github",
            ),
            "translations/deskflow_ko.ts": (
                "support:translations",
                "localization",
                "translations",
            ),
            "src/apps/CMakeLists.txt": (
                "support:source-build",
                "build",
                "src",
            ),
            "src/lib/CMakeLists.txt": (
                "support:source-build",
                "build",
                "src",
            ),
            "src/unittests/CMakeLists.txt": (
                "support:source-build",
                "build",
                "src",
            ),
            "tools/tests/test_codegraph.py": (
                "test:codegraph-tools",
                "test",
                "tools/tests",
            ),
        }

        actual = {
            path: (
                codegraph.classify_component(path).id,
                codegraph.classify_component(path).kind,
                codegraph.classify_component(path).path,
            )
            for path in cases
        }

        self.assertEqual(actual, cases)

    def test_include_resolution_prefers_relative_then_public_source_roots(self):
        """Catches unresolved internal edges and same-name header misresolution."""
        codegraph = self.load_module()
        self.require_api(codegraph, "resolve_local_include")
        tracked = {
            "src/lib/base/Log.h",
            "src/lib/common/Constants.h.in",
            "src/lib/gui/ScreenSetupModel.h",
            "src/lib/gui/dialogs/ServerConfigDialog.cpp",
            "src/lib/net/SecureSocket.cpp",
            "src/lib/net/SecureSocket.h",
        }

        self.assertEqual(
            codegraph.resolve_local_include(
                "src/lib/net/SecureSocket.cpp", "SecureSocket.h", tracked
            ),
            "src/lib/net/SecureSocket.h",
        )
        self.assertEqual(
            codegraph.resolve_local_include(
                "src/lib/net/SecureSocket.cpp", "base/Log.h", tracked
            ),
            "src/lib/base/Log.h",
        )
        self.assertEqual(
            codegraph.resolve_local_include(
                "src/lib/gui/dialogs/ServerConfigDialog.cpp",
                "ScreenSetupModel.h",
                tracked,
            ),
            "src/lib/gui/ScreenSetupModel.h",
        )
        self.assertEqual(
            codegraph.resolve_local_include(
                "src/lib/net/SecureSocket.cpp", "common/Constants.h", tracked
            ),
            "src/lib/common/Constants.h.in",
        )
        self.assertIsNone(
            codegraph.resolve_local_include(
                "src/lib/net/SecureSocket.cpp", "missing/Local.h", tracked
            )
        )

    def test_cmake_parser_extracts_targets_and_link_dependencies(self):
        """Catches losing build-system edges or treating visibility as a target."""
        codegraph = self.load_module()
        self.require_api(codegraph, "parse_cmake")
        cmake = """
            add_library(alpha STATIC alpha.cpp)
            add_executable(app
              main.cpp
            )
            target_link_libraries(app PRIVATE alpha Qt6::Core)
            create_test(
              NAME alpha-tests
              WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
              DEPENDS alpha
              SOURCE AlphaTests.cpp
            )
        """

        targets, links = codegraph.parse_cmake(cmake)

        self.assertEqual(
            [(target.name, target.kind) for target in targets],
            [
                ("alpha", "library"),
                ("alpha-tests", "test"),
                ("app", "executable"),
            ],
        )
        self.assertEqual(
            [(link.source, link.target) for link in links],
            [("app", "Qt6::Core"), ("app", "alpha")],
        )

    def test_cmake_parser_resolves_scalar_variables_and_ignores_function_body_templates(
        self,
    ):
        """Catches publishing `${target}` placeholders as concrete CMake targets."""
        codegraph = self.load_module()
        self.require_api(codegraph, "parse_cmake", "parse_cmake_variables")
        root_cmake = """
            set(IEUM_RUNTIME_ID ieum)
            set(IEUM_CORE_TARGET "${IEUM_RUNTIME_ID}-core")
        """
        component_cmake = """
            function(build_runtime target)
              add_executable(${target} template-main.cpp)
              target_link_libraries(${target} PRIVATE template-lib)
            endfunction()
            set(target ${IEUM_CORE_TARGET})
            add_executable(${target} main.cpp)
            target_link_libraries(${target} PRIVATE common)
        """

        variables = codegraph.parse_cmake_variables(root_cmake)
        targets, links = codegraph.parse_cmake(component_cmake, variables)

        self.assertEqual(variables["IEUM_RUNTIME_ID"], "ieum")
        self.assertEqual(variables["IEUM_CORE_TARGET"], "ieum-core")
        self.assertEqual(
            [(target.name, target.kind) for target in targets],
            [("ieum-core", "executable")],
        )
        self.assertEqual(
            [(link.source, link.target) for link in links],
            [("ieum-core", "common")],
        )

    def test_build_graph_records_every_file_and_collapses_component_edges(self):
        """Catches partial inventories and missing cross-component evidence."""
        codegraph = self.load_module()
        self.require_api(codegraph, "build_graph")
        files = {
            "CMakeLists.txt": (
                "add_subdirectory(src/lib/alpha)\n"
                "add_subdirectory(src/lib/beta)\n"
                "add_subdirectory(src/apps/demo)\n"
            ),
            "src/lib/alpha/CMakeLists.txt": "add_library(alpha Alpha.cpp)\n",
            "src/lib/alpha/Alpha.cpp": '#include "Alpha.h"\n',
            "src/lib/alpha/Alpha.h": "#pragma once\n",
            "src/lib/beta/CMakeLists.txt": (
                "add_library(beta Beta.cpp)\ntarget_link_libraries(beta PUBLIC alpha)\n"
            ),
            "src/lib/beta/Beta.cpp": (
                '#include "Beta.h"\n#include "alpha/Alpha.h"\n#include <vector>\n'
            ),
            "src/lib/beta/Beta.h": "#pragma once\n",
            "src/apps/demo/CMakeLists.txt": (
                "add_executable(demo main.cpp)\n"
                "target_link_libraries(demo PRIVATE beta Qt6::Core)\n"
            ),
            "src/apps/demo/main.cpp": (
                '#include "beta/Beta.h"\nint main() { return 0; }\n'
            ),
        }
        root = self.make_repository(files)

        graph = codegraph.build_graph(root)

        self.assertEqual(graph["schemaVersion"], 1)
        self.assertEqual(
            {item["path"] for item in graph["files"]},
            set(files),
        )
        self.assertEqual(graph["summary"]["trackedFiles"], len(files))
        self.assertEqual(
            {(edge["source"], edge["target"]) for edge in graph["fileEdges"]},
            {
                ("src/apps/demo/main.cpp", "src/lib/beta/Beta.h"),
                ("src/lib/alpha/Alpha.cpp", "src/lib/alpha/Alpha.h"),
                ("src/lib/beta/Beta.cpp", "src/lib/alpha/Alpha.h"),
                ("src/lib/beta/Beta.cpp", "src/lib/beta/Beta.h"),
            },
        )
        component_edges = {
            (edge["source"], edge["target"]): set(edge["kinds"])
            for edge in graph["componentEdges"]
        }
        self.assertEqual(
            component_edges[("app:demo", "lib:beta")],
            {"include", "link"},
        )
        self.assertEqual(
            component_edges[("lib:beta", "lib:alpha")],
            {"include", "link"},
        )
        self.assertIn(
            ("cmake-link", "Qt6::Core"),
            {
                (dependency["kind"], dependency["name"])
                for dependency in graph["externalDependencies"]
            },
        )
        self.assertIn(
            ("system-include", "vector"),
            {
                (dependency["kind"], dependency["name"])
                for dependency in graph["externalDependencies"]
            },
        )
        self.assertEqual(
            [entry["path"] for entry in graph["entryPoints"]],
            ["src/apps/demo/main.cpp"],
        )

    def test_build_graph_is_byte_stable_at_one_revision(self):
        """Catches timestamps, filesystem order, and unstable set iteration."""
        codegraph = self.load_module()
        self.require_api(codegraph, "build_graph")
        root = self.make_repository(
            {
                "CMakeLists.txt": "add_subdirectory(src/lib/alpha)\n",
                "src/lib/alpha/CMakeLists.txt": "add_library(alpha Alpha.cpp)\n",
                "src/lib/alpha/Alpha.cpp": "int alpha() { return 1; }\n",
            }
        )

        first = json.dumps(
            codegraph.build_graph(root),
            ensure_ascii=False,
            indent=2,
            sort_keys=True,
        )
        second = json.dumps(
            codegraph.build_graph(root),
            ensure_ascii=False,
            indent=2,
            sort_keys=True,
        )

        self.assertEqual(first, second)

    def test_generator_cli_writes_stable_json_mermaid_and_readme(self):
        """Catches missing, partial, or non-repeatable graph artifacts."""
        if not GENERATOR.is_file():
            self.fail("tools/generate-codegraph.py must provide the generator CLI")
        root = self.make_repository(
            {
                "CMakeLists.txt": "add_subdirectory(src/apps/demo)\n",
                "src/apps/demo/CMakeLists.txt": ("add_executable(demo main.cpp)\n"),
                "src/apps/demo/main.cpp": "int main() { return 0; }\n",
            }
        )
        output = root / "artifacts"
        command = [
            sys.executable,
            str(GENERATOR),
            "--root",
            str(root),
            "--output",
            str(output),
        ]

        first = subprocess.run(
            command,
            check=False,
            text=True,
            capture_output=True,
            timeout=120,
        )

        self.assertEqual(first.returncode, 0, first.stderr)
        json_path = output / "codegraph.json"
        mermaid_path = output / "component-graph.mmd"
        readme_path = output / "README.md"
        self.assertTrue(json_path.is_file())
        self.assertTrue(mermaid_path.is_file())
        self.assertTrue(readme_path.is_file())
        graph = json.loads(json_path.read_text(encoding="utf-8"))
        self.assertEqual(graph["schemaVersion"], 1)
        self.assertEqual(
            [entry["path"] for entry in graph["entryPoints"]],
            ["src/apps/demo/main.cpp"],
        )
        self.assertTrue(
            mermaid_path.read_text(encoding="utf-8").startswith("flowchart LR\n")
        )
        self.assertIn(
            "| `app:demo` | application |",
            readme_path.read_text(encoding="utf-8"),
        )

        before = {
            path.name: path.read_bytes()
            for path in (json_path, mermaid_path, readme_path)
        }
        second = subprocess.run(
            command,
            check=False,
            text=True,
            capture_output=True,
            timeout=120,
        )
        after = {
            path.name: path.read_bytes()
            for path in (json_path, mermaid_path, readme_path)
        }

        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(before, after)

    def test_generator_cli_reports_invalid_root_without_partial_outputs(self):
        """Catches traceback-only failures and misleading partial artifacts."""
        if not GENERATOR.is_file():
            self.fail("tools/generate-codegraph.py must provide the generator CLI")
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        base = Path(temporary.name)
        missing_root = base / "not-a-repository"
        output = base / "artifacts"

        result = subprocess.run(
            [
                sys.executable,
                str(GENERATOR),
                "--root",
                str(missing_root),
                "--output",
                str(output),
            ],
            check=False,
            text=True,
            capture_output=True,
            timeout=30,
        )

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("code graph generation failed:", result.stderr)
        self.assertIn("not a Git repository root", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
