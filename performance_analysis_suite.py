#!/usr/bin/env python3
"""
Performance Analysis Suite for Panda Modular Build System

This comprehensive suite analyzes build performance, identifies bottlenecks,
tracks compilation dependencies, and generates detailed performance reports
for both legacy and modular build systems.

Features:
1. Build time analysis and profiling
2. Compilation dependency tracking
3. Parallelization efficiency analysis
4. Memory usage monitoring
5. I/O bottleneck detection
6. Incremental build performance
7. Historical trend analysis
8. Optimization recommendations

Usage:
    python performance_analysis_suite.py --profile-builds
    python performance_analysis_suite.py --analyze-dependencies
    python performance_analysis_suite.py --track-trends
    python performance_analysis_suite.py --full-analysis
"""

import os
import sys
import time
import json
import psutil
import subprocess
import threading
import queue
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple
from dataclasses import dataclass, field, asdict
from concurrent.futures import ThreadPoolExecutor
import statistics
import re
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime, timedelta


@dataclass
class BuildProfile:
    """Profile data from a single build."""
    target: str
    system: str  # 'legacy' or 'modular'
    total_time: float
    compilation_time: float
    linking_time: float
    cpu_usage: List[float] = field(default_factory=list)
    memory_usage: List[float] = field(default_factory=list)  # MB
    io_reads: int = 0
    io_writes: int = 0
    parallel_efficiency: float = 0.0
    bottlenecks: List[str] = field(default_factory=list)
    file_compilation_times: Dict[str, float] = field(default_factory=dict)


@dataclass
class DependencyAnalysis:
    """Dependency analysis results."""
    target: str
    total_files: int
    dependency_chains: Dict[str, List[str]] = field(default_factory=dict)
    critical_path: List[str] = field(default_factory=list)
    critical_path_time: float = 0.0
    parallelizable_groups: List[List[str]] = field(default_factory=list)
    dependency_depth: Dict[str, int] = field(default_factory=dict)


@dataclass
class PerformanceReport:
    """Comprehensive performance report."""
    timestamp: float
    build_profiles: List[BuildProfile] = field(default_factory=list)
    dependency_analysis: Optional[DependencyAnalysis] = None
    comparison_summary: Dict[str, Any] = field(default_factory=dict)
    optimization_recommendations: List[str] = field(default_factory=list)
    trend_analysis: Dict[str, Any] = field(default_factory=dict)


class SystemMonitor:
    """Monitor system resources during builds."""
    
    def __init__(self, interval: float = 0.5):
        self.interval = interval
        self.monitoring = False
        self.data_queue = queue.Queue()
        self.monitor_thread = None
    
    def start_monitoring(self):
        """Start monitoring system resources."""
        self.monitoring = True
        self.monitor_thread = threading.Thread(target=self._monitor_loop)
        self.monitor_thread.daemon = True
        self.monitor_thread.start()
    
    def stop_monitoring(self) -> Dict[str, List[float]]:
        """Stop monitoring and return collected data."""
        self.monitoring = False
        if self.monitor_thread:
            self.monitor_thread.join(timeout=2.0)
        
        # Collect all data from queue
        cpu_usage = []
        memory_usage = []
        io_reads = []
        io_writes = []
        
        while not self.data_queue.empty():
            try:
                data = self.data_queue.get_nowait()
                cpu_usage.append(data['cpu'])
                memory_usage.append(data['memory'])
                io_reads.append(data['io_read'])
                io_writes.append(data['io_write'])
            except queue.Empty:
                break
        
        return {
            'cpu_usage': cpu_usage,
            'memory_usage': memory_usage,
            'io_reads': io_reads,
            'io_writes': io_writes
        }
    
    def _monitor_loop(self):
        """Main monitoring loop."""
        process = psutil.Process()
        last_io = process.io_counters()
        
        while self.monitoring:
            try:
                # CPU and memory
                cpu_percent = process.cpu_percent()
                memory_mb = process.memory_info().rss / (1024 * 1024)
                
                # I/O counters
                current_io = process.io_counters()
                io_read = current_io.read_bytes - last_io.read_bytes
                io_write = current_io.write_bytes - last_io.write_bytes
                last_io = current_io
                
                self.data_queue.put({
                    'timestamp': time.time(),
                    'cpu': cpu_percent,
                    'memory': memory_mb,
                    'io_read': io_read,
                    'io_write': io_write
                })
                
                time.sleep(self.interval)
                
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                break
            except Exception as e:
                print(f"Monitoring error: {e}")
                break


