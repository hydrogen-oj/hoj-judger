#include <iostream>
#include <thread>

using namespace std;

int main() {
    long long a, b;
    cin >> a >> b;
    cout << a + b << endl;
    this_thread::sleep_for(1.5s);
    return 0;
}
