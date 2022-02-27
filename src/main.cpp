
#include "../hdr/production.h"

#include <memory>

int main(int argc, char *argv[])
{    
    std::unique_ptr<Production> p = std::make_unique<Production>();
    p->Start(100);
    return 0;
}
