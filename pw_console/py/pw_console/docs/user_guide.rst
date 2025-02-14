.. _module-pw_console-user_guide:

User Guide
==========

.. seealso::

   This guide can be viewed online at: https://pigweed.dev/pw_console/

The Pigweed Console provides a Python repl (read eval print loop) and log viewer
in a single-window terminal based interface.


Starting the Console
--------------------

::

  pw rpc -s localhost:33000 --proto-globs pw_rpc/echo.proto


Exiting
~~~~~~~

1.  Click the :guilabel:`[File]` menu and then :guilabel:`Exit`.
2.  Type ``quit`` or ``exit`` in the Python Input window and press :kbd:`Enter`.


Interface Layout
----------------

On startup the console will display multiple windows one on top of the other.

::

  +-----------------------------------------------------+
  | [File] [View] [Window] [Help]       Pigweed Console |
  +=====================================================+
  |                                                     |
  |                                                     |
  |                                                     |
  | Log Window                                          |
  +=====================================================+
  |                                                     |
  |                                                     |
  | Python Results                                      |
  +- - - - - - - - - - - - - - - - - - - - - - - - - - -+
  |                                                     |
  | Python Input                                        |
  +-----------------------------------------------------+


Navigation
----------

All menus, windows, and toolbar buttons can be clicked on. Scrolling with the
mouse wheel should work too. This requires that your terminal is able to send
mouse events.


Main Menu Navigation with the Keyboard
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

============================================  =====================
Function                                      Keys
============================================  =====================
Move focus between all active UI elements     :kbd:`Shift-Tab`

Move focus between windows and the main menu  :kbd:`Ctrl-Up`
                                              :kbd:`Ctrl-Down`

Move selection in the main menu               :kbd:`Up`
                                              :kbd:`Down`
                                              :kbd:`Left`
                                              :kbd:`Right`
============================================  =====================


Toolbars
~~~~~~~~

Log toolbar functions are clickable. You can also press the keyboard
shortcut highlighted in blue:

::

        / : Search  f : [x] Follow  t : [x] Table  w : [ ] Wrap  C : Clear


Log Window
~~~~~~~~~~

Log Window Scrolling
^^^^^^^^^^^^^^^^^^^^

============================================  =====================
Function                                      Keys
============================================  =====================
Scroll logs up (1 line)                       :kbd:`Up`
                                              :kbd:`k`

Scroll logs down (1 line)                     :kbd:`Down`
                                              :kbd:`j`

Scroll logs up (5 lines)                      :guilabel:`Mouse Wheel Up`
Scroll logs down (5 lines)                    :guilabel:`Mouse Wheel Down`

Scroll logs up one page                       :kbd:`PageUp`
Scroll logs down one page                     :kbd:`PageDown`

Jump to the beginning                         :kbd:`g`
Jump to the end                               :kbd:`G`

Horizontal scroll left or right               :kbd:`Left`
                                              :kbd:`Right`

Horizontal scroll to the beginning            :kbd:`Home`
                                              :kbd:`0`
                                              :kbd:`^`
============================================  =====================

Log Window View Options
^^^^^^^^^^^^^^^^^^^^^^^

============================================  =====================
Function                                      Keys
============================================  =====================
Copy visible log lines to the clipboard.      :kbd:`Ctrl-c`
Toggle line following.                        :kbd:`f`
Toggle table view.                            :kbd:`t`
Toggle line wrapping.                         :kbd:`w`
Clear log pane history.                       :kbd:`C`
============================================  =====================

Log Window Management
^^^^^^^^^^^^^^^^^^^^^^^

============================================  =====================
Function                                      Keys
============================================  =====================
Duplicate this log pane.                      :kbd:`Insert`
Remove log pane.                              :kbd:`Delete`
============================================  =====================

Log Searching
^^^^^^^^^^^^^

============================================  =====================
Function                                      Keys
============================================  =====================
Open the search bar                           :kbd:`/`
                                              :kbd:`Ctrl-f`
Navigate search term history                  :kbd:`Up`
                                              :kbd:`Down`
Start the search and highlight matches        :kbd:`Enter`
Close the search bar without searching        :kbd:`Ctrl-c`
============================================  =====================

Here is a view of the search bar:

