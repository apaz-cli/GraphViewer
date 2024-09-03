import graph_viewer
import json
import os

def create_test_json():
    test_data = {
        "nodes": [
            {"id": 0, "label": "Node 0"},
            {"id": 1, "label": "Node 1"},
            {"id": 2, "label": "Node 2"},
            {"id": 3, "label": "Node 3"}
        ],
        "edges": [
            {"source": 0, "target": 1, "label": "Edge 0-1"},
            {"source": 1, "target": 2, "label": "Edge 1-2"},
            {"source": 2, "target": 3, "label": "Edge 2-3"},
            {"source": 3, "target": 0, "label": "Edge 3-0"}
        ]
    }
    
    with open("test_graph.json", "w") as f:
        json.dump(test_data, f)

def test_graph_viewer():
    create_test_json()
    
    result = graph_viewer.run_graph_viewer("test_graph.json")
    
    print(f"Graph viewer returned: {result}")
    
    os.remove("test_graph.json")

if __name__ == "__main__":
    test_graph_viewer()
