import urllib
assert "Hello World!" ==  urllib.urlopen('http://localhost:8080').read()
assert "About Flask example." ==  urllib.urlopen('http://localhost:8080/about').read()
assert 404 == urllib.urlopen('http://localhost:8080/list').getcode()