::

  +-------------------------------------------------------------------------------+
  |           Enter : Search  Ctrl-Alt-f : Add Filter  Ctrl-Alt-r : Clear Filters |
  |  Search   Ctrl-t : Column:All  Ctrl-v : [ ] Invert  Ctrl-n : Matcher:REGEX    |
  | /                                                                             |
  +-------------------------------------------------------------------------------+

Across the top are various functions with keyboard shortcuts listed. Each of
these are clickable with the mouse. The second line shows configurable search
parameters.

**Search Parameters**

- ``Column:All`` Change the part of the log message to match on. For example:
  ``All``, ``Message`` or any extra metadata column.

- ``Invert`` match. Find lines that don't match the entered text.

- ``Matcher``: How the search input should be interpreted.

    - ``REGEX``: Treat input text as a regex.

    - ``STRING``: Treat input as a plain string. Any regex characters will be
      escaped when search is performed.

    - ``FUZZY``: input text is split on spaces using the ``.*`` regex. For
      example if you search for ``idle run`` the resulting search regex used
      under the hood is ``(idle)(.*?)(run)``. This would match both of these
      lines:

      .. code-block:: text

         Idle task is running
         Idle thread is running

**Active Search Shortcuts**

When a search is started the bar will close, log follow mode is disabled and all
matches will be highlighted.  At this point a few extra keyboard shortcuts are
available.

============================================  =====================
Function                                      Keys
============================================  =====================
Move to the next search result                :kbd:`n`
                                              :kbd:`Ctrl-g`
                                              :kbd:`Ctrl-s`
Move to the previous search result            :kbd:`N`
                                              :kbd:`Ctrl-r`
Removes search highlighting                   :kbd:`Ctrl-l`
Creates a filter using the active search      :kbd:`Ctrl-Alt-f`
Reset all active filters.                     :kbd:`Ctrl-Alt-r`
============================================  =====================


Log Filtering
^^^^^^^^^^^^^

Log filtering allows you to limit what log lines appear in any given log
window. Filters can be added from the currently active search or directly in the
search bar.

- With the search bar **open**:

  Type something to search for then press :kbd:`Ctrl-Alt-f` or click on
  :guilabel:`Add Filter`.

- With the search bar **closed**:

  Press :kbd:`Ctrl-Alt-f` to use the current search term as a filter.

When a filter is active the ``Filters`` toolbar will appear at the bottom of the
log window. For example, here are some logs with one active filter for
``lorem ipsum``.

::

  +------------------------------------------------------------------------------+
  | Time               Lvl  Module  Message                                      |
  +------------------------------------------------------------------------------+
  | 20210722 15:38:14  INF  APP     Log message # 270 Lorem ipsum dolor sit amet |
  | 20210722 15:38:24  INF  APP     Log message # 280 Lorem ipsum dolor sit amet |
  | 20210722 15:38:34  INF  APP     Log message # 290 Lorem ipsum dolor sit amet |
  | 20210722 15:38:44  INF  APP     Log message # 300 Lorem ipsum dolor sit amet |
  | 20210722 15:38:54  INF  APP     Log message # 310 Lorem ipsum dolor sit amet |
  | 20210722 15:39:04  INF  APP     Log message # 320 Lorem ipsum dolor sit amet |
  +------------------------------------------------------------------------------+
  |  Filters   <lorem ipsum (X)>  Ctrl-Alt-r : Clear Filters                     |
  +------------------------------------------------------------------------------+
  |   Logs   / : Search  f : [x] Follow  t : [x] Table  w : [ ] Wrap  C : Clear  |
  +------------------------------------------------------------------------------+

**Stacking Filters**

Adding a second filter on the above logs for ``# 2`` would update the filter
toolbar to show:

::

  +------------------------------------------------------------------------------+
  | Time               Lvl  Module  Message                                      |
  +------------------------------------------------------------------------------+
  |                                                                              |
  |                                                                              |
  |                                                                              |
  | 20210722 15:38:14  INF  APP     Log message # 270 Lorem ipsum dolor sit amet |
  | 20210722 15:38:24  INF  APP     Log message # 280 Lorem ipsum dolor sit amet |
  | 20210722 15:38:34  INF  APP     Log message # 290 Lorem ipsum dolor sit amet |
  +------------------------------------------------------------------------------+
  |  Filters   <lorem ipsum (X)>  <# 2 (X)>  Ctrl-Alt-r : Clear Filters          |
  +------------------------------------------------------------------------------+
  |   Logs   / : Search  f : [x] Follow  t : [x] Table  w : [ ] Wrap  C : Clear  |
  +------------------------------------------------------------------------------+

