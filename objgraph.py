import gc
import types
from typing import Literal, Union
import inspect
import functools

partial = functools.partial
del functools
from weakref import ReferenceType


def generate_object_graph(gc_objects, _not_found=object()) -> dict:
    import sys

    globals_id_to_name: dict[int, str] = {
        id(mod.__dict__): mod.__name__
        for mod in sys.modules.values()
        if mod is not None
    }

    def object_to_string(obj, _id):

        if isinstance(obj, (int, float, str, bool, types.NoneType, inspect.Signature)):
            return str(obj)

        if callable(obj) and (
            (wrapped := getattr(obj, "__wrapped__", _not_found)) is not _not_found
        ):
            end = (
                f" at {obj.__code__.co_filename}:{obj.__code__.co_firstlineno}"
                if hasattr(obj, "__code__")
                else ""
            )
            return (
                f"<{type(obj)} wrapping {object_to_string(wrapped, id(wrapped))}{end}>"
            )
        if isinstance(obj, types.FunctionType):
            if obj.__name__ == "<lambda>":
                code = obj.__code__
                return f"<lambda {code.co_filename}:{code.co_firstlineno}>"
            return (
                f"{obj.__name__}() at "
                f"{obj.__code__.co_filename}:{obj.__code__.co_firstlineno}"
            )
        if isinstance(obj, types.MethodType):
            return str(obj)
        if isinstance(obj, partial):
            func = obj.func
            return f"<partial wrapping {object_to_string(func, id(func))}>"

        if isinstance(obj, types.ModuleType):
            f = getattr(obj, "__file__", None)
            e = f' at "{f}"' if f else ""
            return f"<module {obj.__name__}{e}>"

        if isinstance(obj, types.CellType):
            try:
                c = obj.cell_contents
                return f"<cell containing {object_to_string(c, id(c))}>"
            except ValueError:
                return f"<empty cell>"

        if isinstance(obj, (list, tuple)):
            return str(obj)
        if isinstance(obj, dict):
            if (mod_name := globals_id_to_name.get(_id, _not_found)) is not _not_found:
                return f"<module_globals {mod_name}>"
            return str(obj)

        if hasattr(obj, "__qualname__"):
            return obj.__qualname__
        if hasattr(obj, "__name__"):
            if obj.__name__ in dir(__builtins__) and obj is getattr(
                __builtins__, obj.__name__
            ):
                return obj.__name__
            return obj.__name__

        return f"Object of type: {str(type(obj))}"

    n_obj = 0
    objids = {}

    nodes = []
    edges = []

    class CM:
        @classmethod
        def foo(cls):
            pass

    classmethodtype = type(CM.foo)

    for obj in gc_objects:
        objids[id(obj)] = obj_id = n_obj
        n_obj += 1
        nodes.append(
            {
                "id": obj_id,
                "label": object_to_string(obj, obj_id),
                "type": type(obj).__qualname__,
            }
        )

        if n_obj == 1:
            nodes[0]["root"] = True

    for obj in gc_objects:
        obj_id = objids[id(obj)]
        members = inspect.getmembers(obj)  # (name, value)
        member_ids = {id(v) for _, v in members}

        referents = list(gc.get_referents(obj))
        referent_ids = {id(r) for r in referents}

        for attr_name, attr_value in members:
            if attr_name == "__doc__":
                continue
            if not id(attr_value) in objids:
                continue
            edges.append(
                {"source": obj_id, "target": objids[id(attr_value)], "label": attr_name}
            )

        # An indirect reference is a reference that's tracked by
        # the garbage collector, but doesn't show up as an attribute.
        indirect_refs = referent_ids - member_ids
        indirects = [r for r in referents if id(r) in indirect_refs]
        for indirect in indirects:
            if not id(indirect) in objids:
                continue

            label = f"Indirect Reference to {type(indirect)}"
            if isinstance(
                indirect,
                (
                    types.MemberDescriptorType,
                    types.FunctionType,
                    types.MethodType,
                    types.BuiltinFunctionType,
                    types.CellType,
                    classmethodtype,
                ),
            ):
                label = str(indirect)
            edges.append(
                {
                    "source": objids[id(obj)],
                    "target": objids[id(indirect)],
                    "label": label,
                }
            )

    for node in nodes:
        print(node["label"])

    return {"nodes": nodes, "edges": edges}


import tempfile
import os

def output_object_graph_to_json(target=None, filename=None):
    all_objects = gc.get_objects()

    if target is None:
        gc_objects = all_objects
    else:
        # Get all objects which reference the target object, directly or indirectly.
        assert not isinstance(target, ReferenceType), (
            "target must be None (for a full dump of the GC) or a ReferenceType "
            "object. Create one with weakref.ref(), and make sure not to keep "
            "any extra references around. Be very careful about not creating "
            "extra references, or you'll get a huge object graph."
        )

        gc_objects = []
        if target() is not None:
            objects_to_check = [target()]
            checked_objects = set()
            all_object_ids = {id(obj) for obj in all_objects}

            while objects_to_check:
                obj = objects_to_check.pop()
                if id(obj) not in checked_objects:
                    checked_objects.add(id(obj))
                    gc_objects.append(obj)
                    referrers = [
                        r for r in gc.get_referrers(obj) if id(r) in all_object_ids
                    ]
                    objects_to_check.extend(referrers)

    print(f"Found {len(gc_objects)} objects in the GC")
    print(gc_objects)

    import json

    graph = generate_object_graph(gc_objects)

    if filename is None:
        temp_file = tempfile.NamedTemporaryFile(delete=False, suffix='.json')
        filename = temp_file.name
        temp_file.close()

    with open(filename, "w") as f:
        json.dump(graph, f, indent=2)
    
    print(f"Object graph has been saved to {filename}")
    return filename


def graph_viewer(
    target: Union[ReferenceType, Literal["pytorch"], None] = None,
    output_file: Union[str, None] = None,
):
    try:
        import graph_viewer
    except ImportError:
        import sys

        print(
            "Please put the graph_viewer shared object, dylib, or dll in the proper place.",
            file=sys.stderr,
        )
        return

    json_file = output_object_graph_to_json(target, output_file)
    graph_viewer.run_graph_viewer(json_file)

    if output_file is None:
        os.unlink(json_file)  # Remove the temporary file


# Example usage
if __name__ == "__main__":

    class Needle:
        def __str__(self):
            return "Needle Object"

    needle = Needle()
    del Needle

    # Create a weak reference to the needle
    needle_ref = ReferenceType(needle)

    # Create a container that holds the needle
    haystack = [{"a": tuple}, tuple, [needle], (lambda x: x), 123, "hello"]

    # Remove the original reference to needle
    del needle

    # Now haystack is the only object that holds a reference to the Needle object
    output_object_graph_to_json(needle_ref)
