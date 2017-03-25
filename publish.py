#!/usr/bin/python
# build_native.py

import sys
import os, os.path
import shutil
from optparse import OptionParser

def get_num_of_cpu():
    ''' The build process can be accelerated by running multiple concurrent job processes using the -j-option.
    '''
    try:
        import multiprocessing
        return multiprocessing.cpu_count()
    except Exception:
        print "Can't know cpu count, use default 1 cpu"
        return 1

def check_environment_variables():
    ''' Checking the environment NDK_ROOT, which will be used for building
    '''

    try:
        NDK_ROOT = os.environ['NDK_ROOT']
    except Exception:
        print "NDK_ROOT not defined. Please define NDK_ROOT in your environment"
        sys.exit(1)

    return NDK_ROOT

def select_toolchain_version():
    ndk_root = check_environment_variables()
    if os.path.isdir(os.path.join(ndk_root,"toolchains/arm-linux-androideabi-4.9")):
        os.environ['NDK_TOOLCHAIN_VERSION'] = '4.9'
        print "The Selected NDK toolchain version was 4.9 !"
    elif os.path.isdir(os.path.join(ndk_root,"toolchains/arm-linux-androideabi-4.8")):
        os.environ['NDK_TOOLCHAIN_VERSION'] = '4.8'
        print "The Selected NDK toolchain version was 4.8 !"
    else:
        print "Couldn't find the gcc toolchain."
        exit(1)

def do_build(ndk_root, workDir, num_of_cpu):

    ndk_path = os.path.join(ndk_root, "ndk-build")
    ndk_module_path = 'NDK_MODULE_PATH=%s' % (workDir)

    command = 'ndk-build -j%d NDK_PROJECT_PATH=. NDK_APPLICATION_MK=./Application.mk NDK_MODULE_PATH=.' % (num_of_cpu)
    
    os.chdir(workDir)
    if os.system(command) != 0:
        raise Exception("Build dynamic library for project [ " + workDir + " ] fails!")

def copy_file(workDir, srcFile, dstFile):
    srcFilePath = os.path.join(workDir, srcFile)
    if not os.path.exists(srcFilePath):
        return

    dstFilePath = os.path.join(workDir, dstFile)
    dstFileDir = os.path.dirname(dstFilePath)
    if not os.path.exists(dstFileDir):
        os.makedirs(dstFileDir)

    shutil.copy(srcFilePath, dstFilePath)

def remove_dir(rootDir, path):
    removeDir = os.path.join(rootDir, path)
    if os.path.exists(removeDir):
        return shutil.rmtree(removeDir)

# -------------- main --------------
if __name__ == '__main__':

    parser = OptionParser()
    parser.add_option("-r", "--rebuild", dest="rebuild", action="store_true", 
        default=False, help='rebuild shared libraries')
    (opts, args) = parser.parse_args()
    parser.print_help()
    print ''

    workDir = os.path.dirname(os.path.realpath(__file__))
    ndk_root = check_environment_variables()
    num_of_cpu = get_num_of_cpu()

    if opts.rebuild:
        remove_dir(workDir, "CocosHookV3/obj")
        remove_dir(workDir, "CocosHookV2/obj")
        remove_dir(workDir, "UnityHook/obj")

    do_build(ndk_root, os.path.join(workDir, "CocosHookV3"), num_of_cpu)
    do_build(ndk_root, os.path.join(workDir, "CocosHookV2"), num_of_cpu)
    do_build(ndk_root, os.path.join(workDir, "UnityHook"), num_of_cpu)

    copy_file(workDir, "CocosHookV3/libs/armeabi-v7a/libSmallPackageSolution.so", 
        "nevermore/ccc/a7/nevermore")
    copy_file(workDir, "CocosHookV3/libs/armeabi/libSmallPackageSolution.so", 
        "nevermore/ccc/a/nevermore")

    copy_file(workDir, "CocosHookV2/libs/armeabi-v7a/libSmallPackageSolution.so", 
        "nevermore/cc/a7/nevermore")
    copy_file(workDir, "CocosHookV2/libs/armeabi/libSmallPackageSolution.so", 
        "nevermore/cc/a/nevermore")

    copy_file(workDir, "UnityHook/libs/armeabi-v7a/libSmallPackageSolution.so", 
        "nevermore/u/a7/nevermore")
