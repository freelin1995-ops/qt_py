from ui_common import BaseForm, BoolField, TextField, EnumField, IntField, FileField, watch


class VideoTranscodeForm(BaseForm):
    task_type = "video_transcode"
    description = "视频转码工具，支持多种编码格式与硬件加速，可自定义分辨率与码率。"
    schematic = "images/video_schematic.png"

    input_file = FileField(label="源文件", default="", file_filter="Video Files (*.mp4 *.avi *.mkv *.mov);;All Files (*)")
    output_format = EnumField(label="输出格式", options=["MP4", "AVI", "MKV", "MOV"], default="MP4")
    video_codec = EnumField(label="视频编码", options=["H.264", "H.265/HEVC", "AV1", "VP9"], default="H.264")
    resolution = EnumField(label="分辨率", options=["原始", "1080p", "720p", "480p"], default="原始")
    video_bitrate = EnumField(label="视频码率", options=["自动", "2 Mbps", "5 Mbps", "10 Mbps", "20 Mbps"], default="自动")
    audio_bitrate = EnumField(label="音频码率", options=["128 kbps", "192 kbps", "320 kbps"], default="192 kbps")
    fps = IntField(label="帧率", default=30, min_val=1, max_val=120)
    hw_acceleration = BoolField(label="硬件加速", default=False)
    subtitle_burn = BoolField(label="烧录字幕", default=False)

    @watch("hw_acceleration")
    def on_hw_change(self, enabled: bool):
        if enabled:
            self.video_codec.set_value("H.264")
            self.video_codec.set_visible(False)
        else:
            self.video_codec.set_visible(True)

    @watch("output_format")
    def on_format_change(self, fmt: str):
        if fmt == "AVI":
            self.video_codec.set_value("H.264")
            self.hw_acceleration.set_value(False)
            self.hw_acceleration.set_enabled(False)
        else:
            self.hw_acceleration.set_enabled(True)

    @watch("resolution")
    def on_resolution_change(self, res: str):
        if res == "480p":
            self.video_bitrate.set_value("2 Mbps")
        elif res == "720p":
            self.video_bitrate.set_value("5 Mbps")
        elif res == "1080p":
            self.video_bitrate.set_value("10 Mbps")


def get_form_instance():
    return VideoTranscodeForm()
