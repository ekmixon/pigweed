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
"""UI Color Styles for ConsoleApp."""

import logging
from dataclasses import dataclass

from prompt_toolkit.styles import Style
from prompt_toolkit.filters import has_focus

_LOG = logging.getLogger(__package__)


@dataclass
class HighContrastDarkColors:
    # pylint: disable=too-many-instance-attributes
    default_bg = '#100f10'
    default_fg = '#ffffff'

    dim_bg = '#000000'
    dim_fg = '#e0e6f0'

    button_active_bg = '#4e4e4e'
    button_inactive_bg = '#323232'

    active_bg = '#323232'
    active_fg = '#f4f4f4'

    inactive_bg = '#1e1e1e'
    inactive_fg = '#bfc0c4'

    line_highlight_bg = '#2f2f2f'
    dialog_bg = '#3c3c3c'

    red_accent = '#ffc0bf'
    orange_accent = '#f5ca80'
    yellow_accent = '#d2e580'
    green_accent = '#88ef88'
    cyan_accent = '#60e7e0'
    blue_accent = '#92d9ff'
    purple_accent = '#cfcaff'
    magenta_accent = '#ffb8ff'


@dataclass
class DarkColors:
    # pylint: disable=too-many-instance-attributes
    default_bg = '#2e2e2e'
    default_fg = '#bbc2cf'

    dim_bg = '#262626'
    dim_fg = '#dfdfdf'

    button_active_bg = '#626262'
    button_inactive_bg = '#525252'

    active_bg = '#525252'
    active_fg = '#dfdfdf'

    inactive_bg = '#3f3f3f'
    inactive_fg = '#bfbfbf'

    line_highlight_bg = '#1e1e1e'
    dialog_bg = '#3c3c3c'

    red_accent = '#ff6c6b'
    orange_accent = '#da8548'
    yellow_accent = '#ffcc66'
    green_accent = '#98be65'
    cyan_accent = '#66cccc'
    blue_accent = '#6699cc'
    purple_accent = '#a9a1e1'
    magenta_accent = '#c678dd'


@dataclass
class NordColors:
    # pylint: disable=too-many-instance-attributes
    default_bg = '#2e3440'
    default_fg = '#eceff4'

    dim_bg = '#272c36'
    dim_fg = '#e5e9f0'

    button_active_bg = '#4c566a'
    button_inactive_bg = '#434c5e'

    active_bg = '#434c5e'
    active_fg = '#eceff4'

    inactive_bg = '#373e4c'
    inactive_fg = '#d8dee9'

    line_highlight_bg = '#191c25'
    dialog_bg = '#2c333f'

    red_accent = '#bf616a'
    orange_accent = '#d08770'
    yellow_accent = '#ebcb8b'
    green_accent = '#a3be8c'
    cyan_accent = '#88c0d0'
    blue_accent = '#81a1c1'
    purple_accent = '#a9a1e1'
    magenta_accent = '#b48ead'


@dataclass
class NordLightColors:
    # pylint: disable=too-many-instance-attributes
    default_bg = '#e5e9f0'
    default_fg = '#3b4252'
    dim_bg = '#d8dee9'
    dim_fg = '#2e3440'
    button_active_bg = '#aebacf'
    button_inactive_bg = '#b8c5db'
    active_bg = '#b8c5db'
    active_fg = '#3b4252'
    inactive_bg = '#c2d0e7'
    inactive_fg = '#60728c'
    line_highlight_bg = '#f0f4fc'
    dialog_bg = '#d8dee9'

    red_accent = '#99324b'
    orange_accent = '#ac4426'
    yellow_accent = '#9a7500'
    green_accent = '#4f894c'
    cyan_accent = '#398eac'
    blue_accent = '#3b6ea8'
    purple_accent = '#842879'
    magenta_accent = '#97365b'


