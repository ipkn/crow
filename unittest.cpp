#include <iostream>
using namespace std;

int main()
{
    //cout << strtol("+123999999999999999999", NULL, 10) <<endl;
    //cout <<errno <<endl;
    cout << strtol("+9223372036854775807", NULL, 10) <<endl;
    cout <<errno <<endl;
}