Any filter listed in the Filters toolbar and can be individually removed by
clicking on the red ``(X)`` text.


Python Window
~~~~~~~~~~~~~


Running Code in the Python Repl
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Type code and hit :kbd:`Enter` to run.
-  If multiple lines are used, move the cursor to the end and press
   :kbd:`Enter` twice.
-  :kbd:`Up` / :kbd:`Down` Navigate command history
-  :kbd:`Ctrl-r` Start reverse history searching
-  :kbd:`Ctrl-c` Erase the input buffer
-  :kbd:`Ctrl-v` Paste text from the clipboard to the input buffer
-  :kbd:`Ctrl-Alt-c` Copy the Python Output to the system clipboard

   -  If the input buffer is empty:
      :kbd:`Ctrl-c` cancels any currently running Python commands.

-  :kbd:`F2` Open the python repl settings (from
   `ptpython <https://github.com/prompt-toolkit/ptpython>`__). This
   works best in vertical split mode.

   -  To exit: hit :kbd:`F2` again.
   -  Navigate options with the arrow keys, Enter will close the menu.

-  :kbd:`F3` Open the python repl history (from
   `ptpython <https://github.com/prompt-toolkit/ptpython>`__).

   -  To exit: hit :kbd:`F3` again.
   -  Left side shows previously entered commands
   -  Use arrow keys to navigate.
   -  :kbd:`Space` to select as many lines you want to use

      -  Selected lines will be appended to the right side.

   -  :kbd:`Enter` to accept the right side text, this will be inserted
      into the repl.


Copy & Pasting
~~~~~~~~~~~~~~

Copying Text
^^^^^^^^^^^^

Text can be copied from the Log and Python windows when they are in focus with
these keybindings.

============================================  =====================
Function                                      Keys
============================================  =====================
Copy Logs from the focused log window         :kbd:`Ctrl-c`
Copy Python Output if window is focused       :kbd:`Ctrl-Alt-c`
============================================  =====================

Text will be put in the host computer's system clipboard using the
`pyperclip package <https://pypi.org/project/pyperclip/>`__.

The above functions can also be accessed by clicking on the toolbar help text or
accessed under the :guilabel:`[Edit]` menu.

If you need to copy text from any other part of the UI you will have to use your
terminal's built in text selection:

- **Linux**

  - Holding :kbd:`Shift` and dragging the mouse in most terminals.

- **Mac**

  - **Apple Terminal**:

    Hold :kbd:`Fn` and drag the mouse

  - **iTerm2**:

    Hold :kbd:`Cmd+Option` and drag the mouse

- **Windows**

  - **Git CMD** (included in `Git for Windows <https://git-scm.com/downloads>`__)

    1. Click on the Git window icon in the upper left of the title bar
    2. Click ``Edit`` then ``Mark``
    3. Drag the mouse to select text and press Enter to copy.

  - **Windows Terminal**

    1. Hold :kbd:`Shift` and drag the mouse to select text
    2. Press :kbd:`Ctrl-Shift-C` to copy.

Pasting Text
^^^^^^^^^^^^

Text can be pasted into the Python Input window from the system clipboard with
:kbd:`Ctrl-v`.

If you are using the console on a separate machine (over an ssh connection for
example) then pasting will use that machine's clipboard. This may not be the
computer where you copied the text. In that case you will need to use your
terminal emulator's paste function. How to do this depends on what terminal you
are using and on which OS. Here's how on various platforms:

- **Linux**

  - **XTerm**

    :kbd:`Shift-Insert` pastes text

  - **Gnome Terminal**

    :kbd:`Ctrl-Shift-V` pastes text

- **Windows**

  - **Git CMD** (included in `Git for Windows <https://git-scm.com/downloads>`__)

    1. Click on the Git icon in the upper left of the windows title bar and open
       ``Properties``.
    2. Checkmark the option ``Use Ctrl+Shift+C/V as Copy Paste`` and hit ok.
    3. Then use :kbd:`Ctrl-Shift-V` to paste.

  - **Windows Terminal**

   -  :kbd:`Ctrl-Shift-V` pastes text.
   -  :kbd:`Shift-RightClick` also pastes text.


