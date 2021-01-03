import os
import shutil
import re
import sys


def replace_in_file(filename, mapping):
    print('==> Editing '+filename)
    infile = open(filename)
    outfile = open(filename+'.tmp', 'w+')
    for line in infile:
        for k, v in mapping.items():
            line = line.replace(k, v)
        outfile.write(line)
    infile.close()
    outfile.close()
    shutil.copyfile(filename+'.tmp', filename)
    os.remove(filename+'.tmp')

def list_files_to_edit(root, extensions,
        exclude_directories=[],
        exclude_files=[]):
    result = []
    for dirname, dirnames, filenames in os.walk(root):
        for filename in filenames:
            if filename in exclude_files:
                continue
            for ext in extensions:
                if filename.endswith(ext):
                    result.append(os.path.join(dirname, filename))
                    break
        for d in exclude_directories:
            if d in dirnames:
                dirnames.remove(d)
    return result

def rename_files_and_directories(root, extensions,
        mapping,
        exclude_directories=[],
        exclude_files=[]):
    for dirname, dirnames, filenames in os.walk(root):
        # exclude directories
        for d in exclude_directories:
            if d in dirnames:
                dirnames.remove(d)
        # rename folders
        names_to_changes = {}
        for subdirname in dirnames:
            new_name = subdirname
            for k, v in mapping.items():
                new_name = new_name.replace(k, v)
            if new_name != subdirname:
                print("==> Renaming "+os.path.join(dirname, subdirname)+" into "+os.path.join(dirname, new_name))
                shutil.move(os.path.join(dirname, subdirname),
                            os.path.join(dirname, new_name))
                dirnames.remove(subdirname)
                dirnames.append(new_name)
        # rename files
        for filename in filenames:
            if filename in exclude_files:
                continue
            for ext in extensions:
                if filename.endswith(ext):
                    new_name = filename
                    for  k, v in mapping.items():
                        new_name = new_name.replace(k, v)
                    if new_name != filename:
                        print("==> Renaming "+os.path.join(dirname, filename)+" into "+os.path.join(dirname, new_name))
                        os.rename(os.path.join(dirname, filename),
                                  os.path.join(dirname, new_name))
                    break # don't try the next extension for this file


if __name__ == '__main__':
    service_name = input('Enter the name of your service: ')
    if(not re.match('[a-zA-Z_][a-zA-Z\d_]*', service_name)):
        print("Error: service name must start with a letter and consist of letters, digits, or underscores")
        sys.exit(-1)
    service_name = service_name.rstrip()
    resource_name = input('Enter the name of the resources (e.g., database): ')
    if(not re.match('[a-zA-Z_][a-zA-Z\d_]*', resource_name)):
        print("Error: resource name must start with a letter and consist of letters, digits, or underscores")
        sys.exit(-1)
    mapping = {
        'alpha' : service_name,
        'ALPHA' : service_name.upper(),
        'resource' : resource_name,
        'RESOURCE' : resource_name.upper()
    }
    files_to_edit = list_files_to_edit('.',
        extensions=['.c', '.h', '.txt', '.in'],
        exclude_directories=['.git','build', '.spack-env', 'munit'],
        exclude_files=['uthash.h'])
    for f in files_to_edit:
        replace_in_file(f, mapping)
    rename_files_and_directories('.',
        extensions=['.c', '.h', '.txt', '.in'],
        mapping=mapping,
        exclude_directories=['.git','build', '.spack-env', 'munit'],
        exclude_files=['uthash.h'])
