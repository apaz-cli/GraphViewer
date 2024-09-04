from setuptools import Extension, setup

sdl2_include_dir = '/usr/include/SDL2'

setup(
    ext_modules=[
        Extension(
            name="graph_viewer",
            sources=[
                "graph_viewer.c",
            ],
            include_dirs=[sdl2_include_dir],
        ),
    ]
)
