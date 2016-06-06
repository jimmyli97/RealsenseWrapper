#include "rs_wrapper.h"
#include <iostream>

int addTwo(int a, int b) {
    return a+b;
}

int main(void) {
    int result = addTwo(1,2);
    std::cout << result;
    return result;
}
