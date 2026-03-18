#!/usr/bin/env python3
"""
问题解析器 - 从日志和错误信息中提取结构化问题信息

使用方法:
    python problem-parser.py --file build.log
    python problem-parser.py --type compilation --input "error message"
"""

import re
import json
import argparse
from datetime import datetime
from pathlib import Path
from typing import List, Dict, Optional

class ProblemParser:
    def __init__(self, knowledge_base_path: str = None):
        if knowledge_base_path is None:
            # 默认路径是相对于此脚本的上级目录
            self.kb_path = Path(__file__).parent.parent
        else:
            self.kb_path = Path(knowledge_base_path)

        self.problems_file = self.kb_path / "index" / "problems.json"
        self.tags_file = self.kb_path / "index" / "tags.json"

        # 加载标签信息
        self.load_tags()

    def load_tags(self):
        """加载标签分类信息"""
        try:
            with open(self.tags_file, 'r', encoding='utf-8') as f:
                self.tags_data = json.load(f)
        except FileNotFoundError:
            print(f"警告: 标签文件 {self.tags_file} 不存在")
            self.tags_data = {}

    def parse_build_log(self, log_file: str) -> List[Dict]:
        """解析构建日志，提取错误信息"""
        errors = []
        try:
            with open(log_file, 'r', encoding='utf-8') as f:
                content = f.read()

            # 匹配常见编译错误模式
            patterns = {
                'undefined_reference': r'undefined reference to [`\']([^`\']+)[`\']',
                'file_not_found': r'([^\s:]+):(\d+):\d+: error: ([^:]+): No such file',
                'syntax_error': r'([^\s:]+):(\d+):\d+: error: ([^:]+)',
                'linker_error': r'LINK : fatal error LNK\d+: ([^:]+)',
                'qt_moc_error': r'(?:Error while reading|Moc) (.+)',
                'cmake_error': r'CMake Error at ([^:]+):(\d+) \([^\)]+\):\s*(.+)',
            }

            lines = content.split('\n')
            for line_num, line in enumerate(lines, 1):
                for error_type, pattern in patterns.items():
                    match = re.search(pattern, line, re.IGNORECASE)
                    if match:
                        error_info = self._extract_error_info(
                            error_type, match, line, line_num, lines[max(0, line_num-2):line_num+2]
                        )
                        if error_info:
                            errors.append(error_info)

        except FileNotFoundError:
            print(f"错误: 文件 {log_file} 不存在")
        except Exception as e:
            print(f"解析文件时出错: {e}")

        return errors

    def _extract_error_info(self, error_type: str, match, line: str, line_num: int, context: List[str]) -> Dict:
        """提取错误信息"""
        base_info = {
            'type': error_type,
            'line_number': line_num,
            'message': line.strip(),
            'context': context,
            'timestamp': datetime.now().isoformat()
        }

        # 根据错误类型提取特定信息
        if error_type == 'undefined_reference':
            base_info['symbol'] = match.group(1)
            base_info['suggested_tags'] = ['linker', 'undefined-reference']

        elif error_type == 'file_not_found':
            base_info['file'] = match.group(1)
            base_info['file_line'] = match.group(2)
            base_info['error_desc'] = match.group(3)
            base_info['suggested_tags'] = ['file-not-found', 'include-path']

        elif error_type == 'syntax_error':
            base_info['file'] = match.group(1)
            base_info['file_line'] = match.group(2)
            base_info['error_desc'] = match.group(3)
            base_info['suggested_tags'] = ['syntax-error', 'compiler']

        elif error_type == 'qt_moc_error':
            base_info['file'] = match.group(1)
            base_info['suggested_tags'] = ['qt', 'moc', 'meta-object']

        elif error_type == 'cmake_error':
            base_info['file'] = match.group(1)
            base_info['file_line'] = match.group(2)
            base_info['error_desc'] = match.group(3)
            base_info['suggested_tags'] = ['cmake', 'build-system']

        return base_info

    def analyze_error_trends(self, errors: List[Dict]) -> Dict:
        """分析错误趋势"""
        trends = {
            'total_errors': len(errors),
            'error_types': {},
            'suggested_categories': {},
            'most_common_files': {},
        }

        for error in errors:
            # 统计错误类型
            error_type = error.get('type', 'unknown')
            trends['error_types'][error_type] = trends['error_types'].get(error_type, 0) + 1

            # 建议分类
            if 'compiler' in error.get('suggested_tags', []):
                category = 'compilation'
            elif 'linker' in error.get('suggested_tags', []):
                category = 'compilation'
            elif 'qt' in error.get('suggested_tags', []):
                category = 'compilation'
            elif 'cmake' in error.get('suggested_tags', []):
                category = 'config'
            else:
                category = 'unknown'

            trends['suggested_categories'][category] = trends['suggested_categories'].get(category, 0) + 1

            # 统计文件错误频率
            file_name = error.get('file', 'unknown')
            if file_name != 'unknown':
                trends['most_common_files'][file_name] = trends['most_common_files'].get(file_name, 0) + 1

        return trends

    def generate_problem_suggestions(self, errors: List[Dict]) -> List[Dict]:
        """生成问题记录建议"""
        suggestions = []

        # 按错误类型分组
        error_groups = {}
        for error in errors:
            error_type = error.get('type', 'unknown')
            if error_type not in error_groups:
                error_groups[error_type] = []
            error_groups[error_type].append(error)

        # 为每种错误类型生成建议
        for error_type, type_errors in error_groups.items():
            if len(type_errors) >= 2:  # 只为重复出现的问题生成建议
                suggestion = {
                    'title': f'重复出现的{error_type}错误',
                    'type': error_type,
                    'occurrences': len(type_errors),
                    'examples': type_errors[:3],  # 最多显示3个例子
                    'suggested_title': f'解决{error_type}问题的系统性方法',
                    'suggested_tags': list(set([tag for error in type_errors
                                               for tag in error.get('suggested_tags', [])])),
                    'severity': 'high' if len(type_errors) >= 5 else 'medium'
                }
                suggestions.append(suggestion)

        return suggestions

    def parse_single_error(self, error_message: str, error_type: str = None) -> Optional[Dict]:
        """解析单个错误消息"""
        patterns = {
            'undefined_reference': r'undefined reference to [`\']([^`\']+)[`\']',
            'syntax_error': r'([^\s:]+):(\d+):\d+: error: ([^:]+)',
            'linker_error': r'LINK : fatal error LNK\d+: ([^:]+)',
        }

        if error_type:
            pattern = patterns.get(error_type)
            if pattern:
                match = re.search(pattern, error_message)
                if match:
                    return self._extract_error_info(error_type, match, error_message, 0, [error_message])
        else:
            # 尝试所有模式
            for err_type, pattern in patterns.items():
                match = re.search(pattern, error_message)
                if match:
                    return self._extract_error_info(err_type, match, error_message, 0, [error_message])

        return None

