from flask import Flask
app = Flask(__name__)

@app.route("/")
def hello():
    return "Hello World!"

@app.route("/about/<path:path>/hello")
def hello1(path):
    return "about1"

@app.route("/about")
def hello2():
    return "about2"

print app.url_map

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8888)
