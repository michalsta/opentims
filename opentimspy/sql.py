import numpy as np
import sqlite3


def _sql2list(path, query):
    with sqlite3.connect(path) as conn:
        c = conn.cursor()
        return list(c.execute(query))


def tables_names(path):
    """List names of tables in the SQLite db.

    Arguments:
        path (str): Path to the sqlite db.

    Returns:
        list: Names of tables in 'analysis.tdf'.
    """
    sql = "SELECT name FROM sqlite_master WHERE TYPE = 'table'"
    return [name_tup[0] for name_tup in _sql2list(path, sql)]


def table2dict(path, name):
    """Retrieve a dictionary from a table with a given name.

    This function is simply great for injection attacks (:
    Don't do that.

    Args:
        name (str): Name of the table to extract.
    Returns:
        dict: Maps column name to a list of values.
    """
    assert name in tables_names(path), f"Table '{name}' is not in the database."
    _,colnames,_,_,_,_ = zip(*_sql2list(path, f"PRAGMA table_info({name});"))
    colvalues = zip(*_sql2list(path, f"SELECT * FROM {name}"))
    return dict(zip(colnames, (np.array(values) for values in colvalues)))