@dataclass
class MoonlightColors:
    # pylint: disable=too-many-instance-attributes
    default_bg = '#212337'
    default_fg = '#c8d3f5'
    dim_bg = '#191a2a'
    dim_fg = '#b4c2f0'
    button_active_bg = '#444a73'
    button_inactive_bg = '#2f334d'
    active_bg = '#2f334d'
    active_fg = '#c8d3f5'
    inactive_bg = '#222436'
    inactive_fg = '#a9b8e8'
    line_highlight_bg = '#383e5c'
    dialog_bg = '#1e2030'

    red_accent = '#d95468'
    orange_accent = '#d98e48'
    yellow_accent = '#8bd49c'
    green_accent = '#ebbf83'
    cyan_accent = '#70e1e8'
    blue_accent = '#5ec4ff'
    purple_accent = '#b62d65'
    magenta_accent = '#e27e8d'



_THEME_NAME_MAPPING = {
    'moonlight': MoonlightColors(),
    'nord': NordColors(),
    'nord-light': NordLightColors(),
    'dark': DarkColors(),
    'high-contrast-dark': HighContrastDarkColors(),
} # yapf: disable

def generate_styles(theme_name='dark'):
    """Return prompt_toolkit styles for the given theme name."""
    # Use DarkColors() if name not found.
    theme = _THEME_NAME_MAPPING.get(theme_name, DarkColors())

    pw_console_styles = {
        'default': f'bg:{theme.default_bg} {theme.default_fg}',
        'pane_inactive': f'bg:{theme.dim_bg} {theme.dim_fg}',
        'pane_active': f'bg:{theme.default_bg} {theme.default_fg}',
        'toolbar_active': f'bg:{theme.active_bg} {theme.active_fg}',
        'toolbar_inactive': f'bg:{theme.inactive_bg} {theme.inactive_fg}',
        'toolbar_dim_active': f'bg:{theme.active_bg} {theme.active_fg}',
        'toolbar_dim_inactive': f'bg:{theme.default_bg} {theme.inactive_fg}',
        'toolbar_accent': theme.cyan_accent,
        'toolbar-button-decoration': f'{theme.cyan_accent}',
        'toolbar-setting-active': f'bg:{theme.green_accent} {theme.active_bg}',
        'toolbar-button-active': f'bg:{theme.button_active_bg}',
        'toolbar-button-inactive': f'bg:{theme.button_inactive_bg}',
        'scrollbar.background': f'bg:{theme.default_bg} {theme.default_fg}',
        'scrollbar.button': f'bg:{theme.purple_accent} {theme.default_bg}',
        'scrollbar.arrow': f'bg:{theme.default_bg} {theme.blue_accent}',
        'menu-bar': f'bg:{theme.inactive_bg} {theme.inactive_fg}',
        'menu-bar.selected-item': f'bg:{theme.blue_accent} {theme.inactive_bg}',
        'menu': f'bg:{theme.dialog_bg} {theme.dim_fg}',
        'menu-border': theme.magenta_accent,
        'logo': f'{theme.magenta_accent} bold',
        'keybind': f'{theme.purple_accent} bold',
        'keyhelp': theme.dim_fg,
        'help_window_content': f'bg:{theme.dialog_bg} {theme.dim_fg}',
        'frame.border': f'bg:{theme.dialog_bg} {theme.purple_accent}',
        'pane_indicator_active': f'bg:{theme.magenta_accent}',
        'pane_indicator_inactive': f'bg:{theme.inactive_bg}',
        'pane_title_active': f'{theme.magenta_accent} bold',
        'pane_title_inactive': f'{theme.purple_accent}',
        'window-tab-active': f'bg:{theme.active_bg} {theme.cyan_accent}',
        'window-tab-inactive': f'bg:{theme.inactive_bg} {theme.inactive_fg}',
        'pane_separator': f'bg:{theme.default_bg} {theme.purple_accent}',
        'search': f'bg:{theme.cyan_accent} {theme.default_bg}',
        'search.current': f'bg:{theme.cyan_accent} {theme.default_bg}',
        'selected-log-line': f'bg:{theme.line_highlight_bg}',
        'cursor-line': f'bg:{theme.line_highlight_bg} nounderline',
        'warning-text': f'bg:{theme.default_bg} {theme.yellow_accent}',
        'log-time': f'bg:{theme.default_fg} {theme.default_bg}',
        f'log-level-{logging.CRITICAL}': f'{theme.red_accent} bold',
        f'log-level-{logging.ERROR}': f'{theme.red_accent}',
        f'log-level-{logging.WARNING}': f'{theme.yellow_accent}',
        f'log-level-{logging.INFO}': f'{theme.purple_accent}',
        f'log-level-{logging.DEBUG}': f'{theme.blue_accent}',
        'log-table-column-0': f'{theme.cyan_accent}',
        'log-table-column-1': f'{theme.green_accent}',
        'log-table-column-2': f'{theme.yellow_accent}',
        'log-table-column-3': f'{theme.magenta_accent}',
        'log-table-column-4': f'{theme.purple_accent}',
        'log-table-column-5': f'{theme.blue_accent}',
        'log-table-column-6': f'{theme.orange_accent}',
        'log-table-column-7': f'{theme.red_accent}',
        'search-bar-title': f'bg:{theme.cyan_accent} {theme.default_bg}',
        'search-bar-setting': f'{theme.cyan_accent}',
        'search-bar': f'bg:{theme.inactive_bg}',
        'filter-bar-title': f'bg:{theme.red_accent} {theme.default_bg}',
        'filter-bar-setting': f'{theme.cyan_accent}',
        'filter-bar-delete': f'{theme.red_accent}',
        'filter-bar': f'bg:{theme.inactive_bg}',
        'filter-bar-delimiter': f'{theme.purple_accent}',
        "title": "",
        "label": "bold",
        "percentage": f"{theme.green_accent}",
        "bar": f"{theme.magenta_accent}",
        "bar-a": f"{theme.cyan_accent} bold",
        "bar-b": f"{theme.purple_accent} bold",
        "bar-c": "",
        "current": f"{theme.cyan_accent}",
        "total": f"{theme.cyan_accent}",
        "time-elapsed": f"{theme.purple_accent}",
        "time-left": f"{theme.magenta_accent}",
    }


    return Style.from_dict(pw_console_styles)


