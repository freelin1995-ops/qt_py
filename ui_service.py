import sys
import json
import importlib


class RecordingProxy:
    def __init__(self):
        self.commands = []

    def set_widget_property(self, field_name, prop, value):
        self.commands.append({
            "field": field_name,
            "prop": prop,
            "value": value
        })

    def clear(self):
        self.commands = []


def main():
    if len(sys.argv) < 2:
        print(json.dumps({"type": "error", "message": "Usage: ui_service.py <module_path>"}), flush=True)
        return

    module_path = sys.argv[1]
    try:
        module = importlib.import_module(module_path)
        form = module.get_form_instance()
    except Exception as e:
        print(json.dumps({"type": "error", "message": f"Failed to load module: {e}"}), flush=True)
        return

    proxy = RecordingProxy()

    schema = form.export_meta()
    print(json.dumps({"type": "schema", "data": schema}), flush=True)

    for field_name, field in form._fields.items():
        proxy.clear()
        form.dispatch_change(field_name, field.default, proxy)
        if proxy.commands:
            print(json.dumps({"type": "ui_commands", "commands": proxy.commands}), flush=True)

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            cmd = json.loads(line)
            if cmd.get("cmd") == "change":
                proxy.clear()
                form.dispatch_change(cmd["field"], cmd["value"], proxy)
                print(json.dumps({"type": "ui_commands", "commands": proxy.commands}), flush=True)
            else:
                print(json.dumps({"type": "error", "message": f"Unknown command: {cmd.get('cmd')}"}), flush=True)
        except Exception as e:
            print(json.dumps({"type": "error", "message": str(e)}), flush=True)


if __name__ == "__main__":
    main()
