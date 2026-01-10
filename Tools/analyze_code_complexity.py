#!/usr/bin/env python3
"""
Code Complexity and Maintainability Analyzer for C/C++ Codebases

This script analyzes C/C++ source files and computes various metrics:
- Cyclomatic Complexity (independent paths through code)
- Cognitive Complexity (human effort to understand)
- Halstead Metrics (operators/operands analysis)
- Lines of Code (LOC, SLOC, comment lines, blank lines)
- Maintainability Index

Results are visualized by directory and source file.
"""

import os
import re
import math
import argparse
from pathlib import Path
from dataclasses import dataclass, field
from typing import Dict, List, Tuple, Set
from collections import defaultdict

# Try to import visualization libraries
try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    from matplotlib.colors import LinearSegmentedColormap
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not found. Install with 'pip install matplotlib numpy' for visualizations.")


@dataclass
class HalsteadMetrics:
    """Halstead complexity metrics"""
    n1: int = 0  # Number of distinct operators
    n2: int = 0  # Number of distinct operands
    N1: int = 0  # Total number of operators
    N2: int = 0  # Total number of operands
    
    @property
    def vocabulary(self) -> int:
        """Program vocabulary (n = n1 + n2)"""
        return self.n1 + self.n2
    
    @property
    def length(self) -> int:
        """Program length (N = N1 + N2)"""
        return self.N1 + self.N2
    
    @property
    def calculated_length(self) -> float:
        """Calculated program length"""
        if self.n1 == 0 or self.n2 == 0:
            return 0
        return self.n1 * math.log2(self.n1) + self.n2 * math.log2(self.n2)
    
    @property
    def volume(self) -> float:
        """Program volume (V = N * log2(n))"""
        if self.vocabulary == 0:
            return 0
        return self.length * math.log2(self.vocabulary)
    
    @property
    def difficulty(self) -> float:
        """Program difficulty (D = (n1/2) * (N2/n2))"""
        if self.n2 == 0:
            return 0
        return (self.n1 / 2) * (self.N2 / self.n2)
    
    @property
    def effort(self) -> float:
        """Programming effort (E = D * V)"""
        return self.difficulty * self.volume
    
    @property
    def time_to_program(self) -> float:
        """Time to program in seconds (T = E / 18)"""
        return self.effort / 18
    
    @property
    def bugs_delivered(self) -> float:
        """Estimated bugs (B = V / 3000)"""
        return self.volume / 3000


@dataclass
class FileMetrics:
    """Metrics for a single source file"""
    filepath: str
    total_lines: int = 0
    code_lines: int = 0
    comment_lines: int = 0
    blank_lines: int = 0
    cyclomatic_complexity: int = 1
    cognitive_complexity: int = 0
    halstead: HalsteadMetrics = field(default_factory=HalsteadMetrics)
    function_count: int = 0
    class_count: int = 0
    max_nesting_depth: int = 0
    
    @property
    def maintainability_index(self) -> float:
        """
        Maintainability Index (MI)
        MI = 171 - 5.2 * ln(V) - 0.23 * CC - 16.2 * ln(LOC)
        Scaled to 0-100 range
        """
        v = max(self.halstead.volume, 1)
        cc = max(self.cyclomatic_complexity, 1)
        loc = max(self.code_lines, 1)
        
        mi = 171 - 5.2 * math.log(v) - 0.23 * cc - 16.2 * math.log(loc)
        # Scale to 0-100
        mi = max(0, min(100, mi * 100 / 171))
        return mi
    
    @property
    def comment_ratio(self) -> float:
        """Ratio of comment lines to code lines"""
        if self.code_lines == 0:
            return 0
        return self.comment_lines / self.code_lines


@dataclass
class DirectoryMetrics:
    """Aggregated metrics for a directory"""
    path: str
    file_count: int = 0
    total_lines: int = 0
    code_lines: int = 0
    comment_lines: int = 0
    blank_lines: int = 0
    avg_cyclomatic: float = 0
    avg_cognitive: float = 0
    avg_maintainability: float = 0
    total_functions: int = 0
    total_classes: int = 0
    files: List[FileMetrics] = field(default_factory=list)


