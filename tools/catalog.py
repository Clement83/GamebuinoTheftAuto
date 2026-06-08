"""Inventaire des BMP source -> assets/catalog.csv + planches-contact."""
import os, glob, csv
from PIL import Image, ImageDraw, ImageFont

TILES_DIR = "GTA_textures_LC/LC/tiles"
SPR_DIR = "GTA_textures_LC/LC/sprites"
OUT_CSV = "assets/catalog.csv"
CONTACT_DIR = "assets/contact"


def id_of(path):
    base = os.path.basename(path)
    digits = "".join(c for c in base.split("_")[-1] if c.isdigit())
    return int(digits or 0)


def guess_category(folder, w, h, idnum):
    if folder == "tiles":
        return "tile" if (w, h) == (64, 64) else "tile_other"
    if 992 <= idnum <= 1030:
        return "fx_explosion"
    if w >= 200 or (h and w / h >= 3):
        return "banner"
    if max(w, h) <= 28:
        return "small_object_or_ped"
    return "sprite"


def _font(sz):
    for p in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
              "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        if os.path.exists(p):
            return ImageFont.truetype(p, sz)
    return ImageFont.load_default()


def scan():
    rows = []
    for folder, d in (("tiles", TILES_DIR), ("sprites", SPR_DIR)):
        for p in sorted(glob.glob(d + "/*.bmp"), key=id_of):
            with Image.open(p) as im:
                w, h = im.size
            rows.append({"id": id_of(p), "folder": folder, "w": w, "h": h,
                         "category_guess": guess_category(folder, w, h, id_of(p)),
                         "name": ""})
    return rows


def write_csv(rows):
    os.makedirs(os.path.dirname(OUT_CSV), exist_ok=True)
    with open(OUT_CSV, "w", newline="") as f:
        wr = csv.DictWriter(f, fieldnames=["id", "folder", "w", "h",
                                           "category_guess", "name"])
        wr.writeheader()
        wr.writerows(rows)


def _sheet(files, out, cols=6, cell=96):
    font = _font(12)
    rows = (len(files) + cols - 1) // cols
    lab = 15
    canvas = Image.new("RGB", (cols * (cell + 6), rows * (cell + lab + 6)),
                       (128, 128, 128))
    d = ImageDraw.Draw(canvas)
    for i, fpath in enumerate(files):
        r, c = divmod(i, cols)
        x, y = c * (cell + 6) + 3, r * (cell + lab + 6) + 3
        with Image.open(fpath) as im:
            im = im.convert("RGB")
            w, h = im.size
            if w > cell or h > cell:
                s = min(cell / w, cell / h)
                im = im.resize((max(1, int(w * s)), max(1, int(h * s))),
                               Image.Resampling.NEAREST)
                w, h = im.size
            canvas.paste(im, (x + (cell - w) // 2, y + (cell - h) // 2))
        d.text((x, y + cell + 1), str(id_of(fpath)), fill=(0, 0, 0), font=font)
    canvas.save(out)


def contact_sheets(per_sheet=36):
    os.makedirs(CONTACT_DIR, exist_ok=True)
    for folder, d in (("tiles", TILES_DIR), ("sprites", SPR_DIR)):
        files = sorted(glob.glob(d + "/*.bmp"), key=id_of)
        for s in range(0, len(files), per_sheet):
            _sheet(files[s:s + per_sheet],
                   f"{CONTACT_DIR}/{folder}_{s // per_sheet:02d}.png")


if __name__ == "__main__":
    rows = scan()
    write_csv(rows)
    contact_sheets()
    print(f"catalog: {len(rows)} lignes -> {OUT_CSV} ; planches -> {CONTACT_DIR}/")
