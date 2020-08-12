# -*- coding: utf-8 -*-
"""QGIS Unit base tests for the project storage.

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

from qgis.core import (
    QgsApplication,
    QgsDataSourceUri,
    QgsVectorLayer,
    QgsProject,
)
from PyQt5.QtCore import QDateTime, QUrl, QUrlQuery


def encode_uri(scheme, ds_uri, schema_name, project_name=None):
    u = QUrl()
    urlQuery = QUrlQuery()

    u.setScheme(scheme)
    u.setHost(ds_uri.host())
    if ds_uri.port() != '':
        u.setPort(int(ds_uri.port()))
    if ds_uri.username() != '':
        u.setUserName(ds_uri.username())
    if ds_uri.password() != '':
        u.setPassword(ds_uri.password())

    if ds_uri.service() != '':
        urlQuery.addQueryItem("service", ds_uri.service())
    if ds_uri.authConfigId() != '':
        urlQuery.addQueryItem("authcfg", ds_uri.authConfigId())
    if ds_uri.sslMode() != QgsDataSourceUri.SslPrefer:
        urlQuery.addQueryItem("sslmode", QgsDataSourceUri.encodeSslMode(ds_uri.sslMode()))

    print("dbname={}".format(ds_uri.database()))

    urlQuery.addQueryItem("dbname", ds_uri.database())

    urlQuery.addQueryItem("schema", schema_name)
    if project_name:
        urlQuery.addQueryItem("project", project_name)

    u.setQuery(urlQuery)
    return str(u.toEncoded(), 'utf-8')


class ProjectStorageTestCase(object):

    def dropProjectsTable(self):
        pass

    def testSaveLoadProject(self):
        schema_uri = encode_uri(self.scheme, self.ds_uri, self.schema)
        project_uri = encode_uri(self.scheme, self.ds_uri, self.schema, 'abc')

        self.dropProjectsTable()  # make sure we have a clean start

        prj = QgsProject()
        uri = self.vl.source()
        vl1 = QgsVectorLayer(uri, 'test', self.provider_lib)
        self.assertEqual(vl1.isValid(), True)
        prj.addMapLayer(vl1)

        prj_storage = QgsApplication.projectStorageRegistry().projectStorageFromType(self.scheme)
        self.assertTrue(prj_storage)

        lst0 = prj_storage.listProjects(schema_uri)
        self.assertEqual(lst0, [])

        # try to save project in the database

        prj.setFileName(project_uri)
        res = prj.write()
        self.assertTrue(res)

        lst1 = prj_storage.listProjects(schema_uri)
        self.assertEqual(lst1, ["abc"])

        # now try to load the project back

        prj2 = QgsProject()
        prj2.setFileName(project_uri)
        res = prj2.read()
        self.assertTrue(res)

        self.assertEqual(len(prj2.mapLayers()), 1)

        self.assertEqual(prj2.baseName(), "abc")
        self.assertEqual(prj2.absoluteFilePath(), "")  # path not supported for project storages
        self.assertTrue(abs(prj2.lastModified().secsTo(QDateTime.currentDateTime())) < 10)

        # try to see project's metadata

        res, metadata = prj_storage.readProjectStorageMetadata(project_uri)
        self.assertTrue(res)
        self.assertEqual(metadata.name, "abc")
        time_project = metadata.lastModified
        time_now = QDateTime.currentDateTime()
        time_diff = time_now.secsTo(time_project)
        self.assertTrue(abs(time_diff) < 10)

        # try to remove the project

        res = prj_storage.removeProject(project_uri)
        self.assertTrue(res)

        lst2 = prj_storage.listProjects(schema_uri)
        self.assertEqual(lst2, [])

        self.dropProjectsTable()  # make sure we have a clean finish... "leave no trace"


if __name__ == '__main__':
    unittest.main()
