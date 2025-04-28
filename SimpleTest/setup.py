from setuptools import setup, Extension
from pathlib import Path

this_dir = Path(__file__).parent
parent_dir = this_dir.parent

setup(
    name="simple_env",
    ext_modules=[
        Extension(
            "simple_env",
            sources=[str(this_dir / "SimpleEnv.cpp")],
            include_dirs=[str(parent_dir / "Libraries" / "pybind11" / "include")],
            language="c++",
        ),
    ],
)