class CppAnalyzer:
    """Analyzer for C/C++ source code"""
    
    # C/C++ operators for Halstead metrics
    OPERATORS = {
        # Arithmetic
        '+', '-', '*', '/', '%', '++', '--',
        # Relational
        '==', '!=', '<', '>', '<=', '>=', '<=>',
        # Logical
        '&&', '||', '!',
        # Bitwise
        '&', '|', '^', '~', '<<', '>>',
        # Assignment
        '=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>=',
        # Member access
        '.', '->', '.*', '->*', '::',
        # Other
        '?', ':', ',', ';', '(', ')', '[', ']', '{', '}',
        # Keywords as operators
        'sizeof', 'alignof', 'typeid', 'new', 'delete', 'throw', 'co_await', 'co_yield',
    }
    
    # Keywords that increase cyclomatic complexity
    CC_KEYWORDS = {'if', 'else', 'for', 'while', 'do', 'switch', 'case', 'catch', '&&', '||', '?'}
    
    # Keywords that increase cognitive complexity
    COGNITIVE_KEYWORDS = {'if', 'else', 'for', 'while', 'do', 'switch', 'catch', 'goto', 'break', 'continue'}
    COGNITIVE_NESTING = {'if', 'for', 'while', 'do', 'switch', 'catch', 'try'}
    
    def __init__(self, root_path: str, exclude_dirs: List[str] = None):
        self.root_path = Path(root_path)
        self.exclude_dirs = set(exclude_dirs or [])
        self.file_metrics: List[FileMetrics] = []
        self.directory_metrics: Dict[str, DirectoryMetrics] = {}
        
    def should_exclude(self, path: Path) -> bool:
        """Check if path should be excluded"""
        for part in path.parts:
            if part in self.exclude_dirs:
                return True
        return False
    
    def find_source_files(self) -> List[Path]:
        """Find all C/C++ source files"""
        extensions = {'.cpp', '.c', '.h', '.hpp', '.cc', '.cxx', '.hxx', '.inl'}
        files = []
        
        for ext in extensions:
            for file in self.root_path.rglob(f'*{ext}'):
                if not self.should_exclude(file.relative_to(self.root_path)):
                    files.append(file)
        
        return sorted(files)
    
    def remove_comments_and_strings(self, code: str) -> Tuple[str, int]:
        """Remove comments and string literals, return cleaned code and comment line count"""
        result = []
        comment_lines = set()
        i = 0
        line_num = 1
        in_string = False
        string_char = None
        
        while i < len(code):
            # Track line numbers
            if code[i] == '\n':
                line_num += 1
                result.append(code[i])
                i += 1
                continue
            
            # Handle string literals
            if not in_string and code[i] in '"\'':
                in_string = True
                string_char = code[i]
                result.append(' ')  # Replace with space
                i += 1
                continue
            
            if in_string:
                if code[i] == '\\' and i + 1 < len(code):
                    i += 2  # Skip escaped character
                elif code[i] == string_char:
                    in_string = False
                    i += 1
                else:
                    i += 1
                continue
            
            # Handle single-line comments
            if code[i:i+2] == '//':
                comment_lines.add(line_num)
                while i < len(code) and code[i] != '\n':
                    i += 1
                continue
            
            # Handle multi-line comments
            if code[i:i+2] == '/*':
                start_line = line_num
                i += 2
                while i < len(code) and code[i:i+2] != '*/':
                    if code[i] == '\n':
                        line_num += 1
                        comment_lines.add(line_num)
                    i += 1
                comment_lines.add(start_line)
                if i < len(code):
                    i += 2  # Skip */
                continue
            
            result.append(code[i])
            i += 1
        
        return ''.join(result), len(comment_lines)
    
    def count_lines(self, code: str) -> Tuple[int, int, int, int]:
        """Count total, code, comment, and blank lines"""
        lines = code.split('\n')
        total = len(lines)
        blank = sum(1 for line in lines if not line.strip())
        
        # Get comment lines from original code
        _, comment_count = self.remove_comments_and_strings(code)
        
        # Code lines = total - blank - pure comment lines (approximation)
        code_lines = total - blank - comment_count
        code_lines = max(0, code_lines)
        
        return total, code_lines, comment_count, blank
    
    def calculate_cyclomatic_complexity(self, code: str) -> int:
        """Calculate McCabe's Cyclomatic Complexity"""
        cleaned_code, _ = self.remove_comments_and_strings(code)
        
        cc = 1  # Base complexity
        
        # Count decision points
        patterns = [
            r'\bif\b', r'\belse\s+if\b', r'\bfor\b', r'\bwhile\b', 
            r'\bdo\b', r'\bcase\b', r'\bcatch\b', r'\b\?\b',
            r'&&', r'\|\|'
        ]
        
        for pattern in patterns:
            cc += len(re.findall(pattern, cleaned_code))
        
        return cc
    
    def calculate_cognitive_complexity(self, code: str) -> int:
        """
        Calculate Cognitive Complexity (SonarSource metric)
        Measures how hard code is to understand
        """
        cleaned_code, _ = self.remove_comments_and_strings(code)
        
        cognitive = 0
        nesting_level = 0
        lines = cleaned_code.split('\n')
        
        for line in lines:
            stripped = line.strip()
            
            # Check for nesting increasers
            nesting_increase = False
            for keyword in self.COGNITIVE_NESTING:
                if re.search(rf'\b{keyword}\b', stripped):
                    nesting_increase = True
                    cognitive += 1 + nesting_level  # Base + nesting penalty
                    break
            
            # Count logical operators (each adds 1)
            cognitive += len(re.findall(r'&&|\|\|', stripped))
            
            # Count goto, break to label, continue to label
            if re.search(r'\bgoto\b', stripped):
                cognitive += 1
            
            # Update nesting level
            nesting_level += stripped.count('{')
            nesting_level -= stripped.count('}')
            nesting_level = max(0, nesting_level)
        
        return cognitive
    
    def calculate_halstead_metrics(self, code: str) -> HalsteadMetrics:
        """Calculate Halstead complexity metrics"""
        cleaned_code, _ = self.remove_comments_and_strings(code)
        
        operators: Dict[str, int] = defaultdict(int)
        operands: Dict[str, int] = defaultdict(int)
        
        # Find all identifiers and numbers (operands)
        identifiers = re.findall(r'\b[a-zA-Z_][a-zA-Z0-9_]*\b', cleaned_code)
        numbers = re.findall(r'\b\d+\.?\d*[fFuUlL]*\b', cleaned_code)
        
        # C++ keywords (not operands)
        keywords = {
            'auto', 'break', 'case', 'char', 'const', 'continue', 'default',
            'do', 'double', 'else', 'enum', 'extern', 'float', 'for', 'goto',
            'if', 'int', 'long', 'register', 'return', 'short', 'signed',
            'sizeof', 'static', 'struct', 'switch', 'typedef', 'union',
            'unsigned', 'void', 'volatile', 'while', 'class', 'public',
            'private', 'protected', 'virtual', 'override', 'final', 'const',
            'constexpr', 'nullptr', 'true', 'false', 'template', 'typename',
            'namespace', 'using', 'try', 'catch', 'throw', 'new', 'delete',
            'inline', 'static_cast', 'dynamic_cast', 'const_cast', 'reinterpret_cast',
            'explicit', 'friend', 'mutable', 'operator', 'this', 'bool',
            'noexcept', 'decltype', 'alignas', 'alignof', 'static_assert',
            'thread_local', 'concept', 'requires', 'co_await', 'co_return', 'co_yield'
        }
        
        # Count operators from keywords
        for kw in ['sizeof', 'new', 'delete', 'throw', 'return']:
            count = len(re.findall(rf'\b{kw}\b', cleaned_code))
            if count > 0:
                operators[kw] += count
        
        # Count identifiers as operands (excluding keywords)
        for ident in identifiers:
            if ident not in keywords:
                operands[ident] += 1
        
        # Count numbers as operands
        for num in numbers:
            operands[num] += 1
        
        # Count symbol operators
        sorted_ops = sorted(
            [op for op in self.OPERATORS if not op.isalpha()],
            key=len, reverse=True
        )
        
        temp_code = cleaned_code
        for op in sorted_ops:
            escaped_op = re.escape(op)
            count = len(re.findall(escaped_op, temp_code))
            if count > 0:
                operators[op] += count
                # Remove to avoid double counting
                temp_code = re.sub(escaped_op, ' ', temp_code)
        
        return HalsteadMetrics(
            n1=len(operators),
            n2=len(operands),
            N1=sum(operators.values()),
            N2=sum(operands.values())
        )
    
    def count_functions_and_classes(self, code: str) -> Tuple[int, int]:
        """Count functions and classes in the code"""
        cleaned_code, _ = self.remove_comments_and_strings(code)
        
        # Function pattern (simplified)
        func_pattern = r'\b\w+\s+\w+\s*\([^)]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?\s*\{'
        functions = len(re.findall(func_pattern, cleaned_code))
        
        # Class/struct pattern
        class_pattern = r'\b(?:class|struct)\s+\w+'
        classes = len(re.findall(class_pattern, cleaned_code))
        
        return functions, classes
    
    def calculate_max_nesting(self, code: str) -> int:
        """Calculate maximum nesting depth"""
        cleaned_code, _ = self.remove_comments_and_strings(code)
        
        max_depth = 0
        current_depth = 0
        
        for char in cleaned_code:
            if char == '{':
                current_depth += 1
                max_depth = max(max_depth, current_depth)
            elif char == '}':
                current_depth = max(0, current_depth - 1)
        
        return max_depth
    
    def analyze_file(self, filepath: Path) -> FileMetrics:
        """Analyze a single source file"""
        try:
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                code = f.read()
        except Exception as e:
            print(f"Error reading {filepath}: {e}")
            return FileMetrics(filepath=str(filepath))
        
        total, code_lines, comment_lines, blank = self.count_lines(code)
        cc = self.calculate_cyclomatic_complexity(code)
        cognitive = self.calculate_cognitive_complexity(code)
        halstead = self.calculate_halstead_metrics(code)
        functions, classes = self.count_functions_and_classes(code)
        max_nesting = self.calculate_max_nesting(code)
        
        return FileMetrics(
            filepath=str(filepath.relative_to(self.root_path)),
            total_lines=total,
            code_lines=code_lines,
            comment_lines=comment_lines,
            blank_lines=blank,
            cyclomatic_complexity=cc,
            cognitive_complexity=cognitive,
            halstead=halstead,
            function_count=functions,
            class_count=classes,
            max_nesting_depth=max_nesting
        )
    
    def analyze(self) -> None:
        """Analyze all source files"""
        files = self.find_source_files()
        print(f"Found {len(files)} source files to analyze...")
        
        for i, filepath in enumerate(files):
            if (i + 1) % 50 == 0:
                print(f"  Analyzed {i + 1}/{len(files)} files...")
            
            metrics = self.analyze_file(filepath)
            self.file_metrics.append(metrics)
        
        print(f"Analyzed {len(files)} files.")
        self._aggregate_by_directory()
    
    def _aggregate_by_directory(self) -> None:
        """Aggregate metrics by directory"""
        dir_files: Dict[str, List[FileMetrics]] = defaultdict(list)
        
        for fm in self.file_metrics:
            dir_path = str(Path(fm.filepath).parent)
            dir_files[dir_path].append(fm)
        
        for dir_path, files in dir_files.items():
            if not files:
                continue
            
            dm = DirectoryMetrics(
                path=dir_path,
                file_count=len(files),
                total_lines=sum(f.total_lines for f in files),
                code_lines=sum(f.code_lines for f in files),
                comment_lines=sum(f.comment_lines for f in files),
                blank_lines=sum(f.blank_lines for f in files),
                avg_cyclomatic=sum(f.cyclomatic_complexity for f in files) / len(files),
                avg_cognitive=sum(f.cognitive_complexity for f in files) / len(files),
                avg_maintainability=sum(f.maintainability_index for f in files) / len(files),
                total_functions=sum(f.function_count for f in files),
                total_classes=sum(f.class_count for f in files),
                files=files
            )
            self.directory_metrics[dir_path] = dm
    
    def get_summary(self) -> Dict:
        """Get overall summary statistics"""
        if not self.file_metrics:
            return {}
        
        total_files = len(self.file_metrics)
        total_loc = sum(f.code_lines for f in self.file_metrics)
        total_lines = sum(f.total_lines for f in self.file_metrics)
        
        return {
            'total_files': total_files,
            'total_lines': total_lines,
            'total_code_lines': total_loc,
            'total_comment_lines': sum(f.comment_lines for f in self.file_metrics),
            'total_blank_lines': sum(f.blank_lines for f in self.file_metrics),
            'avg_cyclomatic': sum(f.cyclomatic_complexity for f in self.file_metrics) / total_files,
            'max_cyclomatic': max(f.cyclomatic_complexity for f in self.file_metrics),
            'avg_cognitive': sum(f.cognitive_complexity for f in self.file_metrics) / total_files,
            'max_cognitive': max(f.cognitive_complexity for f in self.file_metrics),
            'avg_maintainability': sum(f.maintainability_index for f in self.file_metrics) / total_files,
            'min_maintainability': min(f.maintainability_index for f in self.file_metrics),
            'total_functions': sum(f.function_count for f in self.file_metrics),
            'total_classes': sum(f.class_count for f in self.file_metrics),
            'avg_halstead_volume': sum(f.halstead.volume for f in self.file_metrics) / total_files,
            'total_estimated_bugs': sum(f.halstead.bugs_delivered for f in self.file_metrics),
        }
    
    def print_report(self) -> None:
        """Print a text report of the analysis"""
        summary = self.get_summary()
        
        print("\n" + "=" * 80)
        print("CODE COMPLEXITY AND MAINTAINABILITY REPORT")
        print("=" * 80)
        
        print("\n?? OVERALL SUMMARY")
        print("-" * 40)
        print(f"  Total Files:          {summary['total_files']:,}")
        print(f"  Total Lines:          {summary['total_lines']:,}")
        print(f"  Code Lines (SLOC):    {summary['total_code_lines']:,}")
        print(f"  Comment Lines:        {summary['total_comment_lines']:,}")
        print(f"  Blank Lines:          {summary['total_blank_lines']:,}")
        print(f"  Total Functions:      {summary['total_functions']:,}")
        print(f"  Total Classes:        {summary['total_classes']:,}")
        
        print("\n?? COMPLEXITY METRICS")
        print("-" * 40)
        print(f"  Avg Cyclomatic:       {summary['avg_cyclomatic']:.2f}")
        print(f"  Max Cyclomatic:       {summary['max_cyclomatic']}")
        print(f"  Avg Cognitive:        {summary['avg_cognitive']:.2f}")
        print(f"  Max Cognitive:        {summary['max_cognitive']}")
        print(f"  Avg Halstead Volume:  {summary['avg_halstead_volume']:.2f}")
        print(f"  Est. Total Bugs:      {summary['total_estimated_bugs']:.2f}")
        
        print("\n?? MAINTAINABILITY")
        print("-" * 40)
        print(f"  Avg Maintainability Index: {summary['avg_maintainability']:.2f}")
        print(f"  Min Maintainability Index: {summary['min_maintainability']:.2f}")
        
        mi_avg = summary['avg_maintainability']
        if mi_avg >= 80:
            rating = "Excellent (highly maintainable)"
        elif mi_avg >= 60:
            rating = "Good (moderately maintainable)"
        elif mi_avg >= 40:
            rating = "Fair (somewhat difficult to maintain)"
        else:
            rating = "Poor (difficult to maintain)"
        print(f"  Overall Rating:       {rating}")
        
        # Directory breakdown
        print("\n?? TOP DIRECTORIES BY CODE LINES")
        print("-" * 40)
        sorted_dirs = sorted(
            self.directory_metrics.values(),
            key=lambda d: d.code_lines,
            reverse=True
        )[:10]
        
        for dm in sorted_dirs:
            print(f"  {dm.path}")
            print(f"    Files: {dm.file_count}, LOC: {dm.code_lines:,}, "
                  f"Avg CC: {dm.avg_cyclomatic:.1f}, MI: {dm.avg_maintainability:.1f}")
        
        # Most complex files
        print("\n?? TOP 10 MOST COMPLEX FILES (by Cyclomatic Complexity)")
        print("-" * 40)
        sorted_files = sorted(
            self.file_metrics,
            key=lambda f: f.cyclomatic_complexity,
            reverse=True
        )[:10]
        
        for fm in sorted_files:
            print(f"  {fm.filepath}")
            print(f"    CC: {fm.cyclomatic_complexity}, Cognitive: {fm.cognitive_complexity}, "
                  f"MI: {fm.maintainability_index:.1f}")
        
        # Lowest maintainability
        print("\n?? TOP 10 FILES NEEDING ATTENTION (lowest Maintainability Index)")
        print("-" * 40)
        sorted_mi = sorted(
            self.file_metrics,
            key=lambda f: f.maintainability_index
        )[:10]
        
        for fm in sorted_mi:
            print(f"  {fm.filepath}")
            print(f"    MI: {fm.maintainability_index:.1f}, CC: {fm.cyclomatic_complexity}, "
                  f"LOC: {fm.code_lines}")


