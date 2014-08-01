#!/usr/bin/env python
import glob
import json
import os
import subprocess
for testfile in glob.glob("*.json"):
    testdoc = json.load(open(testfile))
    for test in testdoc["tests"]:
        if "lambda" in test["data"]:
            continue
        if "partials" in test:
            #print testfile, test["name"]
            continue
        open('data', 'w').write(json.dumps(test["data"]))
        open('template', 'w').write(test["template"])
        ret = subprocess.check_output("./mustachetest")
        print testfile, test["name"]
        if ret != test["expected"]:
            print json.dumps(test["data"])
            print test["template"]
            print 'Expected:',repr(test["expected"])
            print 'Actual:',repr(ret)
        assert ret == test["expected"]
        os.unlink('data')
        os.unlink('template')
