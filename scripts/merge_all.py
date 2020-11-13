#!/usr/bin/env python3

"""Merges all the header files."""
from glob import glob
from os import path as pt, name as osname
from os.path import sep
import re
from collections import defaultdict
import sys, getopt

if len(sys.argv) < 3:
    print("Usage: {} <CROW_HEADERS_DIRECTORY_PATH> <CROW_OUTPUT_HEADER_PATH> (-i(include) OR -e(exclude) item1,item2...)".format(sys.argv[0]))
    print("Available middlewares are in `include/crow/middlewares`. Do NOT type the `.h` when including or excluding")
    sys.exit(1)

header_path = sys.argv[1]
output_path = sys.argv[2]

middlewares = [x.rsplit(sep, 1)[-1][:-2] for x in glob(pt.join(header_path, ('crow'+sep+'middlewares'+sep+'*.h*')))]


middlewares_actual = []
if len(sys.argv) > 3:
    opts, args = getopt.getopt(sys.argv[3:],"i:e:",["include=","exclude="])
    if (len(opts) > 1):
        print("Error:Cannot simultaneously include and exclude middlewares.")
        sys.exit(1)
    if (opts[0][0] in ("-i", "--include")):
        middleware_list = opts[0][1].split(',')
        for item in middlewares:
            if (item in middleware_list):
                middlewares_actual.append(item)
    elif (opts[0][0] in ("-e", "--exclude")):
        middleware_list = opts[0][1].split(',')
        for item in middlewares:
            if (item not in middleware_list):
                middlewares_actual.append(item)
    else:
        print("ERROR:Unknown argument " + opts[0][0])
        sys.exit(1)
else:
    middlewares_actual = middlewares
print("Middlewares: " + str(middlewares_actual))

re_depends = re.compile('^#include "(.*)"', re.MULTILINE)
headers = [x.rsplit(sep, 1)[-1] for x in glob(pt.join(header_path, '*.h*'))]
headers += ['crow'+sep + x.rsplit(sep, 1)[-1] for x in glob(pt.join(header_path, 'crow'+sep+'*.h*'))]
headers += [('crow'+sep+'middlewares'+sep + x + '.h') for x in middlewares_actual]
print(headers)
edges = defaultdict(list)
for header in headers:
    d = open(pt.join(header_path, header)).read()
    match = re_depends.findall(d)
    for m in match:
        actual_m = m
        if (osname == 'nt'): #Windows
            actual_m = m.replace('/', '\\')
        # m should included before header
        edges[actual_m].append(header)

visited = defaultdict(bool)
order = []



def dfs(x):
    """Ensure all header files are visited."""
    visited[x] = True
    for y in edges[x]:
        if not visited[y]:
            dfs(y)
    order.append(x)

for header in headers:
    if not visited[header]:
        dfs(header)

order = order[::-1]
for x in edges:
    print(x, edges[x])
for x in edges:
    for y in edges[x]:
        assert order.index(x) < order.index(y), 'cyclic include detected'

print(order)
build = []
for header in order:
    d = open(pt.join(header_path, header)).read()
    build.append(re_depends.sub(lambda x: '\n', d))
    build.append('\n')

open(output_path, 'w').write('\n'.join(build))
