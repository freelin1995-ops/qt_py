import inspect
import copy


class FieldProxy:
    def __init__(self, label: str, field_type: str, default=None, **kwargs):
        self.label = label
        self.field_type = field_type
        self.default = default
        self.extra = kwargs
        self.name = None
        self._cpp_context = None

    def bind_cpp(self, name, cpp_context):
        self.name = name
        self._cpp_context = cpp_context

    def set_visible(self, visible: bool):
        if self._cpp_context:
            self._cpp_context.set_widget_property(self.name, "visible", visible)

    def set_enabled(self, enabled: bool):
        if self._cpp_context:
            self._cpp_context.set_widget_property(self.name, "enabled", enabled)

    def set_value(self, value):
        if self._cpp_context:
            self._cpp_context.set_widget_property(self.name, "value", value)


class BoolField(FieldProxy):
    def __init__(self, label, default=False):
        super().__init__(label, "bool", default)


class TextField(FieldProxy):
    def __init__(self, label, default=""):
        super().__init__(label, "string", default)


class IntField(FieldProxy):
    def __init__(self, label, default=0, min_val=0, max_val=100):
        super().__init__(label, "int", default, min=min_val, max=max_val)


class EnumField(FieldProxy):
    def __init__(self, label, options: list, default=None):
        super().__init__(label, "enum", default or (options[0] if options else ""), options=options)


def watch(*fields):
    def decorator(func):
        func._watched_fields = fields
        return func
    return decorator


class BaseForm:
    def __init__(self):
        self._fields = {}
        self._watchers = {}

        for name, value in inspect.getmembers(self.__class__):
            if isinstance(value, FieldProxy):
                field_instance = copy.copy(value)
                self._fields[name] = field_instance
                setattr(self, name, field_instance)

        for name, value in inspect.getmembers(self.__class__):
            if hasattr(value, "_watched_fields"):
                for field_name in value._watched_fields:
                    if field_name not in self._watchers:
                        self._watchers[field_name] = []
                    self._watchers[field_name].append(value.__get__(self, type(self)))

    def export_meta(self) -> dict:
        meta_fields = []
        for name, field in self._fields.items():
            entry = {
                "name": name,
                "label": field.label,
                "field_type": field.field_type,
                "default": field.default,
            }
            entry.update(field.extra)
            meta_fields.append(entry)
        return {
            "type": getattr(self, "task_type", "unknown"),
            "fields": meta_fields
        }

    def dispatch_change(self, field_name: str, new_value, cpp_context):
        for name, field in self._fields.items():
            field.bind_cpp(name, cpp_context)

        if field_name in self._watchers:
            for watcher_func in self._watchers[field_name]:
                watcher_func(new_value)
