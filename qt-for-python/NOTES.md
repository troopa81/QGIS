
# Context

This is the report of the investigation of switching to Qt-for-Python for QGIS Python bindings.
This is based on the QEP https://github.com/qgis/QGIS-Enhancement-Proposals/issues/163

# Approach

We have decided to give a trial at setting up Qt-for-Python to generate PyQGIS bindings.
All this work has been done and is available at https://github.com/opengisch/QGIS/tree/qt-for-python-qt5/qt-for-python.

A first try using PySide6 (Qt6) was started by making the code base more compliant with Qt6. This work has been merged to the main branch of the official repo, and thanks to other devs, a set of QGIS core tests is now running with Qt6 as well on CI.
Due to the incomplete state of Qt6 compatibility and amount of work required at that time, we have opted to use PySide2 (Qt5).

A global switch to enable PySide2 is available in the CMake configuration (https://github.com/opengisch/QGIS/blob/qt-for-python-qt5/CMakeLists.txt#L949). PySide2 and PyQt can live at the same time in the code base, so that Qt-for-Python can be evaluated while SIP/PyQt is still maintained.

The goal was to have the code to compile, to be able to call some PyQGIS object and run a test using the new bindings. This would allow us to estimate the effort required to migrate the bindings and spot places where the migration would be problematic.


# Quick technical summary of Qt-for-Python

Qt-for-Python is maintained by Qt Group and is available under the same licence than Qt.

Qt-for-Python includes both
* PySide being the Python bindings of the Qt libraries. Currently QGIS uses PyQt.
* shiboken being the generator of the bindings for QGIS. Currently QGIS uses SIP. The generator uses both the QGIS core headers and auxiliary files (XML typesystem files)


# Technical outcomes

## Typesystem files generation

Shiboken, Qt-for-Python's generator, relies on XML auxiliary files to produce the bindings (similarly, sip relies on sip files).

With shiboken, members are automatically detected. This means that the typesystem file only contains:
* enums
* parts of the code which require special handling (ownership, argument manipulation, code injection)

The question rising is the way to write or produce these typesystem files.
One possibility is to write them manually  as this used to be the case in the past for QGIS, before sipify was created. But it has proven to be problematic: the files were often outdated and not produced from start when developing new API endpoints.
The other approach is to produce them automatically based on the content of the header. We can either:
1. write the whole content of the XML file in the header within #ifdefs
2. Use a combination of simple macros (similarly to what is done currently, see https://github.com/qgis/QGIS/blob/master/src/core/qgis_sip.h) and #ifdefs for things like code injection. In such case, we then need a tool to produce the XML either using a non-parsing tool like sipify or a tool based on a code parser.

While the simplest approaches sound nice, they would probably prove to be too limitating, mainly for applying global changes for the API documentation for instance (automatically generating the Python docs from the C++ docs).

Qt has apparently developed a commercial GUI tool, but based on an open-source example https://code.qt.io/cgit/pyside/pyside-setup.git/tree/sources/shiboken6/tests/dumpcodemodel. We would recommend testing this tool to see if it can extended to our needs.

## Ownership

https://doc.qt.io/qtforpython/shiboken6/typesystem_ownership.html

The ownership system of qt-for-python is well-documented including "common usage" examples.

It covers scenarios like transferring ownership both directions between python/c++. It also
covers concepts like the Qt parent/child relationship.
It even offers some "heuristics" to discover parent/child relationships, it needs to be
checked how these play in the QGIS scenario, very likely they will require to rename some
parameters to `parent` and/or be made explicit.
The documentation is encouraging and the fact that it has been used to create python bindings
for the Qt frameweork as well.
QGIS already now contains very good in-code annotations of memory management thanks to the work
for SIP compatibility.

Both facts, the existing annotations as well as the apparent good support for ownership management
of qt-for-python are encouraging for a smooth transition.
However, due to the fact that the different memory management models of C++ and Python offer a
challenge for any bindings, the risk persists that additional work on the mapping of SIP to the
qt-for-python models needs to be done. It's unlikely that this will be a blocker.

## QVariant

QVariant was removed and is transparently converted to the corresponding native type.
Any function expecting it can receive any Python object (`None` is an invalid QVariant).
The same rule is valid when returning something: the returned QVariant will be converted to its original Python object type.

When a method expects a `QVariant::Type` the programmer can use a string (the type name) or the type itself.
https://pyside.github.io/docs/pyside/pysideapi2.html#qvariant

### NULL

Since QVariant was removed, it might be an opportunity to drop QGIS' `NULL` in favor of Python's None.
Inside QGIS, invalid and null QVariants are treated equally, so this would help to be more aligned with
QGIS internal handling and being more Pythonic at the same time.


## Compatibility layer

One of the goals of PySide6 is to be API compatible with PyQt, with certain exceptions (see https://doc.qt.io/qtforpython/considerations.html).

Therefore it would be easy and useful to propose a compatibility layer, similarly to what we did for PyQt5/6: i.e. `qgis.PyQt.QtCore = PySide.QtCore`.


## Missing PySide bindings

Some objects are not (yet?) part of PySide bindings, especially in PySide2.
https://wiki.qt.io/Qt_for_Python_Missing_Bindings

New types get added regularly and in the list of missing bindings, no blocking type could be identified.
The risk of this is low.

## Raising exceptions instead of returning tuples

In several places of QGIS' API, we tend to return a tuple with the result and with a boolean for the success (e.g. see `QgsUnitTypes.stringToDistanceUnit` method).
It would be more pythonic to raise Python exception instead, and only return the result.
This change would mean an API break, the opportunity could be taken to change this.

## pyuic and pyrcc

The tools pyuic and pyrcc are utilities to compile .ui and .rc files to python files.

There is the possibility to load .ui and helper files at runtime and skipping the compiling
step altogether.
There are also equivalent tools available for Qt-for-Python (pyside6-uic and pyside6-rcc).

https://doc.qt.io/qt-6/uic.html
https://doc.qt.io/qtforpython/tutorials/basictutorial/qrcfiles.html

## QScintilla

QScintilla needs to be ported to PySide. The tool has been developed by Riverbank.
We would need to also write the bindings for it, meaning probably integrating its source code within QGIS to be sure that the bindings and the source are at the same version.

## Code injection

Handwritten code is similar but different.
https://doc.qt.io/qtforpython/shiboken6/typesystem_codeinjection.html
E.g. parameter with type `const QString &` (CPP) is available as `QString *` (SIP) and `const QString &` (PySide2).
This means that handwritten code cannot just be reused and must be revised.

## Translations

There is no corresponding tool for pylupdate with Qt for Python (see https://github.com/qgis/QGIS-Enhancement-Proposals/issues/163#issuecomment-804375040 or https://bugreports.qt.io/browse/PYSIDE-1552).
Solution would be to either
* rely on PyQt6
* add support for Python in the lupdate Qt tool
* build an ad-hoc tool using gettext which can mimic what lupdate is supposed to do

We think that building an ad-hoc tool is the best approach as we are not mixing solutions and avoid a risky development.
Some tools with similar functionality already exist (https://github.com/danhper/python-i18n).

## API Documentation

The generated bindings do not contain API documentation from the C++ API. Using the typesystem generation tool, we need to translate them as we are currently doing for SIP bindings.


# Further considerations

## Community support

Gitter channel is quite active and helpful (https://gitter.im/PySide/pyside2)
We have got close support for a R&D Manager at The Qt Company who is responsible of the Qt-for-Python development.

## Documentation

Documentation is well accessible, concepts are well explained.
  - https://doc.qt.io/qtforpython/shiboken2/
  - https://doc.qt.io/qtforpython/shiboken2/typesystem.html
  - Reference documentation for the typesystem is not always up to date, we had to check the sample code in tests instead
    - https://code.qt.io/cgit/pyside/pyside-setup.git/tree/sources/shiboken6/tests/samplebinding/typesystem_sample.xml



# Integration into QGIS code base

As demonstrated during our testings, the code base can live with the two systems in parallel, allowing a continuous integration of Qt-for-Python.

A complete switch to Qt-for-Python needs to be combined with the switch to Qt6 / QGIS 4.
While we could certainly offer a compatibility layer, asking plugin developers to switch at the same time than Qt6 sounds much more reasonable.
Also, the bindings are less complete under Qt5 (for instance QSignalSpy comes with Qt 6.1).

# Opportunities and risks to switch or stick to current solution

While writing the QEP, we identified the following reasons to evaluate moving away from PyQt:
* it's a solution developed by a single person (hitting bus factor)
* it offers very limited contribution opportunities, poor community experience (no issue tracker, no road-map)
* we have a history of issues which either took a lot of time/energy to get solved or were never solved
* Riverbank did not answered positively to 2 requests to work on issues for QGIS.org
* Qt3D and QtChart are distinct products leading to more solutions to maintain and distribute (mainly for Windows, Android, MacOS and iOS).

Qt-for-Python might offer a better community solution, better integration with Qt and a more future proof solution.

Recent discussions with the future of Qt regarding open-source shall also be taken into consideration.
But switching to Qt-for-Python is closely tied to migrating to Qt6. If the path of Qt6 remains, switching to Qt-for-Python should be safe.

The impact on plugin authors is obviously a matter of consideration.
We would recommend using the compatibility layer instead of directly importing PyQt or PySide: qgis.PyQt (or qgis.Qt) would import the proper bindings depending on the environment. This means replacing the imports in the plugin code.
Additional changes will be required for plugins case dependant but not complicated.
Common exaples would be `QVariant` usages or exceptions instead of tuple return types.
Other changes such as enum being fully qualified will be required in any case (with PyQt6 too).
And QGIS4 will probably brings some small API changes at the same time.

# Rough estimation of the work load for a switch

* Writing the generation tool ~20 days
* Merge existing core part to generate the bindings ~2 days
* Make existing headers compliant with both sipify and the new generation tool ~5 days
* Migrate handwritten code blocks ~8 days
* Handle QScintilla bindings ~5 days
* Implement the compatibility layer (NULL/None, QVariant, etc,) ~3 days
* Tests fixing ~5 days

Total: ~48 days


# What's next ?

To move on, we recommend the following approach:

1. The present report is published and feedback is collected (probably on the mailing list or in a Github issue)
2. PSC calls/nominates a technical committee of 3-6 relevant and interested developers to take a formal technical recommendation and confirm the risks and costs estimates.
3. PSC validates or rejects the technical recommendation.
4. If the switch to Qt-for-Python is validated, development should start as soon as possible and shared among several developers.

N.B.: Chances are high that people involved in the committee would also be developers participating to the migration, which is obviously a risk of neutrality. Integrating several developers from different companies should mitigate this risk.
