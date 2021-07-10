# -*- coding: utf-8 -*-

"""Compile the Cython libraries of Python-Solvespace."""

__author__ = "Yuan Chang"
__copyright__ = "Copyright (C) 2016-2019"
__license__ = "GPLv3+"
__email__ = "pyslvs@gmail.com"

import sys
from os import walk
from os.path import dirname, isdir, join
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
from setuptools.command.sdist import sdist
from distutils import file_util, dir_util
from platform import system

m_path = 'python_solvespace'
include_path = join(m_path, 'include')
src_path = join(m_path, 'src')
platform_path = join(src_path, 'platform')
extlib_path = join(m_path, 'extlib')
mimalloc_path = join(extlib_path, 'mimalloc')
mimalloc_include_path = join(mimalloc_path, 'include')
mimalloc_src_path = join(mimalloc_path, 'src')
build_dir = 'build'
macros = [
    ('M_PI', 'PI'),
    ('_USE_MATH_DEFINES', None),
    ('ISOLATION_AWARE_ENABLED', None),
    ('LIBRARY', None),
    ('EXPORT_DLL', None),
    ('_CRT_SECURE_NO_WARNINGS', None),
]
compile_args = [
    '-O3',
    '-Wno-cpp',
    '-g',
    '-Wno-write-strings',
    '-fpermissive',
    '-fPIC',
    '-std=c++17',
]
compile_args_msvc = [
    '/O2',
    '/std:c++17',
]
link_args = ['-static-libgcc', '-static-libstdc++',
             '-Wl,-Bstatic,--whole-archive',
             '-lwinpthread',
             '-Wl,--no-whole-archive',
             '-lbcrypt',
             '-lpsapi',
             '-Wl,-Bdynamic']
sources = [
    join(m_path, 'slvs.pyx'),
    join(src_path, 'util.cpp'),
    join(src_path, 'entity.cpp'),
    join(src_path, 'expr.cpp'),
    join(src_path, 'constraint.cpp'),
    join(src_path, 'constrainteq.cpp'),
    join(src_path, 'system.cpp'),
    join(src_path, 'lib.cpp'),
    join(platform_path, 'platform.cpp'),
]
mimalloc_sources = [
    # MiMalloc
    join(mimalloc_src_path, 'stats.c'),
    join(mimalloc_src_path, 'random.c'),
    join(mimalloc_src_path, 'os.c'),
    join(mimalloc_src_path, 'bitmap.c'),
    join(mimalloc_src_path, 'arena.c'),
    join(mimalloc_src_path, 'segment-cache.c'),
    join(mimalloc_src_path, 'segment.c'),
    join(mimalloc_src_path, 'page.c'),
    join(mimalloc_src_path, 'alloc.c'),
    join(mimalloc_src_path, 'alloc-aligned.c'),
    join(mimalloc_src_path, 'alloc-posix.c'),
    join(mimalloc_src_path, 'heap.c'),
    join(mimalloc_src_path, 'options.c'),
    join(mimalloc_src_path, 'init.c'),
]
if {'sdist', 'bdist'} & set(sys.argv):
    sources.append(join(platform_path, 'platform.cpp'))
elif system() == 'Windows':
    # Disable format warning
    compile_args.append('-Wno-format')
    # Solvespace arguments
    macros.append(('WIN32', None))
    if sys.version_info < (3, 7):
        macros.append(('_hypot', 'hypot'))
else:
    macros.append(('UNIX_DATADIR', '"solvespace"'))
compiler_directives = {'binding': True, 'cdivision': True}


def copy_source(dry_run):
    dir_util.copy_tree(join('..', 'include'), include_path, dry_run=dry_run)
    dir_util.copy_tree(join('..', 'extlib', 'mimalloc', 'include'),
                       mimalloc_include_path,
                       dry_run=dry_run)
    dir_util.mkpath(src_path)
    dir_util.mkpath(mimalloc_src_path)
    for path in (join('..', 'src'), join('..', 'extlib', 'mimalloc', 'src')):
        for root, _, files in walk(path):
            for f in files:
                if not (f.endswith('.h') or f.endswith('.c')):
                    continue
                f = join(root, f)
                f_new = f.replace('..', m_path)
                if not isdir(dirname(f_new)):
                    dir_util.mkpath(dirname(f_new))
                file_util.copy_file(f, f_new, dry_run=dry_run)
    for f in sources[1:] + mimalloc_sources:
        file_util.copy_file(f.replace(m_path, '..'), f, dry_run=dry_run)
    # Create an empty header
    open(join(platform_path, 'config.h'), 'a').close()


class Build(build_ext):

    def build_extensions(self):
        compiler = self.compiler.compiler_type
        for e in self.extensions:
            e.cython_directives = compiler_directives
            e.libraries = ['mimalloc']
            e.library_dirs = [build_dir]
            if compiler in {'mingw32', 'unix'}:
                e.define_macros = macros
                e.extra_compile_args = compile_args
                if compiler == 'mingw32':
                    e.extra_link_args = link_args
            elif compiler == 'msvc':
                e.define_macros = macros[1:]
                e.libraries.extend(['shell32', 'advapi32', 'Ws2_32'])
                e.extra_compile_args = compile_args_msvc
        has_src = isdir(include_path) and isdir(src_path) and isdir(extlib_path)
        if not has_src:
            copy_source(self.dry_run)
        # Pre-build MiMalloc
        if compiler in {'mingw32', 'unix'}:
            args = ['-fPIC']
        else:
            args = []
        objects = self.compiler.compile(
            mimalloc_sources,
            extra_postargs=args,
            include_dirs=[mimalloc_include_path, mimalloc_src_path]
        )
        dir_util.mkpath(build_dir)
        self.compiler.create_static_lib(objects, 'mimalloc', target_lang='c',
                                        output_dir=build_dir)
        super(Build, self).build_extensions()
        if not has_src:
            dir_util.remove_tree(include_path, dry_run=self.dry_run)
            dir_util.remove_tree(src_path, dry_run=self.dry_run)
            dir_util.remove_tree(extlib_path, dry_run=self.dry_run)


class PackSource(sdist):

    def run(self):
        copy_source(self.dry_run)
        super(PackSource, self).run()
        if not self.keep_temp:
            dir_util.remove_tree(include_path, dry_run=self.dry_run)
            dir_util.remove_tree(src_path, dry_run=self.dry_run)
            dir_util.remove_tree(extlib_path, dry_run=self.dry_run)


setup(ext_modules=[Extension(
    "python_solvespace.slvs",
    sources,
    language="c++",
    include_dirs=[include_path, src_path, mimalloc_include_path,
                  mimalloc_src_path]
)], cmdclass={'build_ext': Build, 'sdist': PackSource})