Window Management
~~~~~~~~~~~~~~~~~

Any window can be hidden by clicking the :guilabel:`[x] Show Window` checkbox
under the :guilabel:`[Window]` menu.

The active window can be moved and resized with the following keys. There are
also menu options under :guilabel:`[View]` for the same actions.

============================================  =====================
Function                                      Keys
============================================  =====================
Enlarge window height                         :kbd:`Alt-=`
Shrink window height                          :kbd:`Alt--`
                                              (:kbd:`Alt` and :kbd:`Minus`)
Enlarge vertical split width                  :kbd:`Alt-,`
Shrink vertical split width                   :kbd:`Alt-.`
Reset window sizes                            :kbd:`Ctrl-u`

Move window up                                :kbd:`Ctrl-Alt-Up`
Move window down                              :kbd:`Ctrl-Alt-Down`
Move window left                              :kbd:`Ctrl-Alt-Left`
Move window right                             :kbd:`Ctrl-Alt-Right`
============================================  =====================

Moving windows left and right will create a new vertical splits. Each vertical
stack can contain multiple windows and show windows as a stack or tabbed
view.

For example here we have 3 window panes in a single stack. If you focus on Log
Window 1 and move it to the right a new stack is formed in a vertical
split. This can be done repeatedly to form additional window stacks.

::

  +----------------------------------+     +----------------------------------+
  | [File] [View] [Window]   Console |     | [File] [View] [Window]   Console |
  +==================================+     +================+=================+
  | Log Window 1                     |     | Log Window 2   | Log Window 1    |
  |                                  |     |                |                 |
  +==================================+     |                |                 |
  | Log Window 2                     |     |                |                 |
  |                                  |     |                |                 |
  +==================================+     +================+                 |
  |                                  |     |                |                 |
  |                                  |     |                |                 |
  | Python Results                   |     | Python Results |                 |
  |                                  |     |                |                 |
  | Python Input                     |     | Python Input   |                 |
  +----------------------------------+     +----------------+-----------------+

Color Depth
-----------

Some terminals support full 24-bit color and pw console will use that by default
in most cases. One notable exeception is Apple Terminal on MacOS which supports
256 colors only. `iTerm2 <https://iterm2.com/>`__ is a good MacOS alternative
that supports 24-bit colors.

To force a particular color depth: set one of these environment variables before
launching the console. For ``bash`` and ``zsh`` shells you can use the
``export`` command.

::

   # 1 bit | Black and white
   export PROMPT_TOOLKIT_COLOR_DEPTH=DEPTH_1_BIT
   # 4 bit | ANSI colors
   export PROMPT_TOOLKIT_COLOR_DEPTH=DEPTH_4_BIT
   # 8 bit | 256 colors
   export PROMPT_TOOLKIT_COLOR_DEPTH=DEPTH_8_BIT
   # 24 bit | True colors
   export PROMPT_TOOLKIT_COLOR_DEPTH=DEPTH_24_BIT

For Windows command prompt (``cmd.exe``) use the ``set`` command:

::

   set PROMPT_TOOLKIT_COLOR_DEPTH=DEPTH_1_BIT
   set PROMPT_TOOLKIT_COLOR_DEPTH=DEPTH_4_BIT
   set PROMPT_TOOLKIT_COLOR_DEPTH=DEPTH_8_BIT
   set PROMPT_TOOLKIT_COLOR_DEPTH=DEPTH_24_BIT

Configuration
-------------

Pigweed Console supports loading project and user specific settings stored in a
YAML file. By default these files will be loaded one after the other:

- ``$PW_PROJECT_ROOT/.pw_console.yaml``
- ``$HOME/.pw_console.yaml``

Each file follows the same format with settings in ``$HOME`` overriding ones in
``$PW_PROJECT_ROOT``.

Example Config
~~~~~~~~~~~~~~

