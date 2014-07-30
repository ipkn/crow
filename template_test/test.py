#!/usr/bin/env python
import glob
import json
import os
import subprocess
for testfile in glob.glob("*.json"):
    testdoc = json.load(open(testfile))
    for test in testdoc["tests"]:
        if "partials" in test:
            continue
        if "partial" in test:
            continue
        if "lambda" in test["data"]:
            continue
        print testfile, test["name"]
        print json.dumps(test["data"])
        print test["template"]
        open('data', 'w').write(json.dumps(test["data"]))
        open('template', 'w').write(test["template"])
        ret = subprocess.check_output("./mustachetest")
        if ret != test["expected"]:
            print 'Expected:',(test["expected"])
            print 'Actual:',(ret)
        os.unlink('data')
        os.unlink('template')
        assert ret == test["expected"]