class MetricsVisualizer:
    """Visualize code metrics"""
    
    def __init__(self, analyzer: CppAnalyzer, output_dir: str = None):
        self.analyzer = analyzer
        self.output_dir = Path(output_dir) if output_dir else Path.cwd()
        self.output_dir.mkdir(parents=True, exist_ok=True)
    
    def create_all_visualizations(self) -> None:
        """Create all visualizations"""
        if not HAS_MATPLOTLIB:
            print("Skipping visualizations (matplotlib not installed)")
            return
        
        print("\nGenerating visualizations...")
        
        self.plot_directory_comparison()
        self.plot_complexity_distribution()
        self.plot_maintainability_heatmap()
        self.plot_loc_breakdown()
        self.plot_halstead_metrics()
        self.plot_top_complex_files()
        
        print(f"Visualizations saved to: {self.output_dir}")
    
    def plot_directory_comparison(self) -> None:
        """Bar chart comparing directories by various metrics"""
        dirs = sorted(
            self.analyzer.directory_metrics.values(),
            key=lambda d: d.code_lines,
            reverse=True
        )[:15]
        
        if not dirs:
            return
        
        fig, axes = plt.subplots(2, 2, figsize=(16, 12))
        fig.suptitle('Directory Metrics Comparison', fontsize=14, fontweight='bold')
        
        names = [d.path.replace('\\', '/').split('/')[-1] or d.path for d in dirs]
        
        # Code lines
        ax = axes[0, 0]
        values = [d.code_lines for d in dirs]
        bars = ax.barh(names, values, color='steelblue')
        ax.set_xlabel('Lines of Code')
        ax.set_title('Code Lines by Directory')
        ax.invert_yaxis()
        
        # Avg Cyclomatic Complexity
        ax = axes[0, 1]
        values = [d.avg_cyclomatic for d in dirs]
        colors = ['green' if v < 10 else 'orange' if v < 20 else 'red' for v in values]
        ax.barh(names, values, color=colors)
        ax.set_xlabel('Average Cyclomatic Complexity')
        ax.set_title('Avg Cyclomatic Complexity by Directory')
        ax.axvline(x=10, color='orange', linestyle='--', alpha=0.7, label='Moderate (10)')
        ax.axvline(x=20, color='red', linestyle='--', alpha=0.7, label='High (20)')
        ax.legend()
        ax.invert_yaxis()
        
        # Avg Cognitive Complexity
        ax = axes[1, 0]
        values = [d.avg_cognitive for d in dirs]
        colors = ['green' if v < 15 else 'orange' if v < 30 else 'red' for v in values]
        ax.barh(names, values, color=colors)
        ax.set_xlabel('Average Cognitive Complexity')
        ax.set_title('Avg Cognitive Complexity by Directory')
        ax.invert_yaxis()
        
        # Maintainability Index
        ax = axes[1, 1]
        values = [d.avg_maintainability for d in dirs]
        colors = ['green' if v >= 65 else 'orange' if v >= 40 else 'red' for v in values]
        ax.barh(names, values, color=colors)
        ax.set_xlabel('Maintainability Index (0-100)')
        ax.set_title('Avg Maintainability Index by Directory')
        ax.axvline(x=65, color='green', linestyle='--', alpha=0.7, label='Good (65)')
        ax.axvline(x=40, color='orange', linestyle='--', alpha=0.7, label='Moderate (40)')
        ax.legend()
        ax.invert_yaxis()
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'directory_comparison.png', dpi=150, bbox_inches='tight')
        plt.close()
    
    def plot_complexity_distribution(self) -> None:
        """Histogram of complexity distributions"""
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('Complexity Metric Distributions', fontsize=14, fontweight='bold')
        
        cc_values = [f.cyclomatic_complexity for f in self.analyzer.file_metrics]
        cog_values = [f.cognitive_complexity for f in self.analyzer.file_metrics]
        mi_values = [f.maintainability_index for f in self.analyzer.file_metrics]
        vol_values = [f.halstead.volume for f in self.analyzer.file_metrics if f.halstead.volume > 0]
        
        # Cyclomatic Complexity Distribution
        ax = axes[0, 0]
        ax.hist(cc_values, bins=30, color='steelblue', edgecolor='black', alpha=0.7)
        ax.axvline(np.mean(cc_values), color='red', linestyle='--', label=f'Mean: {np.mean(cc_values):.1f}')
        ax.axvline(np.median(cc_values), color='orange', linestyle='--', label=f'Median: {np.median(cc_values):.1f}')
        ax.set_xlabel('Cyclomatic Complexity')
        ax.set_ylabel('Number of Files')
        ax.set_title('Cyclomatic Complexity Distribution')
        ax.legend()
        
        # Cognitive Complexity Distribution
        ax = axes[0, 1]
        ax.hist(cog_values, bins=30, color='darkorange', edgecolor='black', alpha=0.7)
        ax.axvline(np.mean(cog_values), color='red', linestyle='--', label=f'Mean: {np.mean(cog_values):.1f}')
        ax.axvline(np.median(cog_values), color='blue', linestyle='--', label=f'Median: {np.median(cog_values):.1f}')
        ax.set_xlabel('Cognitive Complexity')
        ax.set_ylabel('Number of Files')
        ax.set_title('Cognitive Complexity Distribution')
        ax.legend()
        
        # Maintainability Index Distribution
        ax = axes[1, 0]
        colors = ['red' if v < 40 else 'orange' if v < 65 else 'green' for v in sorted(mi_values)]
        ax.hist(mi_values, bins=30, color='seagreen', edgecolor='black', alpha=0.7)
        ax.axvline(np.mean(mi_values), color='red', linestyle='--', label=f'Mean: {np.mean(mi_values):.1f}')
        ax.axvline(65, color='green', linestyle=':', label='Good threshold (65)')
        ax.axvline(40, color='orange', linestyle=':', label='Moderate threshold (40)')
        ax.set_xlabel('Maintainability Index')
        ax.set_ylabel('Number of Files')
        ax.set_title('Maintainability Index Distribution')
        ax.legend()
        
        # Halstead Volume Distribution (log scale)
        ax = axes[1, 1]
        if vol_values:
            ax.hist(vol_values, bins=30, color='purple', edgecolor='black', alpha=0.7)
            ax.set_xlabel('Halstead Volume')
            ax.set_ylabel('Number of Files')
            ax.set_title('Halstead Volume Distribution')
            ax.set_xscale('log')
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'complexity_distribution.png', dpi=150, bbox_inches='tight')
        plt.close()
    
    def plot_maintainability_heatmap(self) -> None:
        """Create a heatmap showing maintainability by directory structure"""
        dirs = list(self.analyzer.directory_metrics.values())
        if not dirs:
            return
        
        # Sort by path for logical grouping
        dirs = sorted(dirs, key=lambda d: d.path)
        
        fig, ax = plt.subplots(figsize=(14, max(8, len(dirs) * 0.3)))
        
        names = [d.path for d in dirs]
        mi_values = [d.avg_maintainability for d in dirs]
        
        # Create color-coded horizontal bars
        cmap = LinearSegmentedColormap.from_list('mi', ['red', 'orange', 'green'])
        normalized = [v / 100 for v in mi_values]
        colors = [cmap(n) for n in normalized]
        
        y_pos = range(len(names))
        bars = ax.barh(y_pos, mi_values, color=colors, edgecolor='black', alpha=0.8)
        
        ax.set_yticks(y_pos)
        ax.set_yticklabels(names, fontsize=8)
        ax.set_xlabel('Maintainability Index')
        ax.set_title('Maintainability Index by Directory\n(Green=Good, Orange=Moderate, Red=Poor)')
        ax.set_xlim(0, 100)
        
        # Add threshold lines
        ax.axvline(x=65, color='green', linestyle='--', alpha=0.5)
        ax.axvline(x=40, color='orange', linestyle='--', alpha=0.5)
        
        # Add value labels
        for bar, val in zip(bars, mi_values):
            ax.text(bar.get_width() + 1, bar.get_y() + bar.get_height()/2,
                   f'{val:.1f}', va='center', fontsize=7)
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'maintainability_heatmap.png', dpi=150, bbox_inches='tight')
        plt.close()
    
    def plot_loc_breakdown(self) -> None:
        """Pie chart and bar chart of LOC breakdown"""
        summary = self.analyzer.get_summary()
        
        fig, axes = plt.subplots(1, 2, figsize=(14, 6))
        fig.suptitle('Lines of Code Analysis', fontsize=14, fontweight='bold')
        
        # Pie chart of line types
        ax = axes[0]
        sizes = [summary['total_code_lines'], summary['total_comment_lines'], summary['total_blank_lines']]
        labels = ['Code Lines', 'Comment Lines', 'Blank Lines']
        colors = ['steelblue', 'seagreen', 'lightgray']
        explode = (0.05, 0, 0)
        
        ax.pie(sizes, explode=explode, labels=labels, colors=colors, autopct='%1.1f%%',
               shadow=True, startangle=90)
        ax.set_title('Line Type Distribution')
        
        # Bar chart of LOC by top directories
        ax = axes[1]
        dirs = sorted(
            self.analyzer.directory_metrics.values(),
            key=lambda d: d.total_lines,
            reverse=True
        )[:12]
        
        names = [d.path.replace('\\', '/').split('/')[-1] or d.path for d in dirs]
        code = [d.code_lines for d in dirs]
        comments = [d.comment_lines for d in dirs]
        blank = [d.blank_lines for d in dirs]
        
        x = np.arange(len(names))
        width = 0.25
        
        ax.bar(x - width, code, width, label='Code', color='steelblue')
        ax.bar(x, comments, width, label='Comments', color='seagreen')
        ax.bar(x + width, blank, width, label='Blank', color='lightgray')
        
        ax.set_xlabel('Directory')
        ax.set_ylabel('Lines')
        ax.set_title('LOC Breakdown by Directory')
        ax.set_xticks(x)
        ax.set_xticklabels(names, rotation=45, ha='right')
        ax.legend()
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'loc_breakdown.png', dpi=150, bbox_inches='tight')
        plt.close()
    
    def plot_halstead_metrics(self) -> None:
        """Visualize Halstead metrics"""
        files = [f for f in self.analyzer.file_metrics if f.halstead.volume > 0]
        if not files:
            return
        
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('Halstead Metrics Analysis', fontsize=14, fontweight='bold')
        
        # Volume vs Difficulty scatter
        ax = axes[0, 0]
        volumes = [f.halstead.volume for f in files]
        difficulties = [f.halstead.difficulty for f in files]
        ax.scatter(volumes, difficulties, alpha=0.5, c='steelblue')
        ax.set_xlabel('Halstead Volume')
        ax.set_ylabel('Halstead Difficulty')
        ax.set_title('Volume vs Difficulty')
        ax.set_xscale('log')
        
        # Effort distribution
        ax = axes[0, 1]
        efforts = [f.halstead.effort for f in files if f.halstead.effort > 0]
        ax.hist(efforts, bins=30, color='darkorange', edgecolor='black', alpha=0.7)
        ax.set_xlabel('Halstead Effort')
        ax.set_ylabel('Number of Files')
        ax.set_title('Programming Effort Distribution')
        ax.set_xscale('log')
        
        # Estimated bugs by directory
        ax = axes[1, 0]
        dirs = sorted(
            self.analyzer.directory_metrics.values(),
            key=lambda d: sum(f.halstead.bugs_delivered for f in d.files),
            reverse=True
        )[:10]
        
        names = [d.path.replace('\\', '/').split('/')[-1] or d.path for d in dirs]
        bugs = [sum(f.halstead.bugs_delivered for f in d.files) for d in dirs]
        
        ax.barh(names, bugs, color='crimson', alpha=0.7)
        ax.set_xlabel('Estimated Bugs (Halstead B)')
        ax.set_title('Estimated Bugs by Directory')
        ax.invert_yaxis()
        
        # Time to program
        ax = axes[1, 1]
        times = [f.halstead.time_to_program / 3600 for f in files if f.halstead.time_to_program > 0]  # Convert to hours
        ax.hist(times, bins=30, color='purple', edgecolor='black', alpha=0.7)
        ax.set_xlabel('Estimated Time to Program (hours)')
        ax.set_ylabel('Number of Files')
        ax.set_title('Estimated Programming Time Distribution')
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'halstead_metrics.png', dpi=150, bbox_inches='tight')
        plt.close()
    
    def plot_top_complex_files(self) -> None:
        """Bar chart of most complex files"""
        fig, axes = plt.subplots(1, 2, figsize=(16, 8))
        fig.suptitle('Files Requiring Attention', fontsize=14, fontweight='bold')
        
        # Top by Cyclomatic Complexity
        ax = axes[0]
        files = sorted(self.analyzer.file_metrics, key=lambda f: f.cyclomatic_complexity, reverse=True)[:15]
        names = [Path(f.filepath).name for f in files]
        values = [f.cyclomatic_complexity for f in files]
        colors = ['green' if v < 10 else 'orange' if v < 20 else 'red' for v in values]
        
        bars = ax.barh(names, values, color=colors, edgecolor='black', alpha=0.8)
        ax.set_xlabel('Cyclomatic Complexity')
        ax.set_title('Top 15 Files by Cyclomatic Complexity')
        ax.invert_yaxis()
        
        # Add threshold annotations
        ax.axvline(x=10, color='orange', linestyle='--', alpha=0.7)
        ax.axvline(x=20, color='red', linestyle='--', alpha=0.7)
        
        # Lowest Maintainability
        ax = axes[1]
        files = sorted(self.analyzer.file_metrics, key=lambda f: f.maintainability_index)[:15]
        names = [Path(f.filepath).name for f in files]
        values = [f.maintainability_index for f in files]
        colors = ['red' if v < 40 else 'orange' if v < 65 else 'green' for v in values]
        
        bars = ax.barh(names, values, color=colors, edgecolor='black', alpha=0.8)
        ax.set_xlabel('Maintainability Index')
        ax.set_title('Top 15 Files with Lowest Maintainability')
        ax.invert_yaxis()
        ax.set_xlim(0, 100)
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'files_requiring_attention.png', dpi=150, bbox_inches='tight')
        plt.close()


