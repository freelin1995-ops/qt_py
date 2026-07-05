# 动态表单系统设计文档

## 概述

C++ Qt 客户端嵌入 Python 解释器，后端以声明式 Python DSL 定义表单结构和联动逻辑，前端自动渲染并实时响应交互。

```
后端 (Python)                 前端 (C++ Qt)
┌─────────────────┐          ┌─────────────────────────┐
│  ImageCompressForm │ json    │  DynamicFormWidget       │
│  ├─ BoolField     │──────→  │  ├─ QCheckBox           │
│  ├─ IntField      │ schema  │  ├─ QSpinBox            │
│  ├─ EnumField     │         │  ├─ QComboBox           │
│  └─ @watch        │         │  └─ dispatchChange()    │
│       ↓           │         │       ↓                 │
│  _ContextProxy    │←─cmd──  │  applyProxyCommands()   │
│  [commands]       │  list   │  widget->setVisible()   │
└─────────────────┘          └─────────────────────────┘
```

## 核心文件

| 文件 | 职责 |
|------|------|
| `ui_common.py` | 声明式表单框架（`BaseForm`、`FieldProxy`、`@watch`） |
| `tasks/*.py` | 后端业务脚本，每文件一个表单定义 |
| `main.cpp` | Qt 客户端 + pybind11 嵌入 Python，含 `DynamicFormWidget` 和 `MainWindow` |
| `CMakeLists.txt` | CMake 构建，依赖 Qt6::Widgets + pybind11::embed + Python3 |

## 架构设计

### 1. 后端：声明式表单 DSL（`ui_common.py`）

#### `FieldProxy` — 控件代理基类

每个表单字段对应一个 `FieldProxy` 实例，充当 Python 侧与 C++ 控件之间的引用。

```
FieldProxy
├── label        # 显示标签
├── field_type   # 控件类型（bool/string/int/enum/file）
├── default      # 默认值
├── extra        # 额外参数（min/max/options 等）
├── _cpp_context # 运行时注入的 C++ 代理对象
├── set_visible(visible)   → _cpp_context.set_widget_property(name, "visible", visible)
├── set_enabled(enabled)   → _cpp_context.set_widget_property(name, "enabled", enabled)
└── set_value(value)       → _cpp_context.set_widget_property(name, "value", value)
```

子类：

| 类 | field_type | 对应 Qt 控件 |
|----|-----------|-------------|
| `BoolField` | `"bool"` | `QCheckBox` |
| `TextField` | `"string"` | `QLineEdit` |
| `IntField` | `"int"` | `QSpinBox` |
| `EnumField` | `"enum"` | `QComboBox` |

#### `@watch(*field_names)` — 声明式联动装饰器

在函数上附加 `_watched_fields` 属性标记，`BaseForm.__init__` 通过反射扫描收集到 `self._watchers` 字典：

```python
self._watchers = {
    "enable_watermark": [bound_on_watermark_toggle],
    "compress_mode":    [bound_on_mode_change],
}
```

当 `dispatch_change("enable_watermark", True, proxy)` 被调用时，自动查出绑定函数并执行。

#### `BaseForm` — 表单基类

初始化流程：

1. `inspect.getmembers(self.__class__)` 扫描所有类属性
2. 识别 `FieldProxy` 子类实例 → `self._fields[name] = copy`
3. 识别带 `_watched_fields` 的函数 → `self._watchers[field_name] = [bound_method]`

核心方法：

| 方法 | 作用 |
|------|------|
| `export_meta()` | 导出 JSON schema（字段名、标签、类型、默认值、约束） |
| `dispatch_change(field, value, cpp_context)` | 注入 context、触发 @watch 回调 |

### 2. 通信协议：Python _ContextProxy（`main.cpp setupPythonProxy`）

纯 Python 命令代理类，在 C++ 中通过 `py::exec` 定义：

```python
class _ContextProxy:
    def __init__(self):
        self.commands = []                    # 命令缓冲区
    def set_widget_property(self, field, prop, value):
        self.commands.append((field, prop, value))  # 追加命令
    def clear(self):
        self.commands = []                    # 清空缓冲区
```

**设计理由**：不使用 `py::class_` 绑定 C++ 类到 Python（避免 pybind11 的类型注册和持有者复杂性问题），改用纯 Python 类的命令队列模式——Python 将 UI 操作命令写入列表，C++ 在执行完 `dispatch_change` 后读取并应用。

### 3. 前端：Qt 动态渲染引擎（`main.cpp DynamicFormWidget`）

#### `renderFromSchema(schema, proxy, formInstance)` — 两步渲染

