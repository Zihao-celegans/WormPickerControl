"""
    Phenotype.py

    Utilities for handling worm phenotyping parameters

    Zihao Li
    Fang-Yen Lab
    Jan 2022
"""

import enum


class sex_type(enum.Enum):
    UNKNOWN_SEX = 0
    HERM = 1
    MALE = 2


class GFP_type(enum.Enum):
    UNKNOWN_GFP = 0
    GFP = 1
    NON_GFP = 2


class RFP_type(enum.Enum):
    UNKNOWN_RFP = 0
    RFP = 1
    NON_RFP = 2


class morph_type(enum.Enum):
    UNKNOWN_MORPH = 0
    NORMAL = 1
    DUMPY = 2


class stage_type(enum.Enum):
    UNKNOWN_STAGE = 0
    ADULT = 1
    L4_LARVA = 2
    L3_LARVA = 3
    L2_LARVA = 4
    L1_LARVA = 5


class Phenotype(object):
    """description of class"""

    '''

    Phenotype class constructs objects describing worm phenotype

    '''

    def __init__(self,
                 sex=sex_type.UNKNOWN_SEX,
                 gfp=GFP_type.UNKNOWN_GFP,
                 rfp=RFP_type.UNKNOWN_RFP,
                 morph=morph_type.UNKNOWN_MORPH,
                 stage=stage_type.UNKNOWN_STAGE):
        self.sex = sex
        self.gfp = gfp
        self.rfp = rfp
        self.morph = morph
        self.stage = stage
