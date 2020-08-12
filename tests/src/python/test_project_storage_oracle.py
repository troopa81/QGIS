# -*- coding: utf-8 -*-
"""QGIS Unit tests for the oracle project storage.

Note: to prepare the DB, you need to run the sql files specified in
tests/testdata/provider/testdata_oracle.sh

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

"""
from builtins import next

__author__ = 'Julien Cabieces'
__date__ = '2020-07-12'
__copyright__ = 'Copyright 2020, The QGIS Project'

import qgis  # NOQA

import os
import time

from qgis.core import (
    QgsApplication,
    QgsDataSourceUri,
    QgsVectorLayer,
    QgsProject,
)
from qgis.PyQt.QtCore import QDateTime, QUrl, QUrlQuery
from qgis.PyQt.QtSql import QSqlDatabase, QSqlQuery
from qgis.testing import start_app, unittest
from utilities import unitTestDataPath
from projectstoragetestbase import ProjectStorageTestCase

QGISAPP = start_app()
TEST_DATA_DIR = unitTestDataPath()


class TestPyQgsProjectStorageOracle(unittest.TestCase, ProjectStorageTestCase):

    @classmethod
    def setUpClass(cls):
        """Run before all tests"""
        cls.dbconn = "host=localhost port=1521 user='QGIS' password='qgis' dbname=XEPDB1"
        if 'QGIS_ORACLETEST_DB' in os.environ:
            cls.dbconn = os.environ['QGIS_ORACLETEST_DB']

        cls.ds_uri = QgsDataSourceUri(cls.dbconn)
        cls.scheme = "oracle"
        cls.provider_lib = "oracle"
        cls.schema = "QGIS"

        # Create test layers
        cls.vl = QgsVectorLayer(
            cls.dbconn + ' sslmode=disable key=\'pk\' srid=4326 type=POINT table="QGIS"."SOME_DATA" (GEOM) sql=', 'test', 'oracle')
        assert(cls.vl.isValid())

        cls.conn = QSqlDatabase.addDatabase('QOCISPATIAL', "oracletest")
        cls.conn.setDatabaseName('localhost/XEPDB1')
        if 'QGIS_ORACLETEST_DBNAME' in os.environ:
            cls.conn.setDatabaseName(os.environ['QGIS_ORACLETEST_DBNAME'])
        cls.conn.setUserName('QGIS')
        cls.conn.setPassword('qgis')
        assert cls.conn.open()

    @classmethod
    def tearDownClass(cls):
        """Run after all tests"""

    def execSQLCommand(self, sql, ignore_errors=False):
        self.assertTrue(self.conn)
        query = QSqlQuery(self.conn)
        res = query.exec_(sql)
        if not ignore_errors:
            self.assertTrue(res, sql + ': ' + query.lastError().text())
        query.finish()

    def dropProjectsTable(self):
        self.execSQLCommand("DROP TABLE qgis.qgis_projects", True)


if __name__ == '__main__':
    unittest.main()