.. code-block:: yaml

   pw_console:

     # Repl and Search History files
     # Setting these to a file located $PW_PROJECT_ROOT is a
     # good way to make Python repl history project specific.

     # Default: $HOME/.pw_console_history
     repl_history: $PW_PROJECT_ROOT/.pw_console_history

     # Default: $HOME/.pw_console_search
     search_history: $PW_PROJECT_ROOT/.pw_console_search

     # Theme Settings

     # Default: dark
     ui_theme: high-contrast-dark

     # Default: pigweed-code
     code_theme: material

     # Default: False
     swap_light_and_dark: False

     # Log Table View Settings

     # Number of spaces to insert between columns
     # Default: 2
     spaces_between_columns: 2

     # Hide the year month and day from the time column.
     hide_date_from_log_time: False

     # Custom Column Ordering
     # By default columns are ordered as:
     #   time, level, metadata1, metadata2, ..., message
     # The log message is always the last value and not required in this list.
     column_order:
       # Column name
       - time
       - level
       - metadata1
       - metadata2
     # If Any metadata field not listed above will be hidden in table view.
     column_order_omit_unspecified_columns: False

     # Unique Colors for Column Values
     #   Color format: 'bg:#BG-HEX #FG-HEX STYLE'
     # All parts are optional.
     # Empty strings will leave styling unchanged.
     # See prompt_toolkit style format docs here:
     #   https://python-prompt-toolkit.readthedocs.io/en/latest/pages/advanced_topics/styling.html
     column_colors:
       # Column name
       time:
       level:
       metadata1:
         # Field values
         # Default will be applied if no match found
         default: '#98be65'
         BATTERY: 'bg:#6699cc #000000 bold'
         CORE1: 'bg:#da8548 #000000 bold'
         CORE2: 'bg:#66cccc #000000 bold'
       metadata2:
         default: '#ffcc66'
         APP: 'bg:#ff6c6b #000000 bold'
         WIFI: '#555555'

     # Each window column is normally aligned side by side in vertical
     # splits. You can change this to one group of windows on top of the other
     # with horizontal splits using this method
     # Default: vertical
     window_column_split_method: vertical

     # Window Layout
     windows:
       # First window column (vertical split)
       # Each split should have a unique name and include either
       # 'stacked' or 'tabbed' to select a window pane display method.
       Split 1 stacked:
         # Items here are window titles, each should be unique.
         # Window 1
         Device Logs:
           height: 33  # Weighted value for window height
           hidden: False  # Hide this window if True
         # Window 2
         Python Repl:
           height: 67
         # Window 3
         Host Logs:
           hidden: True

       # Second window column
       Split 2 tabbed:
         # This is a duplicate of the existing 'Device Logs' window with a new title
         NEW DEVICE:
           duplicate_of: Device Logs
           # Log filters are defined here
           filters:
             # Metadata column names here or 'all'
             source_name:
               # Matching method name here
               # regex, regex-inverted, string, string-inverted
               regex: 'USB'
             module:
               # An inverted match will remove matching log lines
               regex-inverted: 'keyboard'
         NEW HOST:
           duplicate_of: Host Logs
           filters:
             all:
               string: 'FLASH'

       # Third window column
       Split 3 tabbed:
         # This is a brand new log Window
         Keyboard Logs - IBM:
           loggers:
             # Python logger names to include in this log window
             my_cool_keyboard_device:
               # Level the logger should be set to.
               level: DEBUG
           filters:
             all:
               regex: 'IBM Model M'
         Keyboard Logs - Apple:
           loggers:
             my_cool_keyboard_device:
               level: DEBUG
           filters:
             all:
               regex: 'Apple.*USB'


Known Issues
------------

Log Window
~~~~~~~~~~

- Rendering for log lines that include ``\n`` characters is broken and hidden if
  Table view is turned on.

- Tab character rendering will not work in the log pane view. They will
  appear as ``^I`` since prompt_toolkit can't render them. See this issue for details:
  https://github.com/prompt-toolkit/python-prompt-toolkit/issues/556


Upcoming Features
-----------------

For upcoming features see the Pigweed Console Bug Hotlist at:
https://bugs.chromium.org/u/542633886/hotlists/Console


Feature Requests
~~~~~~~~~~~~~~~~

Create a feature request bugs using this template:
https://bugs.chromium.org/p/pigweed/issues/entry?owner=tonymd@google.com&labels=Type-Enhancement,Priority-Medium&summary=pw_console
