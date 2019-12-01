#!/usr/bin/env python2

import os
import subprocess
import sys

USE_SETUPTOOLS = False
try:
    from setuptools import setup, Extension
    USE_SETUPTOOLS = True
except ImportError:
    from distutils.core import setup, Extension

version = '0.3.6'
if os.path.exists('.git'):
    try:
        commit = subprocess.Popen(['git', 'log', '--max-count=1', '--format=%h'], stdout=subprocess.PIPE).communicate()[0]
        version = '%s-%s' % (version, commit.strip())
    except:
        pass


base_modules = [
    Extension('yajl',  [
                'py-yajl/yajl.c',
                'py-yajl/encoder.c',
                'py-yajl/decoder.c',
                'py-yajl/yajl_hacks.c',
                'py-yajl/yajl/src/yajl_alloc.c',
                'py-yajl/yajl/src/yajl_buf.c',
                'py-yajl/yajl/src/yajl.c',
                'py-yajl/yajl/src/yajl_encode.c',
                'py-yajl/yajl/src/yajl_gen.c',
                'py-yajl/yajl/src/yajl_lex.c',
                'py-yajl/yajl/src/yajl_parser.c',
            ],
            include_dirs=('py-yajl', 'py-yajl/includes/', 'py-yajl/yajl/src'),
            extra_compile_args=['-Wall', '-DMOD_VERSION="%s"' % version],
            language='c'),
        ]


packages = ('yajl',)


setup_kwargs = dict(
    name = 'yajl',
    description = '''A CPython module for Yet-Another-Json-Library''',
    version = version,
    author = 'R. Tyler Ballance',
    author_email = 'tyler@monkeypox.org',
    url = 'http://rtyler.github.com/py-yajl',
    long_description='''The `yajl` module provides a Python binding to the Yajl library originally written by `Lloyd Hilaiel <http://github.com/lloyd>`_.

Mailing List
==============
You can discuss the C library **Yajl** or py-yajl on the Yajl mailing list,
simply send your email to yajl@librelist.com
    ''',
    ext_modules=base_modules,
)

if USE_SETUPTOOLS:
    setup_kwargs.update({'test_suite' : 'tests.unit'})

if not os.listdir('py-yajl/yajl'):
    # Submodule hasn't been created, let's inform the user
    print('>>> It looks like the `yajl` submodule hasn\'t been initialized')
    print('>>> I\'ll try to do that, but if I fail, you can run:')
    print('>>>      `git submodule update --init`')
    subprocess.call(['git', 'submodule', 'update', '--init'])

subprocess.call(['git', 'submodule', 'update',])

if not os.path.exists('py-yajl/includes'):
    # Our symlink into the yajl directory isn't there, let's fixulate that
    os.mkdir('includes')

if not os.path.exists(os.path.join('py-yajl', 'includes', 'yajl')):
    print('>>> Creating a symlink for compilationg: includes/yajl -> yajl/src/api')
    # Now that we have a directory, we need a symlink
    os.symlink(os.path.join('..', 'yajl', 'src', 'api'), os.path.join('includes', 'yajl'))

setup(**setup_kwargs)

