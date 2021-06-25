import urllib
assert "Hello World!" ==  urllib.urlopen('http://localhost:18080').read()
assert "About Crow example." ==  urllib.urlopen('http://localhost:18080/about').read()
assert 404 == urllib.urlopen('http://localhost:18080/list').getcode()
assert "3 bottles of beer!" == urllib.urlopen('http://localhost:18080/hello/3').read()
assert "100 bottles of beer!" == urllib.urlopen('http://localhost:18080/hello/100').read()
assert 400 == urllib.urlopen('http://localhost:18080/hello/500').getcode()
assert "3" == urllib.urlopen('http://localhost:18080/add_json', data='{"a":1,"b":2}').read()
assert "3" == urllib.urlopen('http://localhost:18080/add/1/2').read()

# test persistent connection
import socket
import time
s = socket.socket()
s.connect(('localhost', 18080))
for i in xrange(10):
    s.send('''GET / HTTP/1.1
Host: localhost\r\n\r\n''');
    assert 'Hello World!' in s.recv(1024)

# test large
s = socket.socket()
s.connect(('localhost', 18080))
s.send('''GET /large HTTP/1.1
Host: localhost\r\nConnection: close\r\n\r\n''')
r = ''
while True:
     d = s.recv(1024*1024)
     if not d:
         break;
     r += d
     print len(r), len(d)
print len(r), r[:100]
assert len(r) > 512*1024

# test timeout
s = socket.socket()
s.connect(('localhost', 18080))
# invalid request, connection will be closed after timeout
s.send('''GET / HTTP/1.1
hHhHHefhwjkefhklwejfklwejf
''')
print s.recv(1024)

