# -*- coding: utf-8 -*-

"""
***************************************************************************
    __init__.py
    ---------------------
    Date                 : February 2016
    Copyright            : (C) 2016 by Jürgen E. Fischer
    Email                : jef at norbit dot de
***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************
"""

__author__ = 'Denis Rouzaud'
__date__ = 'May 2021'
__copyright__ = '(C) 2021, Denis Rouzaud'

import warnings
from PySide2.uic.Compiler import indenter, compiler
from PySide2.uic.objcreator import widgetPluginPath
from PySide2.uic import properties, uiparser, Compiler
from PySide2.uic import *

__PyQtLoadUiType = loadUiType


def __loadUiType(*args, **kwargs):
    with warnings.catch_warnings():
        warnings.filterwarnings("ignore", category=DeprecationWarning)
        return __PyQtLoadUiType(*args, **kwargs)


loadUiType = __loadUiType
