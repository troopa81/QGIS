# -*- coding: utf-8 -*-
"""QGIS Unit tests for the postgres project storage.

Note: to prepare the DB, you need to run the sql files specified in
tests/testdata/provider/testdata_pg.sh

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

"""
from builtins import next

__author__ = 'Martin Dobias'
__date__ = '2018-03-29'
__copyright__ = 'Copyright 2018, The QGIS Project'

import qgis  # NOQA
import psycopg2

import os
import time

from qgis.core import (
    QgsApplication,
    QgsDataSourceUri,
    QgsVectorLayer,
    QgsProject,
)
from PyQt5.QtCore import QDateTime, QUrl, QUrlQuery
from qgis.testing import start_app, unittest
from utilities import unitTestDataPath
from projectstoragetestbase import ProjectStorageTestCase

QGISAPP = start_app()
TEST_DATA_DIR = unitTestDataPath()


class TestPyQgsProjectStoragePostgres(unittest.TestCase, ProjectStorageTestCase):

    @classmethod
    def setUpClass(cls):
        """Run before all tests"""
        cls.dbconn = 'service=qgis_test'
        if 'QGIS_PGTEST_DB' in os.environ:
            cls.dbconn = os.environ['QGIS_PGTEST_DB']
        cls.ds_uri = QgsDataSourceUri(cls.dbconn)
        cls.scheme = "postgresql"
        cls.provider_lib = "postgres"
        cls.schema = "qgis_test"

        # Create test layers
        cls.vl = QgsVectorLayer(cls.dbconn + ' sslmode=disable key=\'pk\' srid=4326 type=POINT table="qgis_test"."someData" (geom) sql=', 'test', 'postgres')
        assert cls.vl.isValid()
        cls.con = psycopg2.connect(cls.dbconn)

    @classmethod
    def tearDownClass(cls):
        """Run after all tests"""

    def execSQLCommand(self, sql):
        self.assertTrue(self.con)
        cur = self.con.cursor()
        self.assertTrue(cur)
        cur.execute(sql)
        cur.close()
        self.con.commit()

    def dropProjectsTable(self):
        self.execSQLCommand("DROP TABLE IF EXISTS qgis_test.qgis_projects;")


if __name__ == '__main__':
    unittest.main()
