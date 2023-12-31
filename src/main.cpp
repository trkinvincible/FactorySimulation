#include <iostream>
#include <memory>

#ifndef RUN_CATCH

#include "../hdr/production.h"

int main(int argc, char *argv[])
{    
    std::unique_ptr<Production> p = std::make_unique<Production>();
    const int runTime = 100;
    try{
        p->Start(runTime);
    }catch(std::exception& ex){
        std::cout << "ex: " << ex.what() << std::endl;
    }

    std::cout << "Workers owning unfinished products: " << p->getNoOfWorkersWithUnfinishedProducts() << std::endl;
    std::cout << "No. of empty feeds: " << p->getm_noOfEmptyFeed() << std::endl;
    std::cout << "No. of products formed: " << p->getm_noOfProductsFormed() << std::endl;
    std::cout << "No. of components left unhandled: " << p->getm_noOfComponentsUnHandled() << std::endl;

    return 0;
}
#else

#include "../hdr/production.h"
#include "../hdr/catch_testcases.h"

#endif