class CompilationProfiler:
    """Profile individual compilation steps."""
    
    def __init__(self, panda_root: str):
        self.panda_root = Path(panda_root)
        self.compilation_times = {}
        
    def profile_scons_build(self, target: str, system: str) -> BuildProfile:
        """Profile a SCons build with detailed timing."""
        print(f"Profiling {system} build for {target}...")
        
        # Clean build
        self._clean_build(system)
        
        # Setup monitoring
        monitor = SystemMonitor()
        
        # Prepare build command
        cmd = self._get_build_command(target, system)
        
        # Start monitoring and build
        monitor.start_monitoring()
        start_time = time.time()
        
        try:
            # Run build with timing information
            result = subprocess.run(
                cmd,
                cwd=self.panda_root,
                capture_output=True,
                text=True,
                timeout=600  # 10 minute timeout
            )
            
            total_time = time.time() - start_time
            
            # Stop monitoring
            system_data = monitor.stop_monitoring()
            
            # Parse build output for compilation times
            file_times = self._parse_compilation_times(result.stdout, result.stderr)
            
            # Analyze phases
            compilation_time, linking_time = self._analyze_build_phases(result.stdout, total_time)
            
            # Calculate parallel efficiency
            parallel_efficiency = self._calculate_parallel_efficiency(system_data['cpu_usage'])
            
            # Identify bottlenecks
            bottlenecks = self._identify_bottlenecks(file_times, system_data)
            
            profile = BuildProfile(
                target=target,
                system=system,
                total_time=total_time,
                compilation_time=compilation_time,
                linking_time=linking_time,
                cpu_usage=system_data['cpu_usage'],
                memory_usage=system_data['memory_usage'],
                io_reads=sum(system_data['io_reads']) if system_data['io_reads'] else 0,
                io_writes=sum(system_data['io_writes']) if system_data['io_writes'] else 0,
                parallel_efficiency=parallel_efficiency,
                bottlenecks=bottlenecks,
                file_compilation_times=file_times
            )
            
            print(f"✓ {system} build profiled: {total_time:.2f}s total")
            return profile
            
        except subprocess.TimeoutExpired:
            monitor.stop_monitoring()
            raise Exception(f"Build timed out after 10 minutes")
        except Exception as e:
            monitor.stop_monitoring()
            raise Exception(f"Build failed: {str(e)}")
    
    def _clean_build(self, system: str):
        """Clean build artifacts."""
        if system == 'legacy':
            subprocess.run(['scons', '-c'], cwd=self.panda_root, capture_output=True)
        else:
            subprocess.run(['scons', '-c', '-f', 'SConscript.modular'], 
                         cwd=self.panda_root, capture_output=True)
    
    def _get_build_command(self, target: str, system: str) -> List[str]:
        """Get the build command for a target and system."""
        if system == 'legacy':
            return ['scons', '-j4', '--debug=time', target]
        else:
            return ['scons', '-j4', '--debug=time', '-f', 'SConscript.modular', f'{target}_modular']
    
    def _parse_compilation_times(self, stdout: str, stderr: str) -> Dict[str, float]:
        """Parse compilation times from SCons debug output."""
        times = {}
        
        # Look for SCons timing information
        time_pattern = r'Command execution time: ([\d.]+): (.+)'
        
        for line in stdout.split('\n') + stderr.split('\n'):
            match = re.search(time_pattern, line)
            if match:
                exec_time = float(match.group(1))
                command = match.group(2)
                
                # Extract source file from command
                if '.c' in command or '.cpp' in command or '.s' in command:
                    # Find source file in command
                    parts = command.split()
                    for part in parts:
                        if part.endswith(('.c', '.cpp', '.s')) and not part.startswith('-'):
                            times[part] = exec_time
                            break
        
        return times
    
    def _analyze_build_phases(self, output: str, total_time: float) -> Tuple[float, float]:
        """Analyze compilation vs linking phases."""
        # Simple heuristic: assume last 10% is linking
        linking_time = total_time * 0.1
        compilation_time = total_time - linking_time
        
        # Try to detect actual linking phase from output
        linking_patterns = [
            r'Linking.*\.elf',
            r'arm-none-eabi-ld',
            r'Creating library'
        ]
        
        for pattern in linking_patterns:
            if re.search(pattern, output, re.IGNORECASE):
                # Rough estimate based on typical ratios
                linking_time = total_time * 0.15
                compilation_time = total_time * 0.85
                break
        
        return compilation_time, linking_time
    
    def _calculate_parallel_efficiency(self, cpu_usage: List[float]) -> float:
        """Calculate parallel build efficiency."""
        if not cpu_usage:
            return 0.0
        
        # Efficiency is how well we're using available cores
        # 100% CPU usage on 4-core system = 400% in psutil
        max_possible = psutil.cpu_count() * 100
        avg_usage = statistics.mean(cpu_usage)
        
        return min(avg_usage / max_possible, 1.0) if max_possible > 0 else 0.0
    
    def _identify_bottlenecks(self, file_times: Dict[str, float], 
                             system_data: Dict[str, List[float]]) -> List[str]:
        """Identify performance bottlenecks."""
        bottlenecks = []
        
        # File compilation bottlenecks
        if file_times:
            sorted_files = sorted(file_times.items(), key=lambda x: x[1], reverse=True)
            total_compile_time = sum(file_times.values())
            
            # Files taking >10% of total compilation time
            for filename, file_time in sorted_files[:3]:
                if file_time > total_compile_time * 0.1:
                    bottlenecks.append(f"Slow compilation: {filename} ({file_time:.2f}s)")
        
        # Memory bottlenecks
        if system_data['memory_usage']:
            max_memory = max(system_data['memory_usage'])
            if max_memory > 1000:  # >1GB
                bottlenecks.append(f"High memory usage: {max_memory:.0f}MB")
        
        # I/O bottlenecks
        if system_data['io_reads'] and system_data['io_writes']:
            total_io = sum(system_data['io_reads']) + sum(system_data['io_writes'])
            if total_io > 100 * 1024 * 1024:  # >100MB
                bottlenecks.append(f"High I/O activity: {total_io / (1024*1024):.0f}MB")
        
        return bottlenecks


