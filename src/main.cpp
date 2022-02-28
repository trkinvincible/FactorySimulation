#include <iostream>
#include <memory>

#define TEST 1
#include "../hdr/production.h"

int main(int argc, char *argv[])
{    
    std::unique_ptr<Production> p = std::make_unique<Production>();
#if TEST
    const int runTime = 10;
#else
    const int runTime = 100;
#endif
    try{
        p->Start(runTime);
    }catch(std::exception& ex){
        std::cout << "ex: " << ex.what() << std::endl;
    }

    std::cout << "Workers owning unfinished products: " << p->getNoOfWorkersWithUnfinishedProducts() << std::endl;
    std::cout << "No. of empty feeds: " << p->getm_noOfEmptyFeed() << std::endl;
    std::cout << "No. of products formed: " << p->getm_noOfProductsFormed() << std::endl;
    std::cout << "No. of components left unhandled: " << p->getm_noOfComponentsUnHandled() << std::endl;

    // Unit Test.
#if TEST
    if (runTime == 10){
        bool test = (6 == p->getNoOfWorkersWithUnfinishedProducts() &&
                0 == p->getm_noOfEmptyFeed() &&
                0 == p->getm_noOfProductsFormed() &&
                1 == p->getm_noOfComponentsUnHandled());
    std::cout << "Test Result: " << (test ? "PASS" : "FAIL") << std::endl;
    }
#endif
    return 0;
}