def main():
    parser = argparse.ArgumentParser(description='问题解析器')
    parser.add_argument('--file', help='要解析的日志文件')
    parser.add_argument('--type', choices=['compilation', 'runtime', 'hardware', 'integration', 'config'],
                       help='问题类型')
    parser.add_argument('--input', help='单个错误消息')
    parser.add_argument('--kb-path', help='知识库路径（默认为当前目录）')
    parser.add_argument('--output', help='输出文件路径（JSON格式）')

    args = parser.parse_args()

    # 初始化解析器
    problem_parser = ProblemParser(args.kb_path)

    if args.file:
        # 解析日志文件
        print(f"正在解析文件: {args.file}")
        errors = problem_parser.parse_build_log(args.file)

        # 分析趋势
        trends = problem_parser.analyze_error_trends(errors)

        # 生成建议
        suggestions = problem_parser.generate_problem_suggestions(errors)

        # 输出结果
        result = {
            'file': args.file,
            'parse_time': datetime.now().isoformat(),
            'summary': {
                'total_errors': len(errors),
                'error_types': trends['error_types'],
                'suggested_categories': trends['suggested_categories'],
            },
            'errors': errors,
            'trends': trends,
            'suggestions': suggestions
        }

    elif args.input:
        # 解析单个错误
        error = problem_parser.parse_single_error(args.input, args.type)
        result = {
            'input': args.input,
            'parse_time': datetime.now().isoformat(),
            'error': error
        }

    else:
        print("请指定 --file 或 --input 参数")
        return

    # 输出结果
    if args.output:
        with open(args.output, 'w', encoding='utf-8') as f:
            json.dump(result, f, indent=2, ensure_ascii=False)
        print(f"结果已保存到: {args.output}")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))

if __name__ == "__main__":
    main()