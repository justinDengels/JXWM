#include "jxwm.hpp"
#include <memory>

int main()
{
    std::unique_ptr<JXWM> wm = std::make_unique<JXWM>();

    int res = wm->Init();

    if (res != 0) { return res; }

    wm->Run();

    return 0;
}
