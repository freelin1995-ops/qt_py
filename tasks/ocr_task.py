from ui_common import BaseForm, BoolField, TextField, EnumField, FileField, watch


class OCRRecognitionForm(BaseForm):
    task_type = "ocr_recognition"
    description = "OCR 文字识别引擎：支持多语言、图片预处理与多种输出格式。"
    schematic = "images/ocr_schematic.png"

    input_image = FileField(label="输入图片", default="", file_filter="Image Files (*.png *.jpg *.jpeg *.bmp *.tiff);;All Files (*)")
    language = EnumField(label="识别语言", options=["中文", "English", "中文 + English", "日本語", "한국어"], default="中文 + English")
    preprocess = EnumField(label="预处理", options=["无", "二值化", "降噪", "倾斜校正", "自动增强"], default="自动增强")
    psm_mode = EnumField(label="版面分析", options=["自动", "单文本块", "单列文本", "单字", "稀疏文本"], default="自动")
    output_format = EnumField(label="输出格式", options=["纯文本 (TXT)", "Word (DOCX)", "可搜索 PDF", "字幕 (SRT)"], default="纯文本 (TXT)")
    keep_layout = BoolField(label="保留布局", default=True)
    dictionary = BoolField(label="词典纠错", default=True)
    page_number = BoolField(label="输出页码", default=False)

    @watch("output_format")
    def on_format_change(self, fmt: str):
        self.keep_layout.set_visible("TXT" not in fmt)
        self.page_number.set_visible(fmt != "字幕 (SRT)")

    @watch("language")
    def on_lang_change(self, lang: str):
        self.dictionary.set_visible("中文" in lang or "English" in lang)

    @watch("preprocess")
    def on_preprocess_change(self, pp: str):
        if pp == "二值化":
            self.keep_layout.set_value(False)

    def export_meta(self):
        meta = super().export_meta()
        meta["type"] = "ocr_recognition"
        return meta


def get_form_instance():
    return OCRRecognitionForm()
