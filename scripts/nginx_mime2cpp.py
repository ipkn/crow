#get mime.types file from the nginx repository at nginx/conf/mime.types
#typical output filename: mime_types.h
import sys

if len(sys.argv) != 3:
    print("Usage: {} <NGINX_MIME_TYPE_FILE_PATH> <CROW_OUTPUT_HEADER_PATH>".format(sys.argv[0]))
    sys.exit(1)

file_path = sys.argv[1]
output_path = sys.argv[2]

tabspace = "    "
def main():
    outLines = []
    outLines.append("//This file is generated from nginx/conf/mime.types using nginx_mime2cpp.py")
    outLines.extend([
        "#include <unordered_map>",
        "#include <string>",
        "",
        "namespace crow {",
        tabspace + "std::unordered_map<std::string, std::string> mime_types {"])

    with open(file_path, "r") as mtfile:
        incomplete = ""
        incompleteExists = False
        for line in mtfile:
            if ('{' not in line and '}' not in line):
                splitLine = line.split()
                if (';' not in line):
                    incomplete += line
                    incompleteExists = True
                    continue
                elif (incompleteExists):
                    splitLine = incomplete.split()
                    splitLine.extend(line.split())
                    incomplete = ""
                    incompleteExists = False
                outLines.extend(mime_line_to_cpp(splitLine))

    outLines[-1] = outLines[-1][:-1]
    outLines.extend([tabspace + "};", "}"])

    with open(output_path, "w") as mtcppfile:
        mtcppfile.writelines(x + '\n' for x in outLines)

def mime_line_to_cpp(mlist):
    #print("recieved: " + str(mlist))
    stringReturn = []
    for i in range (len(mlist)):
        if (mlist[i].endswith(";")):
            mlist[i] = mlist[i][:-1]
    for i in range (len(mlist)-1, 0, -1):
        stringReturn.append(tabspace*2 + "{\"" + mlist[i] + "\", \"" + mlist[0] + "\"},")
    #print("created: " + stringReturn)
    return stringReturn

main()
