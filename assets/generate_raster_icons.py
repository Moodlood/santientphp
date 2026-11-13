import os
from PIL import Image

def generate_icons():
    assets_dir = os.path.join(os.path.dirname(__file__), "icons")
    os.makedirs(assets_dir, exist_ok=True)
    
    src_path = os.path.join(os.path.dirname(__file__), "..", "editors", "vscode", "icons", "icon.png")
    if not os.path.exists(src_path):
        print(f"Error: {src_path} not found")
        return
    
    base_img = Image.open(src_path).convert("RGBA")
    
    # Save standard PNG resolutions
    sizes = [256, 128, 64, 48, 32, 24, 16]
    for s in sizes:
        resized = base_img.resize((s, s), Image.Resampling.LANCZOS)
        out_png = os.path.join(assets_dir, f"sph-icon-{s}.png")
        resized.save(out_png, "PNG")
        print(f"Generated: {out_png}")
        
    # Main sph.png (256x256)
    base_img.save(os.path.join(assets_dir, "sph.png"), "PNG")
    print("Generated: sph.png")
    
    # Generate multi-layer Windows ICO for .sph file association
    ico_path = os.path.join(assets_dir, "sph.ico")
    base_img.save(
        ico_path,
        format="ICO",
        sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]
    )
    print(f"Generated multi-resolution Windows ICO: {ico_path}")

    # Compiler executable icon
    sphc_ico_path = os.path.join(assets_dir, "sphc.ico")
    base_img.save(
        sphc_ico_path,
        format="ICO",
        sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]
    )
    print(f"Generated multi-resolution Windows ICO: {sphc_ico_path}")

if __name__ == "__main__":
    generate_icons()

