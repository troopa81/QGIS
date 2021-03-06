# -*- coding: utf-8 -*-
"""QGIS Unit tests for QgsProviderRegistry.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Nyall Dawson'
__date__ = '16/03/2020'
__copyright__ = 'Copyright 2020, The QGIS Project'

import qgis  # NOQA
import os

from qgis.core import (
    QgsProviderRegistry,
    QgsMapLayerType,
    QgsWkbTypes,
    QgsProviderSublayerDetails,
    Qgis,
    QgsCoordinateTransformContext,
    QgsVectorLayer
)
from qgis.testing import start_app, unittest
from utilities import unitTestDataPath

# Convenience instances in case you may need them
# to find the srs.db
start_app()


class TestQgsProviderSublayerDetails(unittest.TestCase):

    def testGettersSetters(self):
        """
        Test provider list
        """
        d = QgsProviderSublayerDetails()
        d.setProviderKey('key')
        self.assertEqual(d.providerKey(), 'key')

        d.setType(QgsMapLayerType.MeshLayer)
        self.assertEqual(d.type(), QgsMapLayerType.MeshLayer)

        d.setUri('some uri')
        self.assertEqual(d.uri(), 'some uri')

        d.setName('name')
        self.assertEqual(d.name(), 'name')

        d.setDescription('desc')
        self.assertEqual(d.description(), 'desc')

        d.setPath(['a', 'b', 'c'])
        self.assertEqual(d.path(), ['a', 'b', 'c'])

        self.assertEqual(d.featureCount(), Qgis.FeatureCountState.UnknownCount)
        d.setFeatureCount(1000)
        self.assertEqual(d.featureCount(), 1000)

        self.assertEqual(d.wkbType(), QgsWkbTypes.Unknown)
        d.setWkbType(QgsWkbTypes.Point)
        self.assertEqual(d.wkbType(), QgsWkbTypes.Point)

        d.setGeometryColumnName('geom_col')
        self.assertEqual(d.geometryColumnName(), 'geom_col')

        d.setLayerNumber(13)
        self.assertEqual(d.layerNumber(), 13)

    def test_to_layer(self):
        """
        Test converting sub layer details to a layer
        """
        details = QgsProviderSublayerDetails()
        details.setUri(os.path.join(unitTestDataPath(), 'lines.shp'))
        details.setName('my sub layer')
        details.setType(QgsMapLayerType.VectorLayer)
        details.setProviderKey('ogr')

        options = QgsProviderSublayerDetails.LayerOptions(QgsCoordinateTransformContext())
        ml = details.toLayer(options)
        self.assertTrue(ml.isValid())
        self.assertIsInstance(ml, QgsVectorLayer)
        self.assertEqual(ml.name(), 'my sub layer')


if __name__ == '__main__':
    unittest.main()
