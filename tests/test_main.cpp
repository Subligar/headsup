#include "test.h"

#include <clocale>
#include <exception>

int main()
{
    // The environment's locale for every C call, as gdifonttexture sets the game's: LC_ALL=de_DE.UTF-8 runs the
    // tests where decimals take a comma.
    std::setlocale(LC_ALL, "");
    int failed = 0;
    for (const test::Case& c : test::Registry())
    {
        try
        {
            c.fn();
            std::printf("PASS %s\n", c.name);
        }
        catch (const test::Failure& f)
        {
            ++failed;
            std::printf("FAIL %s\n     %s\n", c.name, f.message.c_str());
        }
        catch (const std::exception& e)
        {
            ++failed;
            std::printf("FAIL %s\n     threw %s\n", c.name, e.what());
        }
        catch (...)
        {
            ++failed;
            std::printf("FAIL %s\n     threw something that is not a std::exception\n", c.name);
        }
        std::fflush(stdout);
    }
    std::printf("\n%zu tests, %d failed\n", test::Registry().size(), failed);
    return failed == 0 ? 0 : 1;
}
