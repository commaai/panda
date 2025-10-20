#!/usr/bin/env python3
"""
Practical Dependency Mapping for Panda Modular Refactoring

Based on analysis of the panda codebase, this creates a practical
modular organization that groups related functionality and defines
clear dependency relationships.
"""

import json
from dataclasses import dataclass
from typing import List

@dataclass
class PracticalModule:
    """A practical module definition for the panda codebase."""
    name: str
    description: str
    files: List[str]
    dependencies: List[str]
    includes: List[str]
    flags: List[str]
    target_specific: bool = False

class PandaModuleMapper:
    """Creates a practical module mapping for the panda codebase."""

    def __init__(self):
        self.modules = {}

    def define_modules(self):
        """Define the practical module structure based on codebase analysis."""

        # 1. Hardware Abstraction Layer (HAL)
        self.modules['hal_stm32h7'] = PracticalModule(
            name='hal_stm32h7',
            description='STM32H7 hardware abstraction layer - low-level hardware access',
            files=[
                'board/stm32h7/clock.h',
                'board/stm32h7/peripherals.h',
                'board/stm32h7/lladc.h',
                'board/stm32h7/lladc_declarations.h',
                'board/stm32h7/llfdcan.h',
                'board/stm32h7/llfdcan_declarations.h',
                'board/stm32h7/llflash.h',
                'board/stm32h7/llfan.h',
                'board/stm32h7/lli2c.h',
                'board/stm32h7/llspi.h',
                'board/stm32h7/lluart.h',
                'board/stm32h7/llusb.h',
                'board/stm32h7/llusb_declarations.h',
                'board/stm32h7/interrupt_handlers.h',
                'board/stm32h7/board.h',
                'board/stm32h7/sound.h',
                'board/stm32h7/stm32h7_config.h',
                'board/stm32h7/startup_stm32h7x5xx.s',
            ],
            dependencies=[],
            includes=[
                'board/stm32h7',
                'board/stm32h7/inc',
            ],
            flags=['-DSTM32H7', '-mcpu=cortex-m7']
        )

        # 2. Core System
        self.modules['core_system'] = PracticalModule(
            name='core_system',
            description='Core system initialization and basic utilities',
            files=[
                'board/early_init.h',
                'board/config.h',
                'board/libc.h',
                'board/utils.h',
                'board/crc.h',
                'board/critical.h',
                'board/critical_declarations.h',
                'board/fake_stm.h',  # For testing
            ],
            dependencies=['hal_stm32h7'],
            includes=['board'],
            flags=[]
        )

        # 3. Basic Drivers
        self.modules['drivers_basic'] = PracticalModule(
            name='drivers_basic',
            description='Basic hardware drivers - GPIO, LED, timers, PWM',
            files=[
                'board/drivers/gpio.h',
                'board/drivers/led.h',
                'board/drivers/timers.h',
                'board/drivers/pwm.h',
                'board/drivers/registers.h',
                'board/drivers/registers_declarations.h',
                'board/drivers/interrupts.h',
                'board/drivers/interrupts_declarations.h',
                'board/drivers/clock_source.h',
                'board/drivers/clock_source_declarations.h',
            ],
            dependencies=['core_system'],
            includes=['board/drivers'],
            flags=[]
        )

        # 4. Communication Drivers
        self.modules['drivers_comm'] = PracticalModule(
            name='drivers_comm',
            description='Communication drivers - UART, USB, SPI',
            files=[
                'board/drivers/uart.h',
                'board/drivers/uart_declarations.h',
                'board/drivers/usb.h',
                'board/drivers/usb_declarations.h',
                'board/drivers/spi.h',
                'board/drivers/spi_declarations.h',
            ],
            dependencies=['drivers_basic'],
            includes=['board/drivers'],
            flags=[]
        )

        # 5. CAN System
        self.modules['drivers_can'] = PracticalModule(
            name='drivers_can',
            description='CAN bus drivers and communication',
            files=[
                'board/drivers/fdcan.h',
                'board/drivers/fdcan_declarations.h',
                'board/drivers/can_common.h',
                'board/drivers/can_common_declarations.h',
            ],
            dependencies=['drivers_comm'],
            includes=['board/drivers'],
            flags=[]
        )

        # 6. Monitoring and Control
        self.modules['drivers_monitoring'] = PracticalModule(
            name='drivers_monitoring',
            description='System monitoring and control drivers',
            files=[
                'board/drivers/fan.h',
                'board/drivers/fan_declarations.h',
                'board/drivers/harness.h',
                'board/drivers/harness_declarations.h',
                'board/drivers/simple_watchdog.h',
                'board/drivers/simple_watchdog_declarations.h',
                'board/drivers/bootkick.h',
                'board/drivers/bootkick_declarations.h',
                'board/drivers/fake_siren.h',
            ],
            dependencies=['drivers_basic'],
            includes=['board/drivers'],
            flags=[]
        )

        # 7. Safety System
        self.modules['safety'] = PracticalModule(
            name='safety',
            description='Safety-critical functionality and monitoring',
            files=[
                'board/health.h',
                'board/faults.h',
                'board/faults_declarations.h',
                # Note: opendbc/safety files would be included if available
            ],
            dependencies=['drivers_can', 'drivers_monitoring'],
            includes=['board', 'opendbc/safety'],
            flags=[]
        )

        # 8. Communication Layer
        self.modules['communication'] = PracticalModule(
            name='communication',
            description='High-level communication protocols',
            files=[
                'board/can_comms.h',
                'board/can.h',
                'board/main_comms.h',
                'board/comms_definitions.h',
            ],
            dependencies=['safety'],
            includes=['board'],
            flags=[]
        )

        # 9. Power Management
        self.modules['power_management'] = PracticalModule(
            name='power_management',
            description='Power saving and management functionality',
            files=[
                'board/power_saving.h',
                'board/power_saving_declarations.h',
            ],
            dependencies=['drivers_monitoring'],
            includes=['board'],
            flags=[]
        )

        # 10. Board Support
        self.modules['board_support'] = PracticalModule(
            name='board_support',
            description='Board-specific configurations and support',
            files=[
                'board/boards/board_declarations.h',
                'board/boards/tres.h',
                'board/boards/cuatro.h',
                'board/boards/red.h',
                'board/boards/unused_funcs.h',
            ],
            dependencies=['core_system'],
            includes=['board/boards'],
            flags=[]
        )

        # 11. Cryptography
        self.modules['crypto'] = PracticalModule(
            name='crypto',
            description='Cryptographic functions for secure boot and signing',
            files=[
                'crypto/rsa.h',
                'crypto/rsa.c',
                'crypto/sha.h',
                'crypto/sha.c',
                'crypto/hash-internal.h',
            ],
            dependencies=['core_system'],
            includes=['crypto'],
            flags=[]
        )

        # 12. Bootloader
        self.modules['bootloader'] = PracticalModule(
            name='bootloader',
            description='Bootloader and firmware update functionality',
            files=[
                'board/bootstub.c',
                'board/bootstub_declarations.h',
                'board/flasher.h',
                'board/provision.h',
            ],
            dependencies=['crypto', 'core_system'],
            includes=['board'],
            flags=[]
        )

        # 13. Main Application
        self.modules['panda_main'] = PracticalModule(
            name='panda_main',
            description='Main panda application entry point and logic',
            files=[
                'board/main.c',
                'board/main_declarations.h',
                'board/main_definitions.h',
            ],
            dependencies=['communication', 'power_management', 'board_support'],
            includes=['board'],
            flags=[],
            target_specific=True
        )

        # 14. Jungle Application
        self.modules['jungle_main'] = PracticalModule(
            name='jungle_main',
            description='Jungle-specific application logic',
            files=[
                'board/jungle/main.c',
                'board/jungle/main_comms.h',
                'board/jungle/jungle_health.h',
                'board/jungle/boards/board_declarations.h',
                'board/jungle/boards/board_v2.h',
                'board/jungle/stm32h7/board.h',
            ],
            dependencies=['communication', 'power_management'],
            includes=['board/jungle'],
            flags=['-DPANDA_JUNGLE'],
            target_specific=True
        )

    def get_build_order(self, target_module: str) -> List[str]:
        """Get the build order for a target module."""
        visited = set()
        result = []

        def _visit(module_name):
            if module_name in visited:
                return
            visited.add(module_name)

            module = self.modules.get(module_name)
            if not module:
                return

            # Visit dependencies first
            for dep in module.dependencies:
                _visit(dep)

            # Add this module
            result.append(module_name)

        _visit(target_module)
        return result

    def generate_target_configs(self):
        """Generate target-specific build configurations."""
        targets = {}

        # Panda H7 target
        panda_build_order = self.get_build_order('panda_main')
        targets['panda_h7'] = {
            'description': 'Main panda firmware for STM32H7',
            'main_module': 'panda_main',
            'build_order': panda_build_order,
            'required_modules': panda_build_order,
            'platform_flags': ['-DSTM32H7', '-DSTM32H725xx'],
            'linker_script': 'board/stm32h7/stm32h7x5_flash.ld',
            'startup_file': 'board/stm32h7/startup_stm32h7x5xx.s',
        }

        # Jungle H7 target
        jungle_build_order = self.get_build_order('jungle_main')
        targets['panda_jungle_h7'] = {
            'description': 'Jungle firmware for STM32H7',
            'main_module': 'jungle_main',
            'build_order': jungle_build_order,
            'required_modules': jungle_build_order,
            'platform_flags': ['-DSTM32H7', '-DSTM32H725xx', '-DPANDA_JUNGLE'],
            'linker_script': 'board/stm32h7/stm32h7x5_flash.ld',
            'startup_file': 'board/stm32h7/startup_stm32h7x5xx.s',
        }

        return targets

    def analyze_module_stats(self):
        """Analyze statistics about the modular organization."""
        stats = {
            'total_modules': len(self.modules),
            'total_files': sum(len(m.files) for m in self.modules.values()),
            'target_specific_modules': len([m for m in self.modules.values() if m.target_specific]),
            'shared_modules': len([m for m in self.modules.values() if not m.target_specific]),
            'max_dependency_depth': 0,
            'dependency_graph': {}
        }

        # Calculate dependency depths
        for module_name in self.modules:
            build_order = self.get_build_order(module_name)
            depth = len(build_order)
            stats['max_dependency_depth'] = max(stats['max_dependency_depth'], depth)
            stats['dependency_graph'][module_name] = {
                'dependencies': self.modules[module_name].dependencies,
                'depth': depth
            }

        return stats

    def generate_report(self):
        """Generate a comprehensive modular organization report."""
        print("="*70)
        print("PANDA PRACTICAL MODULAR ORGANIZATION")
        print("="*70)

        stats = self.analyze_module_stats()

        print("\nOVERVIEW:")
        print(f"  Total modules: {stats['total_modules']}")
        print(f"  Total files: {stats['total_files']}")
        print(f"  Shared modules: {stats['shared_modules']}")
        print(f"  Target-specific modules: {stats['target_specific_modules']}")
        print(f"  Maximum dependency depth: {stats['max_dependency_depth']}")

        print("\nMODULE DEFINITIONS:")
        for name, module in self.modules.items():
            print(f"\n  {name}:")
            print(f"    Description: {module.description}")
            print(f"    Files: {len(module.files)}")
            print(f"    Dependencies: {module.dependencies}")
            print(f"    Target-specific: {module.target_specific}")
            if module.flags:
                print(f"    Flags: {module.flags}")

        targets = self.generate_target_configs()
        print("\nTARGET CONFIGURATIONS:")
        for name, config in targets.items():
            print(f"\n  {name}:")
            print(f"    Description: {config['description']}")
            print(f"    Build order: {' -> '.join(config['build_order'])}")
            print(f"    Required modules: {len(config['required_modules'])}")
            print(f"    Platform flags: {config['platform_flags']}")

        return {
            'modules': {name: {
                'description': mod.description,
                'files': mod.files,
                'dependencies': mod.dependencies,
                'includes': mod.includes,
                'flags': mod.flags,
                'target_specific': mod.target_specific
            } for name, mod in self.modules.items()},
            'targets': targets,
            'statistics': stats
        }

    def save_report(self, filename="practical_dependency_mapping.json"):
        """Save the practical mapping to a JSON file."""
        report_data = self.generate_report()

        with open(filename, 'w') as f:
            json.dump(report_data, f, indent=2)

        print(f"\nPractical dependency mapping saved to {filename}")

def main():
    """Generate the practical dependency mapping."""
    mapper = PandaModuleMapper()
    mapper.define_modules()
    mapper.save_report()

if __name__ == "__main__":
    main()