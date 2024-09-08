import gc
import types
from typing import Literal, Union
from weakref import ReferenceType


TargetType = Union[ReferenceType, Literal["pytorch"], None]


def generate_object_graph(gc_objects: list[object]) -> dict:

    import inspect
    import sys
    import abc
    import functools
    from functools import partial
    import itertools
    import contextlib
    import collections
    import reprlib
    import operator

    _not_found = object()

    def module_types(module: types.ModuleType) -> set[int]:
        return {id(value) for value in vars(module).values() if type(value) is type}

    known_modules = [functools, contextlib, collections, itertools, reprlib, operator]
    known_module_types = set(itertools.chain(*[module_types(m) for m in known_modules]))

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

        if isinstance(obj, types.FrameType):
            return f"<frame {obj.f_code.co_name} at {obj.f_code.co_filename}>"

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

        if isinstance(obj, types.CodeType):
            return f"<code for {obj.co_name} at {obj.co_filename}:{obj.co_firstlineno}>"

        if isinstance(obj, (dict, types.MappingProxyType)):
            if (mod_name := globals_id_to_name.get(_id, _not_found)) is not _not_found:
                return f"<module_globals {mod_name}>"
            return str(obj)

        if isinstance(obj, ReferenceType):
            return f"<weakref to {object_to_string(obj(), id(obj()))}>"

        if isinstance(obj, types.WrapperDescriptorType):
            return str(obj)

        if isinstance(obj, types.MethodDescriptorType):
            return str(obj)

        if isinstance(obj, types.BuiltinFunctionType):
            return str(obj)

        if isinstance(obj, abc.ABCMeta):
            return str(obj)

        if id(obj) in known_module_types:
            return str(obj)

        if hasattr(obj, "__qualname__"):
            return obj.__qualname__

        if hasattr(obj, "__call__") and not hasattr(obj, "__code__"):
            print(obj)
            print(type(obj))
            print()

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

    class _CM:
        @classmethod
        def foo(cls):
            pass

    classmethodtype = type(_CM.foo)

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

        # Calling getmembers() instead raises ValueError for empty cell objects.
        members = inspect.getmembers_static(obj)  # (name, value)
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

    return {"nodes": nodes, "edges": edges}


def collect_to_json(target: TargetType = None, filename: Union[str, None] = None):
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

    import json
    import tempfile

    graph = generate_object_graph(gc_objects)

    if filename is None:
        temp_file = tempfile.NamedTemporaryFile(delete=False, suffix=".json")
        filename = temp_file.name
        temp_file.close()

    assert isinstance(filename, str), "filename must be a string."
    with open(filename, "w") as f:
        json.dump(graph, f, indent=2)

    print(f"Object graph has been saved to {filename}")
    return filename


def view_json(filename: str):
    import graph_viewer

    graph_viewer.run_graph_viewer(filename)


def collect_and_view(
    target: TargetType = None,
    output_file: Union[str, None] = None,
):
    """
    Visualize the object graph.
    """

    json_file = collect_to_json(target, output_file)

    try:
        import graph_viewer

        print("Opening graph viewer...")
        graph_viewer.run_graph_viewer(json_file)
        print("Done.")
    except ImportError:
        import sys

        print(
            "Please put the graph_viewer shared object, dylib, or dll in the proper place.",
            file=sys.stderr,
        )
        return


if __name__ == "__main__":
    collect_and_view()
