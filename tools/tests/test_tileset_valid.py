from tools.validate_tileset import validate


def test_curation_complete():
    errs = validate()
    assert errs == [], "Curation incomplete:\n" + "\n".join(errs)
