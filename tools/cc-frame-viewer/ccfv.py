# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import optparse
import os
import sys

def Init():
  chromeapp_path = os.path.abspath(
    os.path.join(os.path.dirname(__file__),
                 'third_party', 'py-chrome-app'))
  assert os.path.isdir(chromeapp_path)
  sys.path.append(chromeapp_path)
Init()

import chromeapp
from build import parse_deps

srcdir = os.path.abspath(os.path.join(os.path.dirname(__file__), "src"))

js_warning_message = """/**
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* WARNING: This file is generated by ccfv.py
 *
 *        Do not edit directly.
 */
"""

css_warning_message = """/**
/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/* WARNING: This file is generated by ccfv.py
 *
 *        Do not edit directly.
 */
"""

py_warning_message = """#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file. */
#
# WARNING: This file is generated by ccfv.py
#
#        Do not edit directly.
#
"""

def _sopen(filename, mode):
  if filename != '-':
    return open(filename, mode)
  return os.fdopen(os.dup(sys.stdout.fileno()), 'w')

def generate_css(input_filenames):
  load_sequence = parse_deps.calc_load_sequence(input_filenames, srcdir)

  style_sheet_chunks = [css_warning_message, '\n']
  for module in load_sequence:
    for style_sheet in module.style_sheets:
      style_sheet_chunks.append("""%s\n""" % style_sheet.contents)

  return ''.join(style_sheet_chunks)

def generate_js(input_filenames):
  load_sequence = parse_deps.calc_load_sequence(input_filenames, srcdir)

  js_chunks = [js_warning_message, '\n']
  js_chunks.append("window.FLATTENED = {};\n")
  js_chunks.append("window.FLATTENED_RAW_SCRIPTS = {};\n")

  for module in load_sequence:
    js_chunks.append("window.FLATTENED['%s'] = true;\n" % module.name)
    for raw_script in module.dependent_raw_scripts:
      js_chunks.append("window.FLATTENED_RAW_SCRIPTS['%s'] = true;\n" %
                       raw_script.name)

  raw_scripts_that_have_been_written = set()
  for module in load_sequence:
    for raw_script in module.dependent_raw_scripts:
      if raw_script.name in raw_scripts_that_have_been_written:
        continue
      js_chunks.append(raw_script.contents)
      js_chunks.append("\n")
      raw_scripts_that_have_been_written.add(raw_script.name)
    js_chunks.append(module.contents)
    js_chunks.append("\n")

  return ''.join(js_chunks)

def Main(args):
  parser = optparse.OptionParser('%prog <filename>')
  parser.add_option('--debug', dest='debug_mode', action='store_true',
                    default=False, help='Enables debugging features')
  options, args = parser.parse_args(args)
  if len(args) != 1:
    parser.error("argument required")
  if not os.path.exists(args[0]):
    parser.error("%s does not exist" % args[0])

  manifest_file = os.path.join(os.path.dirname(__file__),
                               'app', 'manifest.json')
  app = chromeapp.App('cc-frame-viewer',
                      manifest_file,
                      debug_mode=options.debug_mode)

  def OnLoad(req):
    with open(args[0], 'r') as f:
      return f.read()

  input_filenames = [os.path.join(srcdir, f)
                     for f in ['base.js', 'model_view.js']]
  view_js_file = os.path.join(os.path.dirname(__file__),
                              'app', 'model_view.js')
  view_css_file = os.path.join(os.path.dirname(__file__),
                              'app', 'model_view.css')
  with open(view_js_file, 'w') as f:
    f.write(generate_js(input_filenames))
  with open(view_css_file, 'w') as f:
    f.write(generate_css(input_filenames))

  with chromeapp.AppInstance(app, []) as app_instance:
    app_instance.AddListener('load', OnLoad)
    try:
      return app_instance.Run()
    finally:
      if os.path.exists(view_js_file):
        os.unlink(view_js_file)
      if os.path.exists(view_css_file):
        os.unlink(view_css_file)


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
