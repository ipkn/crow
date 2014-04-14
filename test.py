import urllib
assert "Hello World!" ==  urllib.urlopen('http://localhost:8080').read()
assert "About Flask example." ==  urllib.urlopen('http://localhost:8080/about').read()
assert 404 == urllib.urlopen('http://localhost:8080/list').getcode()
assert "3 bottles of beer!" == urllib.urlopen('http://localhost:8080/hello/3').read()
assert "100 bottles of beer!" == urllib.urlopen('http://localhost:8080/hello/100').read()
assert "" == urllib.urlopen('http://localhost:8080/hello/500').read()
assert 400 == urllib.urlopen('http://localhost:8080/hello/500').getcode()
