import numpy as np
import sqlite3
from collections import namedtuple

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

def table2keyed_dict(connection, tblname):
    """Retrieve a dictionary from a table with a given name.
    Do not feed as tblname anything coming from an untrusted source.

    Args:
        connection : An open sqlite3 connection
        name (str): Name of the table to extract.
    Returns:
        dict: Maps primary_key name to a list of namedtuples containing values.
    """
    sql_key = list(connection.execute("SELECT name FROM pragma_table_info(?) WHERE pk == 1", [tblname]))
    assert len(sql_key) == 1
    sql_key = sql_key[0][0]
    #other_colnames = [res[0] for res in conn.execute("SELECT name FROM pragma_table_info(?) WHERE pk == 0", [tblname])]
    cur = connection.execute("SELECT * FROM " + tblname)
    colnames = [col[0] for col in cur.description]
    sql_key_idx = colnames.index(sql_key)

    tuple_type = namedtuple(tblname+"_row", colnames)
    ret = {}
    for row in cur:
        nt = tuple_type(*row)
        ret[nt[sql_key_idx]] = nt
    return ret
