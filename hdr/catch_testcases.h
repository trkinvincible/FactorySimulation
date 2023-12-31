#pragma once

// validate output
#define CATCH_CONFIG_MAIN
#include "../hdr/catch.hpp"

std::unique_ptr<Production> Test_Function(std::size_t runTime)
{
    std::unique_ptr<Production> p = std::make_unique<Production>();
    try{
        p->Start(runTime);
    }catch(std::exception& ex){
        std::cout << "ex: " << ex.what() << std::endl;
    }

    return p;
}

TEST_CASE("Validate Output")
{
    const auto& p = Test_Function(10);

    std::cout << "Workers owning unfinished products: " << p->getNoOfWorkersWithUnfinishedProducts() << std::endl;
    std::cout << "No. of empty feeds: " << p->getm_noOfEmptyFeed() << std::endl;
    std::cout << "No. of products formed: " << p->getm_noOfProductsFormed() << std::endl;
    std::cout << "No. of components left unhandled: " << p->getm_noOfComponentsUnHandled() << std::endl;

    bool test = (6 == p->getNoOfWorkersWithUnfinishedProducts() &&
                 0 == p->getm_noOfEmptyFeed() &&
                 0 == p->getm_noOfProductsFormed() &&
                 1 == p->getm_noOfComponentsUnHandled());

    REQUIRE(test == true);
}