class DependencyAnalyzer:
    """Analyze compilation dependencies for optimization opportunities."""
    
    def __init__(self, panda_root: str):
        self.panda_root = Path(panda_root)
    
    def analyze_dependencies(self, target: str) -> DependencyAnalysis:
        """Analyze dependency graph for a target."""
        print(f"Analyzing dependencies for {target}...")
        
        # Get dependency information from SCons
        dependency_data = self._extract_dependencies(target)
        
        # Build dependency graph
        dep_graph = self._build_dependency_graph(dependency_data)
        
        # Find critical path
        critical_path, critical_time = self._find_critical_path(dep_graph)
        
        # Identify parallelizable groups
        parallel_groups = self._find_parallel_groups(dep_graph)
        
        # Calculate dependency depths
        dep_depths = self._calculate_dependency_depths(dep_graph)
        
        analysis = DependencyAnalysis(
            target=target,
            total_files=len(dep_graph),
            dependency_chains=dep_graph,
            critical_path=critical_path,
            critical_path_time=critical_time,
            parallelizable_groups=parallel_groups,
            dependency_depth=dep_depths
        )
        
        print(f"✓ Dependency analysis complete: {len(dep_graph)} files")
        return analysis
    
    def _extract_dependencies(self, target: str) -> Dict[str, List[str]]:
        """Extract dependency information using SCons."""
        # Use SCons dry-run to get dependency information
        cmd = ['scons', '--dry-run', '--debug=tree', target]
        
        try:
            result = subprocess.run(
                cmd,
                cwd=self.panda_root,
                capture_output=True,
                text=True,
                timeout=60
            )
            
            return self._parse_scons_tree(result.stdout)
            
        except Exception as e:
            print(f"Warning: Could not extract dependencies: {e}")
            return {}
    
    def _parse_scons_tree(self, output: str) -> Dict[str, List[str]]:
        """Parse SCons dependency tree output."""
        dependencies = {}
        current_target = None
        
        for line in output.split('\n'):
            line = line.strip()
            
            # Look for target files
            if line.endswith(('.o', '.elf', '.bin')):
                current_target = line
                dependencies[current_target] = []
            
            # Look for dependencies (indented)
            elif line.startswith(' ') and current_target:
                dep = line.strip()
                if dep.endswith(('.c', '.cpp', '.h', '.s')):
                    dependencies[current_target].append(dep)
        
        return dependencies
    
    def _build_dependency_graph(self, dep_data: Dict[str, List[str]]) -> Dict[str, List[str]]:
        """Build a simplified dependency graph."""
        graph = {}
        
        # Flatten to source file dependencies
        for target, deps in dep_data.items():
            for dep in deps:
                if dep.endswith(('.c', '.cpp', '.s')):  # Source files
                    if dep not in graph:
                        graph[dep] = []
                    
                    # Find header dependencies for this source
                    for other_dep in deps:
                        if other_dep.endswith('.h') and other_dep != dep:
                            graph[dep].append(other_dep)
        
        return graph
    
    def _find_critical_path(self, graph: Dict[str, List[str]]) -> Tuple[List[str], float]:
        """Find the critical path through the dependency graph."""
        # Simplified critical path - file with most dependencies
        max_deps = 0
        critical_file = None
        
        for file, deps in graph.items():
            if len(deps) > max_deps:
                max_deps = len(deps)
                critical_file = file
        
        if critical_file:
            path = [critical_file] + graph[critical_file]
            # Estimate time (rough heuristic)
            estimated_time = len(path) * 2.0  # 2 seconds per dependency
            return path, estimated_time
        
        return [], 0.0
    
    def _find_parallel_groups(self, graph: Dict[str, List[str]]) -> List[List[str]]:
        """Find groups of files that can be compiled in parallel."""
        # Files with no dependencies can be compiled in parallel
        independent_files = [file for file, deps in graph.items() if not deps]
        
        # Group by estimated compilation time (for load balancing)
        groups = []
        group_size = 4  # Target group size
        
        for i in range(0, len(independent_files), group_size):
            group = independent_files[i:i + group_size]
            if group:
                groups.append(group)
        
        return groups
    
    def _calculate_dependency_depths(self, graph: Dict[str, List[str]]) -> Dict[str, int]:
        """Calculate dependency depth for each file."""
        depths = {}
        
        def calculate_depth(file, visited=None):
            if visited is None:
                visited = set()
            
            if file in visited:
                return 0  # Circular dependency
            
            if file in depths:
                return depths[file]
            
            visited.add(file)
            
            if file not in graph or not graph[file]:
                depth = 0
            else:
                depth = 1 + max(calculate_depth(dep, visited.copy()) for dep in graph[file])
            
            depths[file] = depth
            return depth
        
        for file in graph:
            calculate_depth(file)
        
        return depths


