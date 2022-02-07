__all__ = ['core']

# Preload PySide2 libraries to avoid missing libraries while loading our module
try:
    from PySide2 import QtCore
except Exception:
    print("Failed to load PySide")
    raise
