import os
import json
import platform


def write_ccdb(funUnitGenerator):
    with open('compile_commands.json', 'w') as f:
        f.write('[')
        first = True
        for unit in funUnitGenerator():
            if not first:
                f.write(',')
            first = False
            f.write(f'{{"directory": "{unit[2]}", "arguments": ')
            json.dump(unit[1], f)
            f.write(f', "file": "{unit[0]}" }}')
        f.write(']')


def gen_from_buildbat():
    stuff = {}
    cwd = os.getcwd().replace('\\', '\\\\')
    for line in open('build.bat'):
        line = line.strip()
        if line.startswith('set '):
            lhs, value = line.split('=')
            set, key = lhs.split(' ')
            value = value.split(' ')
            stuff[key] = value

    for src in stuff['SOURCES']:
        yield (src, ['cl'] + stuff['CXXFLAGS'] + stuff['SOURCES'] + stuff['LDFLAGS'], cwd)


if platform.system() == 'Windows':
    write_ccdb(gen_from_buildbat)
