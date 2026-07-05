from ui_common import BaseForm, BoolField, TextField, EnumField, IntField, watch


class ImageCompressForm(BaseForm):
    task_type = "image_compress"
    description = "压缩图片文件，支持智能质量/固定码率两种模式，可添加图片水印。"
    schematic = "images/compress_schematic.png"

    enable_watermark = BoolField(label="添加图片水印", default=False)
    watermark_text = TextField(label="水印文字", default="CopyRight")

    compress_mode = EnumField(label="压缩模式", options=["智能质量", "固定码率"], default="智能质量")
    quality = IntField(label="压缩质量", default=80, min_val=1, max_val=100)

    @watch("enable_watermark")
    def on_watermark_toggle(self, is_checked: bool):
        self.watermark_text.set_visible(is_checked)

    @watch("compress_mode")
    def on_mode_change(self, mode: str):
        if mode == "智能质量":
            self.quality.set_enabled(True)
        else:
            self.quality.set_enabled(False)


def get_form_instance():
    return ImageCompressForm()

