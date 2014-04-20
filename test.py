import urllib
assert "Hello World!" ==  urllib.urlopen('http://localhost:18080').read()
assert "About Flask example." ==  urllib.urlopen('http://localhost:18080/about').read()
assert 404 == urllib.urlopen('http://localhost:18080/list').getcode()
assert "3 bottles of beer!" == urllib.urlopen('http://localhost:18080/hello/3').read()
assert "100 bottles of beer!" == urllib.urlopen('http://localhost:18080/hello/100').read()
assert 400 == urllib.urlopen('http://localhost:18080/hello/500').getcode()
assert "3" == urllib.urlopen('http://localhost:18080/add_json', data='{"a":1,"b":2}').read()
