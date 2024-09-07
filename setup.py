from setuptools import Extension, setup


# >>> import sdl2dll
# >>> sdl2dll.get_dllpath()

VERSION = "0.1"
module_name = "objgraph_viewer"

setup(
    version=VERSION,
    description="For viewing huge object graphs, and finding reference cycles.",
    author="Aaron Pazdera",
    author_email="aarpazdera@gmail.com",
    license="MIT",
    url="https://github.com/apaz-cli/GraphViewer",
    packages=[module_name],
    ext_modules=[
        Extension(
            name=module_name,
            sources=[
                "filepicker.c",
                "graph_viewer.c",
            ],
            libraries=[
                "SDL2",
                "SDL2_image",
                "SDL2_ttf",
                "SDL2_gfx",
                "SDL2_ttf",
            ],
        ),
    ],
)
