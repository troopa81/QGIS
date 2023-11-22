__all__ = ['core']

# Preload PySide libraries to avoid missing libraries while loading our module
try:
    from PySide6 import QtCore
except Exception:
    print("Failed to load PySide")
    raise
