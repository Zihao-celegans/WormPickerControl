"""
    Database.py

    Direct plate info database I/O with Python.

    The database is implemented as a CSV file. Each experiment has its own
    file.
    Data format: id,strain,phenotypes,genNum,creationDate
    e.g. 1,dumpy,HERM GFP NON_RFP NORMAL ADULT : 0.54 HERM NON_GFP NON_RFP NORMAL ADULT : 0.46,0,20220118

    Yuying Fan
    Fang-Yen Lab
    Jan 2022
"""

import Phenotype
