#!/usr/bin/env python3
"""
Dependency Analysis for Panda Codebase

This script analyzes the current panda codebase to extract:
1. Source file dependencies through #include analysis
2. Module boundaries based on directory structure
3. Target-specific requirements
4. Dependency graph visualization

The output helps inform the modular refactoring process.
"""

import re
from pathlib import Path
from collections import defaultdict, deque
import json

class DependencyAnalyzer:
    """Analyzes source code dependencies in the panda codebase."""

    def __init__(self, root_dir="."):
        self.root_dir = Path(root_dir)
        self.includes = defaultdict(set)  # file -> set of included files
        self.dependencies = defaultdict(set)  # file -> set of files it depends on
        self.reverse_deps = defaultdict(set)  # file -> set of files that depend on it
        self.all_files = set()

    def find_source_files(self):
        """Find all relevant source files in the codebase."""
        patterns = ["*.c", "*.h", "*.cpp", "*.hpp"]
        files = []

        for pattern in patterns:
            files.extend(self.root_dir.rglob(pattern))

        # Filter out certain directories
        excluded_dirs = {"tests", "obj", ".git", "__pycache__"}

        filtered_files = []
        for f in files:
            if not any(part in excluded_dirs for part in f.parts):
                filtered_files.append(f)

        return filtered_files

    def extract_includes(self, file_path):
        """Extract #include statements from a source file."""
        includes = set()

        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            # Find #include statements
            include_pattern = r'#include\s+[<"]([^>"]+)[>"]'
            matches = re.findall(include_pattern, content)

            for match in matches:
                includes.add(match)

        except Exception as e:
            print(f"Warning: Could not read {file_path}: {e}")

        return includes

    def resolve_include_path(self, include_str, current_file):
        """Resolve an include string to an actual file path."""
        # Try relative to current file directory
        current_dir = current_file.parent
        candidate = current_dir / include_str
        if candidate.exists():
            return candidate.resolve()

        # Try relative to project root
        candidate = self.root_dir / include_str
        if candidate.exists():
            return candidate.resolve()

        # Try common include directories
        include_dirs = [
            "board",
            "board/stm32h7/inc",
            "board/drivers",
            "crypto",
            "opendbc/safety",
        ]

        for inc_dir in include_dirs:
            candidate = self.root_dir / inc_dir / include_str
            if candidate.exists():
                return candidate.resolve()

        return None

    def analyze_dependencies(self):
        """Analyze dependencies across all source files."""
        print("Finding source files...")
        source_files = self.find_source_files()
        self.all_files = set(source_files)

        print(f"Found {len(source_files)} source files")
        print("Extracting dependencies...")

        for file_path in source_files:
            includes = self.extract_includes(file_path)
            self.includes[file_path] = includes

            # Resolve includes to actual file paths
            for include_str in includes:
                resolved = self.resolve_include_path(include_str, file_path)
                if resolved and resolved in self.all_files:
                    self.dependencies[file_path].add(resolved)
                    self.reverse_deps[resolved].add(file_path)

        print(f"Analyzed dependencies for {len(source_files)} files")

    def identify_modules(self):
        """Identify logical modules based on directory structure and dependencies."""
        modules = {}

        # Group files by directory structure
        dir_groups = defaultdict(list)
        for file_path in self.all_files:
            # Get the logical directory grouping
            parts = file_path.parts
            if "board" in parts:
                board_idx = parts.index("board")
                if board_idx + 1 < len(parts):
                    if parts[board_idx + 1] == "drivers":
                        # Group by driver type
                        module_name = f"drivers_{parts[board_idx + 2].split('.')[0]}" if board_idx + 2 < len(parts) else "drivers"
                    elif parts[board_idx + 1] == "stm32h7":
                        module_name = "hal_stm32h7"
                    elif parts[board_idx + 1] == "jungle":
                        module_name = "jungle"
                    else:
                        module_name = f"board_{parts[board_idx + 1]}"
                else:
                    module_name = "board_core"
            elif "crypto" in parts:
                module_name = "crypto"
            elif "opendbc" in parts:
                module_name = "safety"
            else:
                module_name = "misc"

            dir_groups[module_name].append(file_path)

        # Create module definitions
        for module_name, files in dir_groups.items():
            # Calculate module dependencies
            internal_files = set(files)
            external_deps = set()

            for file_path in files:
                for dep in self.dependencies[file_path]:
                    if dep not in internal_files:
                        # Find which module this dependency belongs to
                        for other_module, other_files in dir_groups.items():
                            if dep in other_files and other_module != module_name:
                                external_deps.add(other_module)
                                break

            modules[module_name] = {
                'files': [str(f.relative_to(self.root_dir)) for f in files],
                'dependencies': list(external_deps),
                'description': self._generate_module_description(module_name, files)
            }

        return modules

    def _generate_module_description(self, module_name, files):
        """Generate a description for a module based on its name and files."""
        descriptions = {
            'hal_stm32h7': 'STM32H7 hardware abstraction layer',
            'drivers_fan': 'Fan control driver',
            'drivers_led': 'LED control driver',
            'drivers_uart': 'UART communication driver',
            'drivers_usb': 'USB communication driver',
            'drivers_can': 'CAN bus driver',
            'drivers_spi': 'SPI communication driver',
            'drivers': 'General hardware drivers',
            'jungle': 'Jungle-specific functionality',
            'board_core': 'Core board functionality',
            'crypto': 'Cryptographic functions',
            'safety': 'Safety-critical CAN communication',
        }

        return descriptions.get(module_name, f'Module for {module_name}')

    def analyze_targets(self):
        """Analyze target-specific dependencies."""
        targets = {}

        # Panda target
        panda_main = self.root_dir / "board" / "main.c"
        if panda_main.exists():
            panda_deps = self._get_transitive_dependencies(panda_main)
            targets['panda_h7'] = {
                'main_file': 'board/main.c',
                'dependencies': [str(f.relative_to(self.root_dir)) for f in panda_deps],
                'description': 'Main panda firmware'
            }

        # Jungle target
        jungle_main = self.root_dir / "board" / "jungle" / "main.c"
        if jungle_main.exists():
            jungle_deps = self._get_transitive_dependencies(jungle_main)
            targets['panda_jungle_h7'] = {
                'main_file': 'board/jungle/main.c',
                'dependencies': [str(f.relative_to(self.root_dir)) for f in jungle_deps],
                'description': 'Jungle firmware'
            }

        return targets

    def _get_transitive_dependencies(self, start_file):
        """Get all transitive dependencies of a file."""
        visited = set()
        to_visit = deque([start_file])

        while to_visit:
            current = to_visit.popleft()
            if current in visited:
                continue

            visited.add(current)

            # Add direct dependencies
            for dep in self.dependencies.get(current, set()):
                if dep not in visited:
                    to_visit.append(dep)

        return visited

    def generate_report(self):
        """Generate a comprehensive dependency analysis report."""
        print("\n" + "="*60)
        print("PANDA DEPENDENCY ANALYSIS REPORT")
        print("="*60)

        # Basic statistics
        print("\nBASIC STATISTICS:")
        print(f"Total source files: {len(self.all_files)}")
        print(f"Total include relationships: {sum(len(deps) for deps in self.dependencies.values())}")

        # Identify modules
        modules = self.identify_modules()
        print(f"\nIDENTIFIED MODULES ({len(modules)}):")
        for name, info in modules.items():
            print(f"  {name}: {len(info['files'])} files - {info['description']}")
            if info['dependencies']:
                print(f"    Dependencies: {', '.join(info['dependencies'])}")

        # Analyze targets
        targets = self.analyze_targets()
        print(f"\nTARGET ANALYSIS ({len(targets)}):")
        for name, info in targets.items():
            print(f"  {name}: {info['description']}")
            print(f"    Main file: {info['main_file']}")
            print(f"    Total dependencies: {len(info['dependencies'])} files")

        # Find highly connected files
        print("\nHIGHLY CONNECTED FILES:")
        dep_counts = [(len(deps), f) for f, deps in self.reverse_deps.items()]
        dep_counts.sort(reverse=True)

        for count, file_path in dep_counts[:10]:
            rel_path = file_path.relative_to(self.root_dir)
            print(f"  {rel_path}: {count} files depend on it")

        # Find potential circular dependencies
        print("\nCIRCULAR DEPENDENCY CHECK:")
        circular = self._find_circular_dependencies()
        if circular:
            print(f"  Found {len(circular)} potential circular dependency chains")
            for cycle in circular[:5]:  # Show first 5
                cycle_str = " -> ".join(str(f.relative_to(self.root_dir)) for f in cycle)
                print(f"    {cycle_str}")
        else:
            print("  No circular dependencies found")

        return {
            'modules': modules,
            'targets': targets,
            'statistics': {
                'total_files': len(self.all_files),
                'total_includes': sum(len(deps) for deps in self.dependencies.values())
            }
        }

    def _find_circular_dependencies(self):
        """Find circular dependencies using DFS."""
        visited = set()
        rec_stack = set()
        cycles = []

        def dfs(node, path):
            if node in rec_stack:
                # Found a cycle
                cycle_start = path.index(node)
                cycles.append(path[cycle_start:] + [node])
                return

            if node in visited:
                return

            visited.add(node)
            rec_stack.add(node)

            for neighbor in self.dependencies.get(node, set()):
                dfs(neighbor, path + [node])

            rec_stack.remove(node)

        for file_path in self.all_files:
            if file_path not in visited:
                dfs(file_path, [])

        return cycles

    def save_report(self, filename="dependency_report.json"):
        """Save the dependency analysis to a JSON file."""
        report_data = self.generate_report()

        # Convert Path objects to strings for JSON serialization
        serializable_data = {}
        for key, value in report_data.items():
            if key == 'modules':
                serializable_data[key] = value
            elif key == 'targets':
                serializable_data[key] = value
            else:
                serializable_data[key] = value

        with open(filename, 'w') as f:
            json.dump(serializable_data, f, indent=2)

        print(f"\nDependency report saved to {filename}")

def main():
    """Run the dependency analysis."""
    analyzer = DependencyAnalyzer()
    analyzer.analyze_dependencies()
    analyzer.save_report()

if __name__ == "__main__":
    main()