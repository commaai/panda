#!/usr/bin/env python3
"""
Module Registry for Panda Modular Build System

This provides a centralized registry for managing modules during the build process.
Each module is self-contained and declares its dependencies explicitly.
"""

import os
from typing import List, Dict
from dataclasses import dataclass, field

@dataclass
class Module:
    """Represents a buildable module in the panda system."""
    name: str
    description: str
    sources: List[str] = field(default_factory=list)
    headers: List[str] = field(default_factory=list)
    includes: List[str] = field(default_factory=list)
    dependencies: List[str] = field(default_factory=list)
    flags: List[str] = field(default_factory=list)
    tests: List[str] = field(default_factory=list)
    built_objects: List = field(default_factory=list)
    directory: str = ""

    def __post_init__(self):
        """Validate module definition."""
        if not self.name:
            raise ValueError("Module name cannot be empty")
        if not self.description:
            raise ValueError(f"Module {self.name} must have a description")

class ModuleRegistry:
    """Central registry for all modules in the build system."""

    def __init__(self):
        self.modules: Dict[str, Module] = {}
        self.build_cache: Dict[str, List] = {}
        self.build_order_cache: Dict[str, List[str]] = {}

    def register_module(self, name: str, description: str, sources: List[str] = None,
                       headers: List[str] = None, includes: List[str] = None,
                       dependencies: List[str] = None, flags: List[str] = None,
                       tests: List[str] = None, directory: str = "") -> Module:
        """Register a new module in the system."""
        if name in self.modules:
            raise ValueError(f"Module {name} already registered")

        module = Module(
            name=name,
            description=description,
            sources=sources or [],
            headers=headers or [],
            includes=includes or [],
            dependencies=dependencies or [],
            flags=flags or [],
            tests=tests or [],
            directory=directory
        )

        self.modules[name] = module
        print(f"Registered module: {name}")
        return module

    def get_module(self, name: str) -> Module:
        """Get a module by name."""
        if name not in self.modules:
            raise ValueError(f"Module {name} not found. Available modules: {list(self.modules.keys())}")
        return self.modules[name]

    def get_build_order(self, target_module: str) -> List[str]:
        """Get the build order for a module and all its dependencies."""
        if target_module in self.build_order_cache:
            return self.build_order_cache[target_module]

        visited = set()
        temp_visited = set()
        result = []

        def visit(module_name: str):
            if module_name in temp_visited:
                raise ValueError(f"Circular dependency detected involving {module_name}")
            if module_name in visited:
                return

            temp_visited.add(module_name)

            if module_name in self.modules:
                module = self.modules[module_name]
                for dep in module.dependencies:
                    visit(dep)

            temp_visited.remove(module_name)
            visited.add(module_name)
            result.append(module_name)

        visit(target_module)
        self.build_order_cache[target_module] = result
        return result

    def get_all_sources(self, target_module: str) -> List[str]:
        """Get all source files needed for a target module."""
        build_order = self.get_build_order(target_module)
        all_sources = []

        for module_name in build_order:
            if module_name in self.modules:
                module = self.modules[module_name]
                # Add sources with directory prefix
                for source in module.sources:
                    if module.directory:
                        full_path = os.path.join(module.directory, source)
                    else:
                        full_path = source
                    all_sources.append(full_path)

        return all_sources

    def get_all_includes(self, target_module: str) -> List[str]:
        """Get all include directories needed for a target module."""
        build_order = self.get_build_order(target_module)
        all_includes = set()

        for module_name in build_order:
            if module_name in self.modules:
                module = self.modules[module_name]
                for include in module.includes:
                    if module.directory and not os.path.isabs(include) and include != '.':
                        full_path = os.path.join(module.directory, include)
                    elif include == '.' and module.directory:
                        full_path = os.path.join(module.directory, include)
                    else:
                        full_path = include
                    all_includes.add(full_path)

        return sorted(list(all_includes))

    def get_all_flags(self, target_module: str) -> List[str]:
        """Get all compiler flags needed for a target module."""
        build_order = self.get_build_order(target_module)
        all_flags = []

        for module_name in build_order:
            if module_name in self.modules:
                module = self.modules[module_name]
                all_flags.extend(module.flags)

        return all_flags

    def print_dependency_graph(self):
        """Print the dependency graph for debugging."""
        print("\nModule Dependency Graph:")
        print("-" * 40)
        for name, module in self.modules.items():
            deps = " -> ".join(module.dependencies) if module.dependencies else "(none)"
            print(f"{name}: {deps}")

    def print_build_order(self, target_module: str):
        """Print the build order for a target module."""
        try:
            build_order = self.get_build_order(target_module)
            print(f"\nBuild order for {target_module}:")
            print(" -> ".join(build_order))
        except Exception as e:
            print(f"Error calculating build order for {target_module}: {e}")

    def validate_all_dependencies(self):
        """Validate that all declared dependencies exist."""
        errors = []
        for name, module in self.modules.items():
            for dep in module.dependencies:
                if dep not in self.modules:
                    errors.append(f"Module {name} depends on non-existent module {dep}")

        if errors:
            raise ValueError("Dependency validation failed:\n" + "\n".join(errors))

        print("✓ All module dependencies validated")

    def get_stats(self) -> Dict:
        """Get statistics about the module system."""
        return {
            'total_modules': len(self.modules),
            'total_sources': sum(len(m.sources) for m in self.modules.values()),
            'total_headers': sum(len(m.headers) for m in self.modules.values()),
            'modules_with_tests': len([m for m in self.modules.values() if m.tests]),
            'modules_with_dependencies': len([m for m in self.modules.values() if m.dependencies]),
        }

# Global module registry instance
module_registry = ModuleRegistry()