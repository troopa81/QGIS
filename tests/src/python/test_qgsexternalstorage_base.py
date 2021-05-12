# -*- coding: utf-8 -*-
"""QGIS Base Unit tests for QgsExternalStorage API

External storage backend must implement a test based on TestPyQgsExternalStorageBase

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""

__author__ = 'Julien Cabieces'
__date__ = '31/03/2021'
__copyright__ = 'Copyright 2021, The QGIS Project'

from shutil import rmtree
import os
import tempfile
import time

from utilities import unitTestDataPath, waitServer

from qgis.PyQt.QtCore import QCoreApplication, QEventLoop, QUrl

from qgis.core import (
    QgsApplication,
    QgsAuthMethodConfig,
    QgsExternalStorageFetchedContent)

from qgis.testing import (
    start_app,
    unittest,
)


class TestPyQgsExternalStorageBase():

    storageType = None
    url = None

    @classmethod
    def setUpClass(cls):
        """Run before all tests:"""
        QCoreApplication.setOrganizationName("QGIS_Test")
        QCoreApplication.setOrganizationDomain(cls.__name__)
        QCoreApplication.setApplicationName(cls.__name__)
        start_app()

        cls.nb_errors = 0

        cls.authm = QgsApplication.authManager()
        assert not cls.authm.isDisabled(), cls.authm.disabledMessage()

        cls.auth_config = QgsAuthMethodConfig("Basic")
        cls.auth_config.setConfig('username', "qgis")
        cls.auth_config.setConfig('password', "myPasswd!")
        cls.auth_config.setName('test_basic_auth_config')
        assert(cls.authm.storeAuthenticationConfig(cls.auth_config)[0])
        assert cls.auth_config.isValid()

        registry = QgsApplication.instance().externalStorageRegistry()
        assert registry

        cls.storage = registry.externalStorageFromType(cls.storageType)
        assert cls.storage

    @classmethod
    def tearDownClass(cls):
        """Run after all tests"""
        pass

    def setUp(self):
        """Run before each test."""
        pass

    def tearDown(self):
        """Run after each test."""
        pass

    def getNewFile(self, content):
        """Return a newly created temporary file with content"""
        f = tempfile.NamedTemporaryFile(suffix='.txt')
        f.write(content)
        f.flush()
        return f

    def checkContent(self, file_path, content):
        """Check that file content matches given content"""
        f = open(file_path, 'r')
        self.assertTrue(f.read(), b"New content")
        f.close()

    def on_error(self, msg):
        self.nb_errors = self.nb_errors + 1

    def testStoreFetchFile(self):
        """
        Test file storing and fetching
        """

        f = self.getNewFile(b"New content")

        # store
        storedContent = self.storage.store(f.name, QUrl(self.url + "/" + os.path.basename(f.name)), self.auth_config.id())
        self.assertTrue(storedContent)

        self.nb_errors = 0
        storedContent.errorOccurred.connect(self.on_error)

        loop = QEventLoop()
        storedContent.stored.connect(loop.quit)
        loop.exec()

        self.assertEqual(self.nb_errors, 0)

        # fetch
        fetchedContent = self.storage.fetch(QUrl(self.url + "/" + os.path.basename(f.name)), self.auth_config.id())
        self.assertTrue(fetchedContent)
        self.assertEqual(fetchedContent.status(), QgsExternalStorageFetchedContent.OnGoing)

        loop = QEventLoop()
        fetchedContent.fetched.connect(loop.quit)
        loop.exec()

        self.assertEqual(fetchedContent.status(), QgsExternalStorageFetchedContent.Finished)
        self.assertTrue(fetchedContent.filePath())
        self.checkContent(fetchedContent.filePath(), b"New content")
        self.assertEqual(os.path.splitext(fetchedContent.filePath())[1], '.txt')

        # fetch again, should be cached
        fetchedContent = self.storage.fetch(QUrl(self.url + "/" + os.path.basename(f.name)), self.auth_config.id())
        self.assertTrue(fetchedContent)
        self.assertEqual(fetchedContent.status(), QgsExternalStorageFetchedContent.Finished)

        self.assertTrue(fetchedContent.filePath())
        self.checkContent(fetchedContent.filePath(), b"New content")
        self.assertEqual(os.path.splitext(fetchedContent.filePath())[1], '.txt')

        # fetch bad url
        fetchedContent = self.storage.fetch(QUrl(self.url + "/error"), self.auth_config.id())
        self.assertTrue(fetchedContent)
        self.assertEqual(fetchedContent.status(), QgsExternalStorageFetchedContent.OnGoing)

        loop = QEventLoop()
        fetchedContent.fetched.connect(loop.quit)
        loop.exec()

        self.assertEqual(fetchedContent.status(), QgsExternalStorageFetchedContent.Failed)
        self.assertTrue(fetchedContent.errorString())
        self.assertFalse(fetchedContent.filePath())

    def testStoreBadUrl(self):
        """
        Test file storing with a bad url
        """

        f = self.getNewFile(b"New content")

        storedContent = self.storage.store(f.name, QUrl("http://nothinghere/" + os.path.basename(f.name)), self.auth_config.id())
        self.assertTrue(storedContent)

        self.nb_errors = 0
        storedContent.errorOccurred.connect(self.on_error)

        loop = QEventLoop()
        storedContent.stored.connect(loop.quit)
        loop.exec()

        self.assertEqual(self.nb_errors, 1)

    def testStoreMissingAuth(self):
        """
        Test file storing with missing authentification
        """

        f = self.getNewFile(b"New content")
        storedContent = self.storage.store(f.name, QUrl(self.url + "/" + os.path.basename(f.name)))
        self.assertTrue(storedContent)

        self.nb_errors = 0
        storedContent.errorOccurred.connect(self.on_error)

        loop = QEventLoop()
        storedContent.stored.connect(loop.quit)
        loop.exec()

        self.assertEqual(self.nb_errors, 1)
