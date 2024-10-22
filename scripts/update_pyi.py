import qgis.core
import sys
from qgis.core import *
import inspect
import re
import ast
from tokenize_rt import Offset, src_to_tokens, tokens_to_src, reversed_enumerate

# import time
# start = time.time()

# TODO
# Cmake things
# do it for all classes/modules
# do it for signal


def get_annotation_signature(_node: ast.Subscript):

    slice_str = None
    if isinstance(_node.slice, ast.Subscript):
        slice_str = get_annotation_signature(_node.slice)
    elif isinstance(_node.slice, ast.Name):
        slice_str = _node.slice.id

    return _node.value.attr + f"[{slice_str}]" if slice_str else ""


def get_function_signature(_node: ast.FunctionDef):
    arguments = []
    for a in _node.args.args:
        arguments.append(f"{a.arg}")
        if a.annotation:
            arguments[-1] += ": " + get_annotation_signature(a.annotation)

    return "(" + ", ".join(arguments) + ")"


def get_deprecated_message(documentation: str):

    deprecated_match = re.search("\\.\\. deprecated:: ([0-9.]*)(.*)", documentation, re.MULTILINE | re.DOTALL)

    if not deprecated_match:
        sys.stderr.write(f"Badly formatted deprecated instruction: {documentation}")
        exit(1)

    version = deprecated_match.group(1)
    msg = deprecated_match.group(2)
    if ".. " in msg:
        msg = msg[:msg.index(".. ")]

    # remove :py:func: and :py:class: documentation instruction
    msg = re.sub(":py:(func|class):`~{0,1}([^`]*)`", "\\2", msg)
    msg = msg.strip()
    return msg


deprecated_functions = {}

for class_name, class_object in inspect.getmembers(qgis.core, predicate=inspect.isclass):
    if class_name != "QgsProject":
        continue

    for function_name, func in inspect.getmembers(class_object):
        # signal and other things
        # if inspect.ismethoddescriptor(func):
        #     print(func)

        if inspect.isbuiltin(func) and func.__doc__ and ".. deprecated:: " in func.__doc__:

            # Check if there is several function with the same name in documentation so
            # we store their signature for later better identification

            func_matches = list(re.finditer(r"^" + function_name + r"(\(.*\)(?:$| -> .*))", func.__doc__, re.MULTILINE))
            if len(func_matches) > 1:
                messages = {}
                for i, func_match in enumerate(func_matches):
                    doc_start = func_match.end() + 1
                    doc_end = func_matches[i + 1].start() - 1 if i + 1 < len(func_matches) else None
                    doc = func.__doc__[doc_start:doc_end]
                    signature = func_match.group(1)

                    if ".. deprecated:: " in doc:
                        messages[signature] = get_deprecated_message(doc)

                deprecated_functions[(class_name, function_name)] = messages

            else:
                deprecated_functions[(class_name, function_name)] = get_deprecated_message(func.__doc__)

filename = "/home/julien/work/QGIS/build/python/core/build/_core/_core.pyi"
with open(filename, encoding='UTF-8') as f:
    contents = f.read()

need_deprecated = {}

tree = ast.parse(contents, filename=filename)
for parent in ast.walk(tree):

    for node in ast.iter_child_nodes(parent):
        if isinstance(parent, ast.ClassDef) and isinstance(node, ast.FunctionDef) and (parent.name, node.name) in deprecated_functions:

            deprecated_msg = deprecated_functions[(parent.name, node.name)]

            if isinstance(deprecated_msg, dict):
                signature = get_function_signature(node)
                if signature not in deprecated_msg:
                    print(node.name)
                    sys.stderr.write(f"Cannot find deprecated signature '{signature}' in '{deprecated_msg.keys()}'")
                    exit(1)

                deprecated_msg = deprecated_msg[get_function_signature(node)]

            need_deprecated[Offset(node.lineno, node.col_offset)] = deprecated_msg

# print(f"parse tree: {(time.time()-start)*1000}")


tokens = src_to_tokens(contents)

# print(f"src_to_tokens: {(time.time()-start)*1000}")

for i, token in reversed_enumerate(tokens):
    if token.offset in need_deprecated:
        tokens[i] = tokens[i]._replace(src=f'@deprecated("""{need_deprecated[token.offset]}""")\n{" " * token.offset.utf8_byte_offset}{tokens[i].src}')

# print(f"update tokens: {(time.time()-start)*1000}")

new_contents = tokens_to_src(tokens)
with open("/tmp/qgis.pyi", 'w') as f:
    f.write(new_contents)

# print(f"write content: {(time.time()-start)*1000}")
