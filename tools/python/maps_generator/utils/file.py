import errno
import functools
import glob
import os
import shutil
import subprocess
import tarfile

from .md5 import md5, check_md5


def is_executable(fpath):
    return os.path.isfile(fpath) and os.access(fpath, os.X_OK)


@functools.lru_cache()
def find_executable(path, exe=None):
    if exe is None:
        if is_executable(path):
            return path
        else:
            raise FileNotFoundError(path)
    find_pattern = f"{path}/**/{exe}"
    for name in glob.iglob(find_pattern, recursive=True):
        if is_executable(name):
            return name
    raise FileNotFoundError(f"{exe} not found in {path}")


def download_file(url, name, output=subprocess.DEVNULL,
                  error=subprocess.DEVNULL):
    return subprocess.Popen(["curl", "-s", "-L", "-o" + name, url],
                            stdout=output, stderr=error)


def is_exists_file_and_md5(name):
    return os.path.isfile(name) and os.path.isfile(md5(name))


def is_verified(name):
    return is_exists_file_and_md5(name) and check_md5(name, md5(name))


def copy_overwrite(from_path, to_path):
    if os.path.exists(to_path):
        shutil.rmtree(to_path)
    shutil.copytree(from_path, to_path)


def symlink_force(target, link_name):
    try:
        os.symlink(target, link_name)
    except OSError as e:
        if e.errno == errno.EEXIST:
            os.remove(link_name)
            os.symlink(target, link_name)
        else:
            raise e


def make_tarfile(output_filename, source_dir):
    with tarfile.open(output_filename, "w:gz") as tar:
        tar.add(source_dir, arcname=os.path.basename(source_dir))
