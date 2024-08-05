import gc

import types
import inspect


class CM:
    @classmethod
    def foo(cls):
        pass


classmethodtype = type(CM.foo)
del CM


def object_to_string(obj):
    if hasattr(obj, "__qualname__"):
        return obj.__qualname__
    if hasattr(obj, "__name__"):
        return obj.__name__
    if isinstance(obj, (int, float, str, bool, type(None))):
        return str(obj)
    if isinstance(obj, (list, tuple)):
        return str([object_to_string(item) for item in obj])
    if isinstance(obj, dict):
        return str({str(k): object_to_string(v) for k, v in list(obj.items())})
    return str(type(obj))


def get_object_name(obj):
    
    if hasattr(obj, "__class__"):
        class_name = obj.__class__.__name__
        if isinstance(obj, (list, tuple, set)):
            return f"{class_name}[{len(obj)}]"
        elif isinstance(obj, dict):
            return f"{class_name}{{{len(obj)}}}"
        elif isinstance(obj, str):
            return (
                f"{class_name}[{obj[:20]}...]"
                if len(obj) > 20
                else f"{class_name}[{obj}]"
            )
        else:
            return class_name
    else:
        return str(type(obj))


def generate_object_graph():
    n_obj = 0
    objids = {}

    nodes = []
    edges = []
    objects = gc.get_objects()

    assert classmethodtype is not None

    for obj in objects:
        objids[id(obj)] = obj_id = n_obj
        n_obj += 1
        nodes.append(
            {
                "id": obj_id,
                "label": object_to_string(obj),
                "type": type(obj).__qualname__,
            }
        )

    for obj in objects:
        obj_id = objids[id(obj)]
        members = inspect.getmembers(obj)  # (name, value)
        member_ids = {id(v) for _, v in members}

        referents = list(gc.get_referents(obj))
        referent_ids = {id(r) for r in referents}

        for attr_name, attr_value in members:
            if not id(attr_value) in objids:
                continue
            edges.append(
                {"source": obj_id, "target": objids[id(attr_value)], "label": attr_name}
            )

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


def output_object_graph_to_json(filename="object_graph.json"):
    graph = generate_object_graph()

    import json

    with open(filename, "w") as f:
        json.dump(graph, f, indent=2)
    print(f"Object graph has been saved to {filename}")


# Example usage
if __name__ == "__main__":
    output_object_graph_to_json()
