#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import glob
import os
import subprocess
import sys
import tempfile
import time
import zipfile

import mopy.gn as gn
from mopy.config import Config
from mopy.paths import Paths
from mopy.version import Version

def find_apps_to_upload(build_dir):
  apps = []
  for path in glob.glob(build_dir + "/*"):
    if not os.path.isfile(path):
      continue
    _, ext = os.path.splitext(path)
    if ext != '.mojo':
      continue
    if 'apptests' in os.path.basename(path):
      continue
    apps.append(path)
  return apps

def upload(config, source, dest, dry_run):
  paths = Paths(config)
  sys.path.insert(0, os.path.join(paths.src_root, "tools"))
  # pylint: disable=F0401
  import find_depot_tools

  depot_tools_path = find_depot_tools.add_depot_tools_to_path()
  gsutil_exe = os.path.join(depot_tools_path, "third_party", "gsutil", "gsutil")

  if dry_run:
    print str([gsutil_exe, "cp", source, dest])
  else:
    subprocess.check_call([gsutil_exe, "cp", source, dest])

def upload_shell(config, dry_run, verbose):
  paths = Paths(config)
  zipfile_name = "%s-%s" % (config.target_os, config.target_arch)
  dest = "gs://mojo/shell/" + Version().version + "/" + zipfile_name + ".zip"
  with tempfile.NamedTemporaryFile() as zip_file:
    with zipfile.ZipFile(zip_file, 'w') as z:
      shell_path = paths.target_mojo_shell_path
      with open(shell_path) as shell_binary:
        shell_filename = os.path.basename(shell_path)
        zipinfo = zipfile.ZipInfo(shell_filename)
        zipinfo.external_attr = 0777 << 16L
        compress_type = zipfile.ZIP_DEFLATED
        if config.target_os == Config.OS_ANDROID:
          # The APK is already compressed.
          compress_type = zipfile.ZIP_STORED
        zipinfo.compress_type = compress_type
        zipinfo.date_time = time.gmtime(os.path.getmtime(shell_path))
        if verbose:
          print "zipping %s" % shell_path
        z.writestr(zipinfo, shell_binary.read())
        upload(config, zip_file.name, dest, dry_run)

def upload_app(app_binary_path, config, dry_run):
  app_binary_name = os.path.basename(app_binary_path)
  target = config.target_os + "-" + config.target_arch
  version = Version().version
  dest = "gs://mojo/services/" + target + "/" + version + "/" + app_binary_name
  upload(config, app_binary_path, dest, dry_run)

def main():
  # Default to the build dir used by the Linux release config so that the script
  # works correctly on the Linux release bot, where it is invoked without the
  # --build_dir argument.
  # TODO(blundell): Remove this hack once the Linux bot is passing the build dir
  # to this script.
  linux_release_config = Config(target_os=Config.OS_LINUX, is_debug=False)
  default_build_dir = Paths(linux_release_config).build_dir
  parser = argparse.ArgumentParser(description="Upload mojo_shell binary to "+
      "google storage")
  parser.add_argument("-n", "--dry_run", help="Dry run, do not actually "+
      "upload", action="store_true")
  parser.add_argument("-v", "--verbose", help="Verbose mode",
      action="store_true")
  parser.add_argument("--build_dir",
                      type=str,
                      metavar="<build_dir>",
                      help="The build dir containing the shell to be uploaded",
                      default=default_build_dir)
  args = parser.parse_args()

  config = gn.ConfigForGNArgs(gn.ParseGNConfig(args.build_dir))
  upload_shell(config, args.dry_run, args.verbose)

  apps_to_upload = find_apps_to_upload(args.build_dir)
  for app in apps_to_upload:
    upload_app(app, config, args.dry_run)
  return 0

if __name__ == "__main__":
  sys.exit(main())
