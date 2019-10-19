# Please install wheezy.template:
#     py -3 -m pip install wheezy.template
# Usage:
#     py -3 tgen.py -i <template_dir> -o <out_dir> -n <name>
#     py -3 tgen.py --options <option file>
# *.template files in template directory are processed as templates, otherwise copied
# map.json file contains renaming rules

import os, json, shutil

from wheezy.template.engine   import Engine
from wheezy.template.ext.core import CoreExtension
from wheezy.template.ext.code import CodeExtension
from wheezy.template.loader   import FileLoader

import sys, getopt

# TODO: add directory traversal, recreation and optional renaming + automatic output folder creation
def main(argv):
    done = True
    try:
        opts, args = getopt.getopt(argv, "i:o:n:")
        opts = dict(opts)
        template_dir = os.path.abspath(opts["-i"])
        out_dir = os.path.abspath(opts["-o"])
        name = opts["-n"]
    except Exception:
        done = False

    if not done:
        done = True
        try:
            opts, args = getopt.getopt(argv, "", ["options="])
            fileName = opts[0][1]
            with open(fileName, 'r') as f:
                options = json.load(f)

            template_dir = os.path.abspath(options["template_dir"])
            out_dir = os.path.abspath(options["out_dir"])
            name = options["name"]
        except Exception:
            done = False

    if (argv is None or len(argv) == 0 or not done):
        print("py -3 tgen.py -i <template_dir> -o <out_dir> -n <name>\npy -3 tgen.py --options <option file>")
        sys.exit(2)

    if not os.path.isdir(template_dir):
        print('%s is not a directory' % template_dir)
        exit(1)

    engine = Engine(loader=FileLoader([template_dir]),extensions=[CoreExtension(),CodeExtension()])

    map_filename = os.path.join(template_dir, "map.json")
    with open(map_filename, 'r') as f:
        file_data = json.load(f)

    for root, _, files in os.walk(template_dir):
        for f in files:
            if f == "map.json":
                continue

            src_path = os.path.join(root, f)
            dst_dir = os.path.join(out_dir, os.path.relpath(root, template_dir))

            fileName, ext = os.path.splitext(f)

            if ext != ".template":
                fileName = f

            if (f in file_data):
                dst_path = os.path.join(dst_dir, file_data[f].format(name=name))
            else:
                dst_path = os.path.join(dst_dir, fileName)

            if not os.path.exists(dst_dir):
                os.makedirs(dst_dir)

            if (ext == '.template'):
                print("Generating {1} from {0}".format(src_path, dst_path))
                template = engine.get_template(src_path)
                with open(dst_path, 'wb') as out:
                    out.write(template.render({'name': name}).encode('utf-8'))
            else:
                print("Copying {} -> {}".format(src_path, dst_path))
                shutil.copyfile(src_path, dst_path)
    return

    for source, dest in file_data.items():
        _, ext = os.path.splitext(source)
        src_path = os.path.join(template_dir, source)
        dst_path = os.path.join(out_dir, dest.format(name=name))
        if (ext == '.template'):
            print("Generating {1} from {0}".format(src_path, dst_path))
            template = engine.get_template(src_path)
            with open(dst_path, 'wb') as out:
                out.write(template.render({'name': name}).encode('utf-8'))
        else:
            print("Copying {} -> {}".format(src_path, dst_path))
            shutil.copyfile(src_path, dst_path)

if __name__ == "__main__":
   main(sys.argv[1:])