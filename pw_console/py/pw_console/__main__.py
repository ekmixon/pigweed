# Copyright 2021 The Pigweed Authors
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
"""Pigweed Console entry point."""

import argparse
import inspect
import logging
import sys
from typing import List

import pw_cli.log
import pw_cli.argument_types

import pw_console
import pw_console.python_logging

_LOG = logging.getLogger(__package__)


# TODO(tonymd): Remove this when no downstream projects are using it.
def create_temp_log_file():
    return pw_console.python_logging.create_temp_log_file()


def _build_argument_parser() -> argparse.ArgumentParser:
    """Setup argparse."""
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument('-l',
                        '--loglevel',
                        type=pw_cli.argument_types.log_level,
                        default=logging.DEBUG,
                        help='Set the log level'
                        '(debug, info, warning, error, critical)')

    parser.add_argument('--logfile', help='Pigweed Console log file.')

    parser.add_argument('--test-mode',
                        action='store_true',
                        help='Enable fake log messages for testing purposes.')

    parser.add_argument('--console-debug-log-file',
                        help='Log file to send console debug messages to.')

    return parser


def main() -> int:
    """Pigweed Console."""

    parser = _build_argument_parser()
    args = parser.parse_args()

    if not args.logfile:
        # Create a temp logfile to prevent logs from appearing over stdout. This
        # would corrupt the prompt toolkit UI.
        args.logfile = pw_console.python_logging.create_temp_log_file()

    pw_cli.log.install(level=args.loglevel,
                       use_color=True,
                       hide_timestamp=False,
                       log_file=args.logfile)

    if args.console_debug_log_file:
        pw_cli.log.install(level=logging.DEBUG,
                           use_color=True,
                           hide_timestamp=False,
                           log_file=args.console_debug_log_file,
                           logger=logging.getLogger(__package__))
        logging.getLogger(__package__).propagate = False

    global_vars = None
    default_loggers: List = []
    if args.test_mode:
        default_loggers = [
            # Don't include pw_console package logs (_LOG) in the log pane UI.
            # Add the fake logger for test_mode.
            logging.getLogger(pw_console.console_app.FAKE_DEVICE_LOGGER_NAME)
        ]
        # Give access to adding log messages from the repl via: `LOG.warning()`
        global_vars = dict(LOG=default_loggers[0])

    help_text = None
    app_title = None
    if args.test_mode:
        app_title = 'Console Test Mode'
        help_text = inspect.cleandoc("""
            Welcome to the Pigweed Console Test Mode!

            Example commands:

              rpcs.pw.rpc.EchoService.Echo(msg='hello!')

              LOG.warning('Message appears console log window.')
        """)

    console = pw_console.PwConsoleEmbed(
        global_vars=global_vars,
        loggers=default_loggers,
        test_mode=args.test_mode,
        help_text=help_text,
        app_title=app_title,
    )
    console.embed()

    if args.logfile:
        print(f'Logs saved to: {args.logfile}')

    return 0


if __name__ == '__main__':
    sys.exit(main())
