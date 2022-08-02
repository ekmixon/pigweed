#!/usr/bin/env python3
# Copyright 2020 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Creates a Git hook that calls a script with certain arguments."""

import argparse
import logging
import os
from pathlib import Path
import re
import shlex
import subprocess
from typing import Sequence, Union

_LOG: logging.Logger = logging.getLogger(__name__)


def git_repo_root(path: Union[Path, str]) -> Path:
    return Path(
        subprocess.run(['git', '-C', path, 'rev-parse', '--show-toplevel'],
                       check=True,
                       stdout=subprocess.PIPE).stdout.strip().decode())


def install_hook(script,
                 hook: str,
                 args: Sequence[str] = (),
                 repository: Union[Path, str] = '.') -> None:
    """Installs a simple Git hook that calls a script with arguments."""
    root = git_repo_root(repository).resolve()
    script = os.path.relpath(script, root)

    if root.joinpath('.git').is_dir():
        hook_path = root.joinpath('.git', 'hooks', hook)
    elif match := re.match('^gitdir: (.*)$', root.joinpath('.git').read_text()):
        hook_path = root.joinpath(match[1], 'hooks', hook).resolve()

    else:
        raise ValueError('Unexpected format for .git file')

    hook_path.parent.mkdir(exist_ok=True)

    command = ' '.join(shlex.quote(arg) for arg in (script, *args))

    with hook_path.open('w') as file:
        line = lambda *args: print(*args, file=file)

        line('#!/bin/sh')
        line(f'# {hook} hook generated by {__file__}')
        line()
        line('# Unset Git environment variables, which are set when this is ')
        line('# run as a Git hook. These environment variables cause issues ')
        line('# when trying to run Git commands on other repos from a ')
        line('# submodule hook.')
        line('unset $(git rev-parse --local-env-vars)')
        line()
        line(command)

    hook_path.chmod(0o755)
    logging.info('Created %s hook for %s at %s', hook, script, hook_path)


def argument_parser(parser=None) -> argparse.ArgumentParser:
    if parser is None:
        parser = argparse.ArgumentParser(description=__doc__)

    def path(arg: str) -> Path:
        if not os.path.exists(arg):
            raise argparse.ArgumentTypeError(f'"{arg}" is not a valid path')

        return Path(arg)

    parser.add_argument(
        '-r',
        '--repository',
        default='.',
        type=path,
        help='Path to the repository in which to install the hook')
    parser.add_argument('--hook',
                        required=True,
                        help='Which type of Git hook to create')
    parser.add_argument('-s',
                        '--script',
                        required=True,
                        type=path,
                        help='Path to the script to execute in the hook')
    parser.add_argument('args',
                        nargs='*',
                        help='Arguments to provide to the commit hook')

    return parser


if __name__ == '__main__':
    logging.basicConfig(format='%(message)s', level=logging.INFO)
    install_hook(**vars(argument_parser().parse_args()))
