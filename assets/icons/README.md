# SantientPHP (SPH) Icon & Branding Assets

This directory contains the official brand marks, document icons, vector graphics, and Windows shell icons for SantientPHP and `.sph` source files.

---

## Icon Catalog

| File | Format | Dimensions | Usage |
| :--- | :--- | :--- | :--- |
| **`sph.svg`** | Scalable Vector | 256×256 base | Primary brand mark with cybernetic spark and shield |
| **`sph-file.svg`** | Scalable Vector | 32×32 | Dark mode `.sph` source code document icon |
| **`sph-file-light.svg`** | Scalable Vector | 32×32 | Light mode `.sph` source code document icon |
| **`sph-manifest.svg`** | Scalable Vector | 32×32 | `sph.toml` project manifest file icon |
| **`sph.ico`** | Windows Icon | 16–256 multi-layer | Windows Explorer `.sph` file association icon |
| **`sphc.ico`** | Windows Icon | 16–256 multi-layer | `sphc.exe` native compiler executable icon |
| **`sph.png`** | Raster PNG | 256×256 | High-resolution raster brand mark |
| **`sph-icon-*.png`** | Raster PNG | 16, 24, 32, 48, 64, 128, 256 | Scaled raster icons for web, docs, and UI |

---

## Windows Explorer File Association

To display the SantientPHP icon for `.sph` files in Windows Explorer and Desktop:

```powershell
powershell -ExecutionPolicy Bypass -File assets\icons\register_sph_icon.ps1
```

---

## VS Code Extension Integration

The VS Code extension (`editors/vscode`) provides the **SantientPHP Icons** theme:
- `.sph` files automatically display `sph-file-icon.svg` (or light variant)
- `sph.toml` files display `sph-manifest-icon.svg`

To activate:
1. Open Command Palette (`Ctrl+Shift+P`)
2. Type `Preferences: File Icon Theme`
3. Select **SantientPHP Icons**

