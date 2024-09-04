from setuptools import Extension, setup

setup(
    ext_modules=[
        Extension(
            name="graph_viewer",
            sources=[
                "graph_viewer.c",
            ],
        ),
    ]
)
