import glob
import re
from collections import defaultdict
OUTPUT = 'crow_all.h'
re_depends = re.compile('^#include "(.*)"', re.MULTILINE)
headers = [x.rsplit('/',1)[-1] for x in glob.glob('../include/*.h')]
print headers
edges = defaultdict(list)
for header in headers:
    d = open("../include/" + header).read().decode('utf8')
    match = re_depends.findall(d)
    for m in match:
        # m should included before header
        edges[m].append(header)

visited = defaultdict(bool)
order = []
def dfs(x):
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
    print x, edges[x]
for x in edges:
    for y in edges[x]:
        assert order.index(x) < order.index(y), 'cyclic include detected'

print order
build = []
for header in order:
    d = open("../include/" + header).read().decode('utf8')
    build.append(re_depends.sub(lambda x:'\n', d))
    build.append('\n')

open(OUTPUT,'w').write('\n'.join(build))

