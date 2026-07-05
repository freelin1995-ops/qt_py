#!/usr/bin/env python3
"""
调试 task 脚本的 CLI 工具。
用法:
  python3 debug_task.py tasks.image_task
  python3 debug_task.py tasks.image_task --interactive
  python3 debug_task.py tasks.image_task --dispatch enable_watermark True
"""

import sys
import json
import importlib


class DebugProxy:
    def __init__(self):
        self.commands = []

    def set_widget_property(self, field_name, prop, value):
        self.commands.append((field_name, prop, value))
        print(f"  >>> set_widget_property({field_name!r}, {prop!r}, {value!r})")

    def clear(self):
        self.commands = []


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    module_path = sys.argv[1]

    try:
        module = importlib.import_module(module_path)
    except ModuleNotFoundError as e:
        print(f"❌ 导入模块失败: {e}")
        print("确保 <模块名> 是 Python 模块路径，例如 tasks.image_task")
        print(f"当前 sys.path: {sys.path[:3]}")
        sys.exit(1)

    try:
        form = module.get_form_instance()
    except AttributeError as e:
        print(f"❌ 模块中没有 get_form_instance(): {e}")
        sys.exit(1)

    # --- 打印 schema ---
    meta = form.export_meta()
    print(f"\n{'='*60}")
    print(f"task_type: {meta['type']}")
    print(f"{'='*60}")
    print(json.dumps(meta, indent=2, ensure_ascii=False))
    print()

    # --- 打印字段一览 ---
    print(f"{'字段名':<25} {'类型':<10} {'标签':<20} {'默认值':<15}")
    print("-" * 70)
    for name, field in form._fields.items():
        print(f"{name:<25} {field.field_type:<10} {field.label:<20} {str(field.default):<15}")
    print()

    proxy = DebugProxy()

    # --- 初始分发（模拟 C++ 加载时的行为） ---
    print(f"{'='*60}")
    print("初始 @watch 分发 (使用默认值)")
    print(f"{'='*60}")
    for field_name, field in form._fields.items():
        proxy.commands = []
        form.dispatch_change(field_name, field.default, proxy)
        if proxy.commands:
            print(f"  dispatch_change({field_name!r}, {field.default!r})")
            for cmd in proxy.commands:
                print(f"    -> {cmd}")
    print()

    # --- 交互模式或命令行 dispatch ---
    if "--interactive" in sys.argv:
        print(f"{'='*60}")
        print("交互模式，输入命令: <字段名> <新值>")
        print("例如: enable_watermark True")
        print("      compress_mode 固定码率")
        print("      空行退出")
        print(f"{'='*60}")
        while True:
            try:
                line = input("> ").strip()
            except EOFError:
                break
            if not line:
                break
            parts = line.split(maxsplit=1)
            if len(parts) < 2:
                print("  格式: <字段名> <新值>")
                continue
            field_name, value_str = parts
            if field_name not in form._fields:
                print(f"  ❌ 未知字段: {field_name}，可用: {list(form._fields.keys())}")
                continue

            field = form._fields[field_name]
            try:
                if field.field_type == "bool":
                    value = value_str.lower() in ("true", "1", "yes")
                elif field.field_type == "int":
                    value = int(value_str)
                else:
                    value = value_str
            except ValueError as e:
                print(f"  ❌ 值转换失败: {e}")
                continue

            proxy.commands = []
            form.dispatch_change(field_name, value, proxy)
            print(f"  dispatch_change({field_name!r}, {value!r})")
            if proxy.commands:
                for cmd in proxy.commands:
                    print(f"    -> {cmd}")
            else:
                print("    (无联动效果)")
            print()

    elif len(sys.argv) > 2 and sys.argv[2] == "--dispatch" and len(sys.argv) >= 5:
        field_name = sys.argv[3]
        value_str = sys.argv[4]
        value = value_str
        field = form._fields.get(field_name)
        if field:
            if field.field_type == "bool":
                value = value_str.lower() in ("true", "1", "yes")
            elif field.field_type == "int":
                value = int(value_str)

        proxy.commands = []
        form.dispatch_change(field_name, value, proxy)
        print(f"dispatch_change({field_name!r}, {value!r})")
        for cmd in proxy.commands:
            print(f"  -> {cmd}")


if __name__ == "__main__":
    main()