def get_toolbar_style(pt_container, dim=False) -> str:
    """Return the style class for a toolbar if pt_container is in focus."""
    if has_focus(pt_container.__pt_container__())():
        return 'class:toolbar_dim_active' if dim else 'class:toolbar_active'
    return 'class:toolbar_dim_inactive' if dim else 'class:toolbar_inactive'


def get_button_style(pt_container) -> str:
    """Return the style class for a toolbar if pt_container is in focus."""
    if has_focus(pt_container.__pt_container__())():
        return 'class:toolbar-button-active'
    return 'class:toolbar-button-inactive'


def get_pane_style(pt_container) -> str:
    """Return the style class for a pane title if pt_container is in focus."""
    if has_focus(pt_container.__pt_container__())():
        return 'class:pane_active'
    return 'class:pane_inactive'


def get_pane_indicator(pt_container, title, mouse_handler=None):
    """Return formatted text for a pane indicator and title."""
    if mouse_handler:
        inactive_indicator = ('class:pane_indicator_inactive', ' ',
                              mouse_handler)
        active_indicator = ('class:pane_indicator_active', ' ', mouse_handler)
        inactive_title = ('class:pane_title_inactive', title, mouse_handler)
        active_title = ('class:pane_title_active', title, mouse_handler)
    else:
        inactive_indicator = ('class:pane_indicator_inactive', ' ')
        active_indicator = ('class:pane_indicator_active', ' ')
        inactive_title = ('class:pane_title_inactive', title)
        active_title = ('class:pane_title_active', title)

    if has_focus(pt_container.__pt_container__())():
        return [active_indicator, active_title]
    return [inactive_indicator, inactive_title]
