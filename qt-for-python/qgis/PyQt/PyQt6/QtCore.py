# -*- coding: utf-8 -*-

"""
***************************************************************************
    QtCore.py
    ---------------------
    Date                 : November 2015
    Copyright            : (C) 2015 by Matthias Kuhn
    Email                : matthias at opengis dot ch
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

from PySide6.QtCore import *

from types import MethodType

class _QVariant(object):
    def __init__(self, param):
        pass

QVariant = _QVariant
QVariant.Int = int
QVariant.String = str
QVariant.Double = float
QVariant.DateTime = QDateTime

NULL = None