def export_csv(analyzer: CppAnalyzer, output_dir: Path) -> None:
    """Export metrics to CSV files"""
    import csv
    
    # File metrics CSV
    file_csv = output_dir / 'file_metrics.csv'
    with open(file_csv, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow([
            'File', 'Total Lines', 'Code Lines', 'Comment Lines', 'Blank Lines',
            'Cyclomatic Complexity', 'Cognitive Complexity', 'Maintainability Index',
            'Functions', 'Classes', 'Max Nesting', 'Halstead Volume', 
            'Halstead Difficulty', 'Halstead Effort', 'Estimated Bugs'
        ])
        
        for fm in analyzer.file_metrics:
            writer.writerow([
                fm.filepath, fm.total_lines, fm.code_lines, fm.comment_lines, fm.blank_lines,
                fm.cyclomatic_complexity, fm.cognitive_complexity, f'{fm.maintainability_index:.2f}',
                fm.function_count, fm.class_count, fm.max_nesting_depth,
                f'{fm.halstead.volume:.2f}', f'{fm.halstead.difficulty:.2f}',
                f'{fm.halstead.effort:.2f}', f'{fm.halstead.bugs_delivered:.3f}'
            ])
    
    print(f"  File metrics exported to: {file_csv}")
    
    # Directory metrics CSV
    dir_csv = output_dir / 'directory_metrics.csv'
    with open(dir_csv, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow([
            'Directory', 'File Count', 'Total Lines', 'Code Lines', 'Comment Lines',
            'Avg Cyclomatic', 'Avg Cognitive', 'Avg Maintainability',
            'Total Functions', 'Total Classes'
        ])
        
        for dm in analyzer.directory_metrics.values():
            writer.writerow([
                dm.path, dm.file_count, dm.total_lines, dm.code_lines, dm.comment_lines,
                f'{dm.avg_cyclomatic:.2f}', f'{dm.avg_cognitive:.2f}', f'{dm.avg_maintainability:.2f}',
                dm.total_functions, dm.total_classes
            ])
    
    print(f"  Directory metrics exported to: {dir_csv}")


def main():
    parser = argparse.ArgumentParser(
        description='Analyze C/C++ codebase for complexity and maintainability metrics'
    )
    parser.add_argument(
        'path',
        nargs='?',
        default='.',
        help='Root path of the codebase to analyze (default: current directory)'
    )
    parser.add_argument(
        '-e', '--exclude',
        nargs='+',
        default=['Middleware', 'ThirdParty', 'External', 'vendor', 'build', 'Build', '.git'],
        help='Directories to exclude from analysis'
    )
    parser.add_argument(
        '-o', '--output',
        default='complexity_report',
        help='Output directory for visualizations and CSV files'
    )
    parser.add_argument(
        '--no-viz',
        action='store_true',
        help='Skip generating visualizations'
    )
    parser.add_argument(
        '--csv',
        action='store_true',
        help='Export metrics to CSV files'
    )
    
    args = parser.parse_args()
    
    root_path = Path(args.path).resolve()
    output_dir = Path(args.output).resolve()
    
    print(f"Analyzing codebase at: {root_path}")
    print(f"Excluding directories: {', '.join(args.exclude)}")
    
    # Create analyzer and run analysis
    analyzer = CppAnalyzer(str(root_path), exclude_dirs=args.exclude)
    analyzer.analyze()
    
    # Print text report
    analyzer.print_report()
    
    # Export CSV if requested
    if args.csv:
        output_dir.mkdir(parents=True, exist_ok=True)
        print("\nExporting CSV files...")
        export_csv(analyzer, output_dir)
    
    # Generate visualizations
    if not args.no_viz and HAS_MATPLOTLIB:
        output_dir.mkdir(parents=True, exist_ok=True)
        visualizer = MetricsVisualizer(analyzer, str(output_dir))
        visualizer.create_all_visualizations()
    
    print("\n? Analysis complete!")
    
    return analyzer


if __name__ == '__main__':
    main()
