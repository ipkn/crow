#get mime.types file from the nginx repository at nginx/conf/mime.types
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

    with open("mime.types", "r") as mtfile:
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

    with open("mime_types.h", "w") as mtcppfile:
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
