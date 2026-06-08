from tools.catalog import id_of, guess_category


def test_id_of():
    assert id_of("a/b/GTASPR1_1000.bmp") == 1000
    assert id_of("GTATIL1_7.bmp") == 7


def test_guess_category():
    assert guess_category("tiles", 64, 64, 5) == "tile"
    assert guess_category("tiles", 32, 32, 5) == "tile_other"
    assert guess_category("sprites", 220, 18, 2) == "banner"
    assert guess_category("sprites", 24, 24, 1000) == "fx_explosion"
    assert guess_category("sprites", 20, 24, 5) == "small_object_or_ped"
    assert guess_category("sprites", 60, 50, 5) == "sprite"