class PerformanceAnalyzer:
    """Main performance analysis coordinator."""
    
    def __init__(self, panda_root: str = None):
        self.panda_root = Path(panda_root or os.getcwd())
        self.results_dir = self.panda_root / 'performance_analysis_results'
        self.results_dir.mkdir(exist_ok=True)
        
        self.profiler = CompilationProfiler(self.panda_root)
        self.dependency_analyzer = DependencyAnalyzer(self.panda_root)
        
        # Load historical data
        self.history_file = self.results_dir / 'performance_history.json'
        self.performance_history = self._load_history()
    
    def _load_history(self) -> List[Dict[str, Any]]:
        """Load performance history from file."""
        if self.history_file.exists():
            try:
                with open(self.history_file, 'r') as f:
                    return json.load(f)
            except Exception as e:
                print(f"Warning: Could not load performance history: {e}")
        return []
    
    def _save_history(self):
        """Save performance history to file."""
        with open(self.history_file, 'w') as f:
            json.dump(self.performance_history, f, indent=2)
    
    def run_full_analysis(self, targets: List[str] = None) -> PerformanceReport:
        """Run comprehensive performance analysis."""
        print("Starting comprehensive performance analysis...")
        
        if targets is None:
            targets = ['panda_h7']
        
        report = PerformanceReport(timestamp=time.time())
        
        # Profile builds for each target
        for target in targets:
            print(f"\nAnalyzing target: {target}")
            
            try:
                # Profile both systems
                legacy_profile = self.profiler.profile_scons_build(target, 'legacy')
                modular_profile = self.profiler.profile_scons_build(target, 'modular')
                
                report.build_profiles.extend([legacy_profile, modular_profile])
                
                # Analyze dependencies
                dep_analysis = self.dependency_analyzer.analyze_dependencies(target)
                report.dependency_analysis = dep_analysis
                
            except Exception as e:
                print(f"Error analyzing {target}: {e}")
        
        # Generate comparison summary
        report.comparison_summary = self._generate_comparison_summary(report.build_profiles)
        
        # Generate optimization recommendations
        report.optimization_recommendations = self._generate_recommendations(report)
        
        # Analyze trends
        report.trend_analysis = self._analyze_trends(report)
        
        # Save report
        self._save_report(report)
        
        # Update history
        self._update_history(report)
        
        # Generate visualizations
        self._generate_visualizations(report)
        
        # Print summary
        self._print_analysis_summary(report)
        
        return report
    
    def _generate_comparison_summary(self, profiles: List[BuildProfile]) -> Dict[str, Any]:
        """Generate summary comparing legacy and modular builds."""
        summary = {
            'targets_analyzed': set(p.target for p in profiles),
            'system_comparisons': {}
        }
        
        # Group by target
        by_target = {}
        for profile in profiles:
            if profile.target not in by_target:
                by_target[profile.target] = {}
            by_target[profile.target][profile.system] = profile
        
        # Compare each target
        for target, systems in by_target.items():
            if 'legacy' in systems and 'modular' in systems:
                legacy = systems['legacy']
                modular = systems['modular']
                
                comparison = {
                    'build_time_ratio': modular.total_time / legacy.total_time,
                    'compilation_time_ratio': modular.compilation_time / legacy.compilation_time,
                    'memory_usage_ratio': (
                        statistics.mean(modular.memory_usage) / statistics.mean(legacy.memory_usage)
                        if legacy.memory_usage and modular.memory_usage else 1.0
                    ),
                    'parallel_efficiency_delta': modular.parallel_efficiency - legacy.parallel_efficiency,
                    'bottleneck_comparison': {
                        'legacy_bottlenecks': len(legacy.bottlenecks),
                        'modular_bottlenecks': len(modular.bottlenecks)
                    }
                }
                
                summary['system_comparisons'][target] = comparison
        
        return summary
    
    def _generate_recommendations(self, report: PerformanceReport) -> List[str]:
        """Generate optimization recommendations."""
        recommendations = []
        
        # Analyze build profiles
        for profile in report.build_profiles:
            # Slow compilation files
            if profile.file_compilation_times:
                slow_files = [
                    (f, t) for f, t in profile.file_compilation_times.items() 
                    if t > 5.0  # Files taking >5 seconds
                ]
                if slow_files:
                    recommendations.append(
                        f"Optimize slow compilation in {profile.system}: "
                        f"{', '.join(f for f, t in slow_files[:3])}"
                    )
            
            # Parallel efficiency
            if profile.parallel_efficiency < 0.5:
                recommendations.append(
                    f"Improve parallelization in {profile.system} build "
                    f"(efficiency: {profile.parallel_efficiency:.1%})"
                )
            
            # Memory usage
            if profile.memory_usage and max(profile.memory_usage) > 2000:  # >2GB
                recommendations.append(
                    f"Reduce memory usage in {profile.system} build "
                    f"(peak: {max(profile.memory_usage):.0f}MB)"
                )
        
        # Dependency analysis recommendations
        if report.dependency_analysis:
            dep = report.dependency_analysis
            
            if dep.critical_path_time > 30:  # >30 seconds on critical path
                recommendations.append(
                    f"Optimize critical path dependencies "
                    f"(estimated: {dep.critical_path_time:.1f}s)"
                )
            
            if len(dep.parallelizable_groups) < 2:
                recommendations.append(
                    "Increase parallelization opportunities by reducing dependencies"
                )
        
        # System comparison recommendations
        for target, comparison in report.comparison_summary.get('system_comparisons', {}).items():
            if comparison['build_time_ratio'] > 1.2:  # Modular 20% slower
                recommendations.append(
                    f"Modular build for {target} is significantly slower "
                    f"({comparison['build_time_ratio']:.1f}x) - investigate bottlenecks"
                )
            
            if comparison['memory_usage_ratio'] > 1.5:  # 50% more memory
                recommendations.append(
                    f"Modular build for {target} uses significantly more memory "
                    f"({comparison['memory_usage_ratio']:.1f}x)"
                )
        
        return recommendations
    
    def _analyze_trends(self, report: PerformanceReport) -> Dict[str, Any]:
        """Analyze performance trends over time."""
        trends = {
            'build_time_trend': 'stable',
            'memory_trend': 'stable',
            'recent_regressions': []
        }
        
        if len(self.performance_history) < 3:
            trends['insufficient_data'] = True
            return trends
        
        # Analyze recent history (last 10 runs)
        recent_history = self.performance_history[-10:]
        
        # Build time trend
        build_times = [entry.get('avg_build_time', 0) for entry in recent_history]
        if len(build_times) >= 3:
            # Simple trend detection
            if build_times[-1] > build_times[0] * 1.2:
                trends['build_time_trend'] = 'increasing'
            elif build_times[-1] < build_times[0] * 0.8:
                trends['build_time_trend'] = 'decreasing'
        
        # Memory usage trend
        memory_usage = [entry.get('avg_memory_usage', 0) for entry in recent_history]
        if len(memory_usage) >= 3:
            if memory_usage[-1] > memory_usage[0] * 1.3:
                trends['memory_trend'] = 'increasing'
            elif memory_usage[-1] < memory_usage[0] * 0.7:
                trends['memory_trend'] = 'decreasing'
        
        return trends
    
    def _update_history(self, report: PerformanceReport):
        """Update performance history with new data."""
        # Calculate averages for this run
        avg_build_time = statistics.mean(p.total_time for p in report.build_profiles)
        avg_memory = statistics.mean(
            statistics.mean(p.memory_usage) if p.memory_usage else 0 
            for p in report.build_profiles
        )
        
        entry = {
            'timestamp': report.timestamp,
            'avg_build_time': avg_build_time,
            'avg_memory_usage': avg_memory,
            'targets': list(report.comparison_summary.get('targets_analyzed', [])),
            'recommendation_count': len(report.optimization_recommendations)
        }
        
        self.performance_history.append(entry)
        
        # Keep only last 50 entries
        self.performance_history = self.performance_history[-50:]
        
        self._save_history()
    
    def _generate_visualizations(self, report: PerformanceReport):
        """Generate performance visualization charts."""
        try:
            import matplotlib.pyplot as plt
            
            # Build time comparison chart
            self._plot_build_time_comparison(report)
            
            # Performance trend chart
            self._plot_performance_trends()
            
            # Memory usage chart
            self._plot_memory_usage(report)
            
            print(f"✓ Performance charts saved to {self.results_dir}")
            
        except ImportError:
            print("Warning: matplotlib not available - skipping visualizations")
        except Exception as e:
            print(f"Warning: Could not generate visualizations: {e}")
    
    def _plot_build_time_comparison(self, report: PerformanceReport):
        """Plot build time comparison between systems."""
        plt.figure(figsize=(10, 6))
        
        # Group profiles by target and system
        targets = set(p.target for p in report.build_profiles)
        systems = ['legacy', 'modular']
        
        x_pos = range(len(targets))
        width = 0.35
        
        legacy_times = []
        modular_times = []
        
        for target in targets:
            legacy_time = next((p.total_time for p in report.build_profiles 
                              if p.target == target and p.system == 'legacy'), 0)
            modular_time = next((p.total_time for p in report.build_profiles 
                               if p.target == target and p.system == 'modular'), 0)
            
            legacy_times.append(legacy_time)
            modular_times.append(modular_time)
        
        plt.bar([x - width/2 for x in x_pos], legacy_times, width, label='Legacy', alpha=0.8)
        plt.bar([x + width/2 for x in x_pos], modular_times, width, label='Modular', alpha=0.8)
        
        plt.xlabel('Targets')
        plt.ylabel('Build Time (seconds)')
        plt.title('Build Time Comparison: Legacy vs Modular')
        plt.xticks(x_pos, targets)
        plt.legend()
        plt.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(self.results_dir / 'build_time_comparison.png')
        plt.close()
    
    def _plot_performance_trends(self):
        """Plot performance trends over time."""
        if len(self.performance_history) < 3:
            return
        
        plt.figure(figsize=(12, 8))
        
        timestamps = [datetime.fromtimestamp(entry['timestamp']) for entry in self.performance_history]
        build_times = [entry['avg_build_time'] for entry in self.performance_history]
        memory_usage = [entry['avg_memory_usage'] for entry in self.performance_history]
        
        # Subplot 1: Build times
        plt.subplot(2, 1, 1)
        plt.plot(timestamps, build_times, marker='o', linewidth=2)
        plt.title('Build Time Trend')
        plt.ylabel('Average Build Time (s)')
        plt.grid(True, alpha=0.3)
        
        # Subplot 2: Memory usage
        plt.subplot(2, 1, 2)
        plt.plot(timestamps, memory_usage, marker='s', color='red', linewidth=2)
        plt.title('Memory Usage Trend')
        plt.ylabel('Average Memory Usage (MB)')
        plt.xlabel('Date')
        plt.grid(True, alpha=0.3)
        
        # Format x-axis
        plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%m/%d'))
        plt.gca().xaxis.set_major_locator(mdates.DayLocator(interval=1))
        plt.xticks(rotation=45)
        
        plt.tight_layout()
        plt.savefig(self.results_dir / 'performance_trends.png')
        plt.close()
    
    def _plot_memory_usage(self, report: PerformanceReport):
        """Plot memory usage during builds."""
        plt.figure(figsize=(10, 6))
        
        for i, profile in enumerate(report.build_profiles):
            if profile.memory_usage:
                time_points = range(len(profile.memory_usage))
                plt.plot(time_points, profile.memory_usage, 
                        label=f'{profile.target} ({profile.system})',
                        alpha=0.7)
        
        plt.xlabel('Time (0.5s intervals)')
        plt.ylabel('Memory Usage (MB)')
        plt.title('Memory Usage During Builds')
        plt.legend()
        plt.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(self.results_dir / 'memory_usage.png')
        plt.close()
    
    def _save_report(self, report: PerformanceReport):
        """Save the performance report to file."""
        report_file = self.results_dir / f'performance_report_{int(report.timestamp)}.json'
        
        # Convert to dictionary for JSON serialization
        report_dict = asdict(report)
        
        with open(report_file, 'w') as f:
            json.dump(report_dict, f, indent=2)
        
        # Save as latest report
        latest_file = self.results_dir / 'latest_performance_report.json'
        with open(latest_file, 'w') as f:
            json.dump(report_dict, f, indent=2)
        
        print(f"✓ Performance report saved to: {report_file}")
    
    def _print_analysis_summary(self, report: PerformanceReport):
        """Print human-readable analysis summary."""
        print(f"\n{'='*80}")
        print("PERFORMANCE ANALYSIS SUMMARY")
        print(f"{'='*80}")
        
        # Build time summary
        print(f"\nBUILD PERFORMANCE:")
        for profile in report.build_profiles:
            print(f"  {profile.target} ({profile.system}):")
            print(f"    Total time: {profile.total_time:.2f}s")
            print(f"    Compilation: {profile.compilation_time:.2f}s")
            print(f"    Linking: {profile.linking_time:.2f}s")
            print(f"    Parallel efficiency: {profile.parallel_efficiency:.1%}")
            if profile.memory_usage:
                print(f"    Peak memory: {max(profile.memory_usage):.0f}MB")
        
        # System comparisons
        print(f"\nSYSTEM COMPARISONS:")
        for target, comparison in report.comparison_summary.get('system_comparisons', {}).items():
            ratio = comparison['build_time_ratio']
            if ratio > 1:
                print(f"  {target}: Modular {ratio:.1f}x slower")
            else:
                print(f"  {target}: Modular {1/ratio:.1f}x faster")
        
        # Bottlenecks
        print(f"\nBOTTLENECKS IDENTIFIED:")
        for profile in report.build_profiles:
            if profile.bottlenecks:
                print(f"  {profile.target} ({profile.system}):")
                for bottleneck in profile.bottlenecks:
                    print(f"    - {bottleneck}")
        
        # Recommendations
        print(f"\nOPTIMIZATION RECOMMENDATIONS:")
        for i, rec in enumerate(report.optimization_recommendations, 1):
            print(f"  {i}. {rec}")
        
        # Trends
        trends = report.trend_analysis
        if not trends.get('insufficient_data'):
            print(f"\nPERFORMANCE TRENDS:")
            print(f"  Build time: {trends['build_time_trend']}")
            print(f"  Memory usage: {trends['memory_trend']}")
        
        print(f"\nDetailed report: {self.results_dir / 'latest_performance_report.json'}")


