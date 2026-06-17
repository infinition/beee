import io
import numpy as np
from PIL import Image, ImageSequence

def convert_to_rgb565(img: Image.Image) -> bytes:
    """Convert a PIL Image to raw RGB565 little-endian bytes (vectorise numpy).

    ~50-100x plus rapide que la boucle pixel par pixel : c'etait LE goulot qui
    rendait l'upload de GIF tres lent.
    """
    img = img.convert("RGB")
    arr = np.asarray(img, dtype=np.uint16)  # forme (H, W, 3)

    r = (arr[:, :, 0] >> 3) & 0x1F
    g = (arr[:, :, 1] >> 2) & 0x3F
    b = (arr[:, :, 2] >> 3) & 0x1F

    val = (r << 11) | (g << 5) | b          # RGB565, ligne par ligne (row-major)
    # '<u2' = little-endian uint16 (l'ESP32 est little-endian -> cast direct en uint16_t*)
    return val.astype('<u2').tobytes()

def process_gif_or_image(file_data: bytes, target_w: int = 172, target_h: int = 320):
    """
    Process image or GIF from binary data.
    Returns:
        frames: List of bytes, each containing a 172x320 RGB565 frame
        duration_ms: Frame delay in milliseconds
    """
    img = Image.open(io.BytesIO(file_data))
    
    frames = []
    durations = []
    
    for frame in ImageSequence.Iterator(img):
        # 1. Calculate smart crop (center crop to maintain aspect ratio)
        orig_w, orig_h = frame.size
        target_aspect = target_w / target_h
        orig_aspect = orig_w / orig_h
        
        if orig_aspect > target_aspect:
            # Image is too wide - crop sides
            new_w = int(orig_h * target_aspect)
            offset = (orig_w - new_w) // 2
            box = (offset, 0, offset + new_w, orig_h)
        else:
            # Image is too tall - crop top/bottom
            new_h = int(orig_w / target_aspect)
            offset = (orig_h - new_h) // 2
            box = (0, offset, orig_w, offset + new_h)
            
        cropped = frame.crop(box)
        resized = cropped.resize((target_w, target_h), Image.Resampling.LANCZOS)
        
        # Convert frame to RGB565
        frames.append(convert_to_rgb565(resized))
        
        # Get frame delay
        duration = frame.info.get('duration', 100) # Default to 100ms (10fps)
        if duration == 0:
            duration = 100
        durations.append(duration)
        
    avg_duration = sum(durations) // len(durations) if durations else 100
    
    return frames, avg_duration
