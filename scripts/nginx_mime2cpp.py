def main():
    mtfile = open("mime.types", "r")
    mtcppfile = open("mime_types.h", "w")
    mtcppfile.write("//This file is generated from /etc/nginx/mime.types using nginx_mime2cpp.py\n")
    mtcppfile.write("#include <map>\n#include <string>\n\nnamespace crow{\nstd::map<std::string, std::string> mime_types {\n")
    lines = mtfile.readlines()
    i=0
    while i < len(lines):
        line = lines[i]
        if ('{' not in line and '}' not in line):
            pline = line.split()
            if (';' not in line):
                pline.extend(lines[i+1].split())
                i += 1
            mtcppfile.write(mime_line_to_cpp(pline))
        i += 1
    mtfile.close()
    mtcppfile.seek(mtcppfile.tell()-2)
    mtcppfile.write("\n};\n}")
    mtcppfile.close()

def mime_line_to_cpp(mlist):
    print("recieved: " + str(mlist))
    stringReturn = ""
    for i in range (len(mlist)):
        if (mlist[i].endswith(";")):
            mlist[i] = mlist[i][:-1]
    for i in range (len(mlist)-1, 0, -1):
        stringReturn += "{\"" + mlist[i] + "\", \"" + mlist[0] + "\"},\n"
    print("created: " + stringReturn)
    return stringReturn

main()