def main():
    """Main entry point for performance analysis."""
    import argparse
    
    parser = argparse.ArgumentParser(description='Panda Performance Analysis Suite')
    parser.add_argument('--targets', nargs='+', default=['panda_h7'],
                       help='Targets to analyze')
    parser.add_argument('--profile-builds', action='store_true',
                       help='Profile build performance')
    parser.add_argument('--analyze-dependencies', action='store_true',
                       help='Analyze dependency structure')
    parser.add_argument('--track-trends', action='store_true',
                       help='Track performance trends')
    parser.add_argument('--full-analysis', action='store_true',
                       help='Run complete analysis')
    
    args = parser.parse_args()
    
    analyzer = PerformanceAnalyzer()
    
    if args.full_analysis or not any([args.profile_builds, args.analyze_dependencies, args.track_trends]):
        # Run full analysis
        report = analyzer.run_full_analysis(args.targets)
    else:
        # Run specific analyses
        report = PerformanceReport(timestamp=time.time())
        
        if args.profile_builds:
            for target in args.targets:
                legacy_profile = analyzer.profiler.profile_scons_build(target, 'legacy')
                modular_profile = analyzer.profiler.profile_scons_build(target, 'modular')
                report.build_profiles.extend([legacy_profile, modular_profile])
        
        if args.analyze_dependencies:
            for target in args.targets:
                dep_analysis = analyzer.dependency_analyzer.analyze_dependencies(target)
                report.dependency_analysis = dep_analysis
        
        if args.track_trends:
            report.trend_analysis = analyzer._analyze_trends(report)
        
        # Generate partial report
        analyzer._save_report(report)
    
    return 0


if __name__ == "__main__":
    sys.exit(main())