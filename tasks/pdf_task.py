from ui_common import BaseForm, BoolField, TextField, EnumField, IntField, FileField, watch


class PDFToolkitForm(BaseForm):
    task_type = "pdf_toolkit"
    description = "PDF 文件工具箱：合并、拆分、压缩、加密、添加水印一站式处理。"
    schematic = "images/pdf_schematic.png"

    action = EnumField(label="操作类型", options=["合并 PDF", "拆分 PDF", "压缩 PDF", "添加水印", "加密 PDF"], default="合并 PDF")

    input_files = FileField(label="输入文件", default="", file_filter="PDF Files (*.pdf);;All Files (*)")
    output_name = TextField(label="输出文件名", default="output")

    password = TextField(label="密码", default="")
    quality = EnumField(label="压缩质量", options=["高 (无损)", "中 (推荐)", "低 (小体积)"], default="中 (推荐)")
    page_range = TextField(label="页码范围", default="1-")
    watermark_text = TextField(label="水印文字", default="机密")
    keep_bookmarks = BoolField(label="保留书签", default=True)

    @watch("action")
    def on_action_change(self, action: str):
        self.page_range.set_visible(action in ("拆分 PDF",))
        self.watermark_text.set_visible(action == "添加水印")
        self.quality.set_visible(action == "压缩 PDF")
        self.password.set_visible(action == "加密 PDF")
        self.keep_bookmarks.set_visible(action in ("合并 PDF", "压缩 PDF"))

    @watch("password")
    def on_password_change(self, pw: str):
        self._encrypt = bool(pw)

    @watch("quality")
    def on_quality_change(self, q: str):
        if "低" in q:
            self.keep_bookmarks.set_value(False)
            self.keep_bookmarks.set_enabled(False)
        else:
            self.keep_bookmarks.set_enabled(True)

    def export_meta(self):
        meta = super().export_meta()
        meta["type"] = "pdf_toolkit"
        return meta


def get_form_instance():
    return PDFToolkitForm()
