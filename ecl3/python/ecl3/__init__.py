from . import core

class Summary(object):
    pass

def summary(path):
    stream = core.stream(path)
    summary = Summary()

    kws = { kw.keyword.strip(): kw.values for kw in stream.keywords() }

    dimens = kws['DIMENS']
    summary.nlist = dimens[0]
    summary.gridshape = (dimens[1], dimens[2], dimens[3])
    summary.istar = dimens[5]

    summary.keywords = [kw.strip() for kw in kws['KEYWORDS']]
    summary.wgnames = [kw.strip() for kw in kws['WGNAMES']]

    summary.index = kws

    return summary
