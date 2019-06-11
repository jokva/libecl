from .. import summary

def test_load_smspec():
    s = summary('simple.smspec')

    print(s.nlist)
    print(s.gridshape)
    print(s.keywords)
    print(s.kws.keys())

    assert False
