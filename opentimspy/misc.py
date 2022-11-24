# create a slice object from a string
# from: https://stackoverflow.com/questions/680826/python-create-slice-object-from-string/23895339
def parse_slice(value):
    """
    Parses a `slice()` from string, like `start:stop:step`.
    """
    if value:
        parts = value.split(':')
        if len(parts) == 1:
            # slice(stop)
            parts = [None, parts[0]]
        # else: slice(start, stop[, step])
    else:
        # slice()
        parts = []
    return slice(*[int(p) if p else None for p in parts])
