#include <re2/re2.h>
int main(void){re2::RE2 re("hello"); return re.ok() ? 0 : 1;}