**第一遍：创建控件 + 布局**
```
遍历 schema["fields"]:
  ├── field_type == "bool"   → new QCheckBox, setChecked(default)
  ├── field_type == "string" → new QLineEdit, setText(default)
  ├── field_type == "int"    → new QSpinBox, setRange(min,max), setValue(default)
  ├── field_type == "enum"   → new QComboBox, addItems(options), setCurrentText(default)
  └── field_type == "file"   → new QWidget(QHBoxLayout(QLineEdit + QPushButton))
  每个控件 → m_layout->addRow(label, widget)
           → m_widgets[name] = widget
```

**第二遍：连接信号**
```
遍历 FieldEntry 列表:
  ├── bool   → QCheckBox::toggled → dispatchChange
  ├── string → QLineEdit::textChanged → dispatchChange
  ├── int    → QSpinBox::valueChanged → dispatchChange
  ├── enum   → QComboBox::currentTextChanged → dispatchChange
  └── file   → QLineEdit::textChanged → dispatchChange
```

**两步法的意义**：避免创建过程中的信号误触（`setChecked`/`setCurrentText` 等会发射信号），确保调用 `dispatchChange` 时所有控件已在 `m_widgets` 中注册。

#### `dispatchChange(name, py_value)` — 信号响应

```
1. m_pyProxy.attr("clear")()                    # 清空命令缓冲区
2. m_pyFormInstance.attr("dispatch_change")(...) # 执行 @watch 回调
   └── Python 侧 set_visible/set_enabled → proxy.commands 追加命令
3. applyProxyCommands()                         # 遍历命令并应用到 Qt 控件
```

#### `applyProxyCommands()` — 命令执行

遍历 `m_pyProxy.commands`，每项为 `(field_name, prop, value)` 三元组：

| prop | 操作 |
|------|------|
| `"visible"` | `widget->setVisible(bool)` + `label->setVisible(bool)` |
| `"enabled"` | `widget->setEnabled(bool)` |
| `"value"` | 根据控件类型 `qobject_cast` 后 setValue/setText |

### 4. 主流程（`main.cpp MainWindow`）

```
main()
  ├── py::scoped_interpreter     # 初始化 Python 解释器
  ├── sys.path.append()          # 添加脚本搜索路径
  ├── setupPythonProxy()         # 定义 _ContextProxy 类
  ├── discoverTasks()            # 扫描 tasks/*.py
  ├── loadTask(module)           # 加载当前选中的任务
  │    ├── py::module_::import   # 导入 Python 脚本
  │    ├── get_form_instance()   # 实例化表单
  │    ├── export_meta()         # 导出 JSON schema
  │    ├── json.dumps → QJsonDocument  # 序列化
  │    ├── renderFromSchema()    # 渲染 Qt 控件
  │    └── 初始 dispatch 循环    # 触发 @watch 设初始状态
  └── exec()                     # Qt 事件循环
```

## 扩展指南

### 新增任务类型

在 `tasks/` 目录下新建 `.py` 文件：

```python
from ui_common import BaseForm, BoolField, EnumField, TextField, watch

class MyNewForm(BaseForm):
    task_type = "my_task"
    enable_log = BoolField(label="启用日志", default=False)
    log_path = TextField(label="日志路径", default="/var/log")

    @watch("enable_log")
    def on_log_toggle(self, checked: bool):
        self.log_path.set_visible(checked)

def get_form_instance():
    return MyNewForm()
```

无需修改 C++ 代码，无需重新编译，重启即生效。

### 新增控件类型

需要修改三处：

1. `ui_common.py`：新增 `FieldProxy` 子类
2. `main.cpp DynamicFormWidget::renderFromSchema`：第一遍创建 Qt 控件
3. `main.cpp DynamicFormWidget::renderFromSchema`：第二遍连接信号

## 约束与边界

### 支持的控件类型

| 类型 | Qt 控件 | 支持的 extra 参数 |
|------|---------|-----------------|
| `bool` | `QCheckBox` | — |
| `string` | `QLineEdit` | — |
| `int` | `QSpinBox` | `min`, `max` |
| `enum` | `QComboBox` | `options` |
| `file` | `QLineEdit` + `QPushButton` | `file_filter` |

### 联动能力

- `set_visible(bool)` — 隐藏/显示控件及其标签
- `set_enabled(bool)` — 启用/禁用控件
- `set_value(value)` — 修改控件值（会触发 dispatch，可能形成递归）

### 约束

- 表单定义 Python 脚本中不应有阻塞操作
- `@watch` 回调内修改其他字段时，不应导致无限递归（当前通过 `clear()` 在每次 dispatch 前清空命令来缓解）
- C++ 侧不可直接访问 Python 堆对象（通过 `_ContextProxy.commands` 间接通信）

## 构建与运行

```bash
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="$(python3 -c 'import pybind11; print(pybind11.get_cmake_dir())')" \
  -DPython3_EXECUTABLE=$(which python3)
cmake --build build
cd build && ./qt_py
```
