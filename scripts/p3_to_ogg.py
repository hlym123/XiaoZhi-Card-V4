#!/usr/bin/env python3
"""
将 P3 音效转为项目使用的 OGG（libopus 16k 单声道），并可选复制到 assets/common。

用法:
  python scripts/p3_to_ogg.py click.p3 startup.p3
  python scripts/p3_to_ogg.py --copy click.p3 startup.p3   # 转换后复制到 main/assets/common/

依赖: p3_tools 的 convert_p3_to_audio（P3->WAV）, ffmpeg（WAV->OGG）
"""
import os
import sys
import subprocess
import tempfile

# 项目根目录
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
P3_TOOLS = os.path.join(SCRIPT_DIR, "p3_tools")
COMMON_OGG_DIR = os.path.join(PROJECT_DIR, "main", "assets", "common")


def p3_to_wav(p3_path, wav_path):
    """P3 -> WAV，调用 p3_tools/convert_p3_to_audio.py"""
    convert_script = os.path.join(P3_TOOLS, "convert_p3_to_audio.py")
    if not os.path.exists(convert_script):
        raise FileNotFoundError(f"未找到 {convert_script}，请确认 p3_tools 存在")
    ret = subprocess.run(
        [sys.executable, convert_script, os.path.abspath(p3_path), os.path.abspath(wav_path)],
        cwd=P3_TOOLS,
        capture_output=True,
        text=True,
    )
    if ret.returncode != 0:
        raise RuntimeError(f"P3->WAV 失败: {ret.stderr or ret.stdout}")


def wav_to_ogg(wav_path, ogg_path):
    """WAV -> OGG，与 ogg_converter 一致：libopus 16k 单声道 16kHz frame_duration=60"""
    ret = subprocess.run(
        [
            "ffmpeg", "-y",
            "-i", wav_path,
            "-c:a", "libopus", "-b:a", "16k", "-ac", "1", "-ar", "16000",
            "-frame_duration", "60",
            ogg_path,
        ],
        capture_output=True,
        text=True,
    )
    if ret.returncode != 0:
        raise RuntimeError(f"WAV->OGG 失败: {ret.stderr or ret.stdout}")


def p3_to_ogg(p3_path, ogg_path):
    """P3 -> 临时 WAV -> OGG"""
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
        wav_path = f.name
    try:
        p3_to_wav(p3_path, wav_path)
        wav_to_ogg(wav_path, ogg_path)
    finally:
        if os.path.exists(wav_path):
            os.unlink(wav_path)


def main():
    import argparse
    parser = argparse.ArgumentParser(description="P3 转 OGG（项目用 libopus 格式）")
    parser.add_argument("files", nargs="+", help="P3 文件，如 click.p3 startup.p3")
    parser.add_argument("--copy", action="store_true", help="转换后复制到 main/assets/common/")
    parser.add_argument("-o", "--output-dir", default=None, help="输出目录，默认当前目录")
    args = parser.parse_args()

    out_dir = args.output_dir or os.getcwd()
    if args.copy:
        out_dir = COMMON_OGG_DIR
        os.makedirs(out_dir, exist_ok=True)
        print(f"输出到: {out_dir}")

    for p3_path in args.files:
        if not os.path.exists(p3_path):
            print(f"跳过（不存在）: {p3_path}")
            continue
        base = os.path.splitext(os.path.basename(p3_path))[0]
        ogg_path = os.path.join(out_dir, f"{base}.ogg")
        try:
            print(f"转换: {p3_path} -> {ogg_path}")
            p3_to_ogg(p3_path, ogg_path)
            print(f"  完成: {ogg_path}")
        except Exception as e:
            print(f"  失败: {e}")

    if args.copy:
        print("\n已将 OGG 放入 assets/common，重新编译后 gen_lang.py 会生成 Lang::Sounds::OGG_<NAME>（如 OGG_CLICK、OGG_STARTUP）")


if __name__ == "__main__":
    main()
