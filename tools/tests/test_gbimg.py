from tools.gbimg import rgb565


def test_rgb565_primaries():
    assert rgb565(255, 0, 0) == 0xF800   # rouge pur
    assert rgb565(0, 255, 0) == 0x07E0   # vert pur
    assert rgb565(0, 0, 255) == 0x001F   # bleu pur
    assert rgb565(0, 0, 0) == 0x0000
    assert rgb565(255, 255, 255) == 0xFFFF


def test_rgb565_truncation():
    # 5/6/5 bits : les bits de poids faible sont jetés
    assert rgb565(7, 3, 7) == 0x0000
    assert rgb565(8, 4, 8) == ((1 << 11) | (1 << 5) | 1)
