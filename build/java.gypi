# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to build Java in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my-package_java',
#   'type': 'none',
#   'variables': {
#     'java_in_dir': 'path/to/package/root',
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#
# Required variables:
#  java_in_dir - The top-level java directory. The src should be in
#    <java_in_dir>/src.
# Optional/automatic variables:
#  additional_input_paths - These paths will be included in the 'inputs' list to
#    ensure that this target is rebuilt when one of these paths changes.
#  additional_src_dirs - Additional directories with .java files to be compiled
#    and included in the output of this target.
#  generated_src_dirs - Same as additional_src_dirs except used for .java files
#    that are generated at build time. This should be set automatically by a
#    target's dependencies. The .java files in these directories are not
#    included in the 'inputs' list (unlike additional_src_dirs).
#  input_jars_paths - The path to jars to be included in the classpath. This
#    should be filled automatically by depending on the appropriate targets.
#  javac_includes - A list of specific files to include. This is by default
#    empty, which leads to inclusion of all files specified. May include
#    wildcard, and supports '**/' for recursive path wildcards, ie.:
#    '**/MyFileRegardlessOfDirectory.java', '**/IncludedPrefix*.java'.
#  has_java_resources - Set to 1 if the java target contains an
#    Android-compatible resources folder named res.  If 1, R_package and
#    R_package_relpath must also be set.
#  R_package - The java package in which the R class (which maps resources to
#    integer IDs) should be generated, e.g. org.chromium.content.
#  R_package_relpath - Same as R_package, but replace each '.' with '/'.
#  java_strings_grd - The name of the grd file from which to generate localized
#    strings.xml files, if any.

{
  'dependencies': [
    '<(DEPTH)/build/build_output_dirs_android.gyp:build_output_dirs'
  ],
  'variables': {
    'android_jar': '<(android_sdk)/android.jar',
    'input_jars_paths': [ '<(android_jar)' ],
    'additional_src_dirs': [],
    'javac_includes': [],
    'jar_name': '<(_target_name).jar',
    'jar_path': '<(PRODUCT_DIR)/lib.java/<(jar_name)',
    'excluded_classes': [ '*/R.class', '*/R##*.class' ],
    'additional_input_paths': ['>@(additional_R_files)'],
    'generated_src_dirs': ['>@(generated_R_dirs)'],
    'generated_R_dirs': [],
    'additional_R_files': [],
    'has_java_resources%': 0,
    'java_strings_grd%': '',
    'intermediate_dir': '<(SHARED_INTERMEDIATE_DIR)/<(_target_name)',
    'classes_dir': '<(intermediate_dir)/classes',
    'compile_stamp': '<(intermediate_dir)/compile.stamp',
  },
  # This all_dependent_settings is used for java targets only. This will add the
  # jar path to the classpath of dependent java targets.
  'all_dependent_settings': {
    'variables': {
      'input_jars_paths': ['<(jar_path)'],
    },
  },
  'conditions': [
    ['has_java_resources == 1', {
      'variables': {
        'res_dir': '<(java_in_dir)/res',
        'out_res_dir': '<(intermediate_dir)/res',
        'R_dir': '<(intermediate_dir)/java_R',
        'R_file': '<(R_dir)/<(R_package_relpath)/R.java',
        'R_text_file': '<(R_dir)/R.txt',
        'generated_src_dirs': ['<(R_dir)'],
        'additional_input_paths': ['<(R_file)'],
        # grit_grd_file is used by grit_action.gypi, included below.
        'grit_grd_file': '<(java_in_dir)/strings/<(java_strings_grd)',
      },
      'all_dependent_settings': {
        'variables': {
          # Dependent jars include this target's R.java file via
          # generated_R_dirs and additional_R_files.
          'generated_R_dirs': ['<(R_dir)'],
          'additional_R_files': ['<(R_file)'],
          'additional_R_text_files': ['<(R_text_file)'],

          # Dependent APKs include this target's resources via
          # additional_res_dirs and additional_res_packages.
          'additional_res_dirs': ['<(out_res_dir)', '<(res_dir)'],
          'additional_res_packages': ['<(R_package)'],
        },
      },
      'conditions': [
        ['java_strings_grd != ""', {
          'actions': [
            {
              'action_name': 'generate_localized_strings_xml',
              'variables': {
                'grit_out_dir': '<(out_res_dir)',
                # resource_ids is unneeded since we don't generate .h headers.
                'grit_resource_ids': '',
              },
              'includes': ['../build/grit_action.gypi'],
            },
          ],
        }],
      ],
      'actions': [
        # Generate R.java and crunch image resources.
        {
          'action_name': 'process_resources',
          'message': 'processing resources for <(_target_name)',
          'conditions': [
            ['java_strings_grd != ""', {
              'inputs': [
                # TODO(newt): replace this with .../values/strings.xml once
                # the English strings.xml is generated as well? That would be
                # simpler and faster and should be equivalent.
                '<!@pymod_do_main(grit_info <@(grit_defines) --outputs "<(out_res_dir)" <(grit_grd_file))',
              ],
            }],
          ],
          'inputs': [
            '<(DEPTH)/build/android/process_resources.py',
            '<!@(find <(res_dir) -type f)',
          ],
          'outputs': [
            '<(R_file)',
          ],
          'action': [
            '<(DEPTH)/build/android/process_resources.py',
            '--android-sdk', '<(android_sdk)',
            '--android-sdk-tools', '<(android_sdk_tools)',
            '--R-package', '<(R_package)',
            '--R-dir', '<(R_dir)',
            '--res-dir', '<(res_dir)',
            '--out-res-dir', '<(out_res_dir)',

            # Add list of inputs to the command line, so if inputs change
            # (e.g. if a resource if removed), the command will be re-run.
            # TODO(newt): remove this once crbug.com/177552 is fixed in ninja.
            '--ignore', '>(_inputs)',
          ],
        },
      ],
    }],
  ],
  'actions': [
    {
      'action_name': 'javac_<(_target_name)',
      'message': 'Compiling <(_target_name) java sources',
      'variables': {
        'all_src_dirs': [
          '>@(java_in_dir)/src',
          '>@(additional_src_dirs)',
          '>@(generated_src_dirs)',
        ],
      },
      'inputs': [
        '<(DEPTH)/build/android/pylib/build_utils.py',
        '<(DEPTH)/build/android/javac.py',
        '>!@(find >(java_in_dir) >(additional_src_dirs) -name "*.java")',
        '>@(input_jars_paths)',
        '>@(additional_input_paths)',
      ],
      'outputs': [
        '<(compile_stamp)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/javac.py',
        '--output-dir=<(classes_dir)',
        '--classpath=>(input_jars_paths)',
        '--src-dirs=>(all_src_dirs)',
        '--javac-includes=<(javac_includes)',
        '--stamp=<(compile_stamp)',

        # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
        '--ignore=>!(echo >(_inputs) | md5sum)',
      ]
    },
    {
      'action_name': 'jar_<(_target_name)',
      'message': 'Creating <(_target_name) jar',
      'inputs': [
        '<(DEPTH)/build/android/pylib/build_utils.py',
        '<(DEPTH)/build/android/jar.py',
        '<(compile_stamp)',
      ],
      'outputs': [
        '<(jar_path)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/jar.py',
        '--classes-dir=<(classes_dir)',
        '--jar-path=<(jar_path)',
        '--excluded-classes=<(excluded_classes)',

        # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
        '--ignore=>!(echo >(_inputs) | md5sum)',
      ]
    },
  ],
}
