[![Build status](https://ci.appveyor.com/api/projects/status/b2o8jw7xnfqghqr5?svg=true)](https://ci.appveyor.com/project/KmolYuan/solvespace)
[![Build status](https://img.shields.io/travis/KmolYuan/solvespace.svg?logo=travis)](https://travis-ci.org/KmolYuan/solvespace)
[![PyPI](https://img.shields.io/pypi/v/python-solvespace.svg)](https://pypi.org/project/python-solvespace/)
[![GitHub license](https://img.shields.io/badge/license-GPLv3+-blue.svg)](https://raw.githubusercontent.com/KmolYuan/solvespace/master/LICENSE)

# python-solvespace

Python library from the solver of SolveSpace, an open source CAD software.

+ [Python API](https://pyslvs-ui.readthedocs.io/en/stable/python-solvespace-api/)
+ [C API](https://github.com/solvespace/solvespace/blob/master/exposed/DOC.txt)

# Install

```bash
pip install python-solvespace
```

# Build and Test (Repository)

Build and install the module:

```bash
pip install python-solvespace
# From repository
git submodule update --init
pip install -e .
```

Run unit tests:

```bash
python test
```

Uninstall the module:

```bash
pip uninstall python-solvespace
```

[GNU Make]: https://sourceforge.net/projects/mingw-w64/files/latest/download?source=files
[Cython]: https://cython.org/
