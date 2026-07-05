from ui_common import BaseForm, BoolField, TextField, EnumField, IntField, watch


class IDontKnow(BaseForm):
    task_type = "i_dont_know"

    boolTest = BoolField(label="测试bool", default=False)
    stringTest = TextField(label="测试文本", default="test")

    enumTest = EnumField(label="EnumTest", options=["1", "2"], default="1")
    intTest = IntField(label="IntTest", default=80, min_val=1, max_val=100)

    @watch("boolTest")
    def on_bool_test_change(self, is_checked: bool):
        self.stringTest.set_visible(is_checked)

    @watch("enumTest")
    def on_enumTestchange(self, mode: str):
        if mode == "1":
            self.intTest.set_enabled(True)
        else:
            self.intTest.set_enabled(False)


def get_form_instance():
    return IDontKnow()

