import os
import sys
import argparse
from pathlib import Path
import importlib


def list_modules(module_path: Path, package_path: Path):
    package_name = ('.'+'.'.join(str(module_path).split('/'))) if module_path is not None else '.'
    package_path = package_path.absolute()
    sys.path.append(str(package_path.parent))

    # init_file = package_path/package_name.lstrip('.') / '__init__.py'
    # if not init_file.exists():
    #     _packagename = init_file.parent.stem
    #     with init_file.open('w') as file:
    #         file.write(f"from .{_packagename} import *\n")
    #     print(f"{str(package_path.stem)}.{_packagename}.__init__.py created with content: `from .{_packagename} import *`")

    module_path = os.path.dirname(importlib.import_module(
        package_name, str(package_path.stem)).__file__)
    modules = []
    for root, dirs, files in os.walk(module_path, topdown=True):
        for file in files:
            dirs[:] = [d for d in dirs if d not in ["build", "__pycache__"]] # exclude `build` directories, to avoid repeating .so files.
            if file.endswith(".so"):
                file = file.split('.')[0]
                module_name = os.path.splitext(os.path.relpath(os.path.join(
                    root, file), module_path))[0].replace(os.path.sep, ".")
                module_name = f"{package_name}.{module_name}" if package_name != '.' else f".{module_name}"
                module = importlib.import_module(
                    module_name, str(package_path.stem))
                module_name = f"{str(package_path.stem)}{module_name}"
                modules.append((module, module_name, root))
    return modules

def get_module(package_name: Path, package_path: Path):
    package_name = str(package_name)
    package_path = package_path.absolute()
    path =  str(package_path)
    if sys.path[0] != path:
        sys.path.insert(0, str(path))
    module = importlib.import_module(package_name)
    return module, module.__name__, package_path


def stub(module_name: str, module_path: str):
    import logging
    from pybind11_stubgen import CLIArgs, stub_parser_from_args, Printer, to_output_and_subdir, run, Writer
    logging.basicConfig(
        level=logging.INFO,
        format="%(name)s - [%(levelname)7s] %(message)s",
    )
    args = CLIArgs(
        module_name=module_name,
        output_dir= module_path, #'../',
        stub_extension="pyi",
        # default ags:
        root_suffix=None,
        ignore_all_errors=False,
        ignore_invalid_identifiers=None,
        ignore_invalid_expressions=None,
        ignore_unresolved_names=None,
        exit_code=False,
        numpy_array_wrap_with_annotated=False,
        numpy_array_use_type_var=False,
        numpy_array_remove_parameters=False,
        enum_class_locations=[],
        print_safe_value_reprs=None,
        print_invalid_expressions_as_is=False,
        dry_run=False)

    parser = stub_parser_from_args(args)
    printer = Printer(invalid_expr_as_ellipses=not args.print_invalid_expressions_as_is)

    out_dir, sub_dir = to_output_and_subdir(
        output_dir=args.output_dir,
        module_name=args.module_name,
        root_suffix=args.root_suffix,
    )

    run(
        parser,
        printer,
        args.module_name,
        out_dir,
        sub_dir=sub_dir,
        dry_run=args.dry_run,
        writer=Writer(stub_ext=args.stub_extension),
    )


parser = argparse.ArgumentParser()
parser.add_argument('-p', help='package path', default=None)
parser.add_argument('--root', help='module path')
parser.add_argument('--single', help='single module', default=False, type=bool)
parser.add_argument('--eval', help='strings to executed by Python before handling', default=None, type=str)

args = parser.parse_args()
p = Path(args.p) if args.p is not None else None
root = Path(args.root)
single = args.single
eval_str = args.eval
sys.path.insert(0, str(root.absolute()))

if eval_str is not None:
    exec(eval_str)

if not single:
    modules = list_modules(p, root)
    for _module, module_name, _module_path in modules:
        module_name: str
        stub(module_name, str(root.parent))
else:
    _module, module_name, _module_path = get_module(p, root)
    stub(module_name, _module_path)
    