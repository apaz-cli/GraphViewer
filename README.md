# GraphViewer
For viewing huge object graphs, and finding reference cycles.


## Installation
```bash
# Pending acceptance to PyPI
pip install objgraph_viewer
```

```python
# Let's create some objects to visualize.
class Needle:
    def __str__(self):
        return "Needle Object"

needle = Needle()
del Needle

# Create a weak reference to the needle
needle_ref = ReferenceType(needle)

# Create a container that holds the needle
haystack = [{"a": tuple}, tuple, [needle], (lambda x: x), 123, "hello"]

# Remove the original reference to needle.
# Now "haystack" is the only object that holds a reference to the Needle object.
del needle
```
```python
# Now, let's use the module to visualize the objects.
import graph_viewer

# Visualize the object graph using a GUI
graph_viewer.collect_and_view(needle_ref)

# Or export to JSON file to view it elsewhere.
graph_viewer.collect_to_json(needle_ref, output_file="graph.json")
graph_viewer.view_json("graph.json")
```