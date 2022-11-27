'''
    PlateIndex.py

    Utilities for plate indexing
    
    Zihao Li
    Fang-Yen Lab
    Jan 2022
'''


class PlateIndex(object):
    """description of PlateIndex class"""
    '''

    PlateIndex class constructs objects containing row and column info for a plate, that can be passed to scripts,
    making the scripts can be called easier.

    '''

    def __init__(self, row = 0, col = 0):

        self.row = row
        self.col = col


